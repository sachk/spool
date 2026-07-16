#include "NativeAppWindow.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QExposeEvent>
#include <QGuiApplication>
#include <QMetaObject>
#include <QMutexLocker>
#include <QResizeEvent>
#include <QThread>

#include <qpa/qplatformnativeinterface.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cstring>

extern "C" {
#include "video/out/starfish/starfish_ctx.h"
}

namespace JellyfinNative {

namespace {

    enum class OverlayBufferKind { Image, Wayland };

    struct OverlayBuffer {
        OverlayBufferKind kind = OverlayBufferKind::Image;
        QImage image;
        int x = 0;
        int y = 0;
        wl_shm_pool *pool = nullptr;
        wl_buffer *waylandBuffer = nullptr;
        uint8_t *pixels = nullptr;
        size_t size = 0;
    };

    void destroyOverlayBuffer(OverlayBuffer *buffer)
    {
        if (!buffer)
            return;
        if (buffer->waylandBuffer)
            wl_buffer_destroy(buffer->waylandBuffer);
        if (buffer->pool)
            wl_shm_pool_destroy(buffer->pool);
        if (buffer->pixels)
            munmap(buffer->pixels, buffer->size);
        delete buffer;
    }

    void overlayBufferReleased(void *data, wl_buffer *)
    {
        destroyOverlayBuffer(static_cast<OverlayBuffer *>(data));
    }

    constexpr wl_buffer_listener kOverlayBufferListener = { overlayBufferReleased };

    int createOverlayFd(size_t size)
    {
        int fd = -1;
#ifdef SYS_memfd_create
        fd = static_cast<int>(syscall(SYS_memfd_create, "tern-mpv-osd", 0x0001U));
#endif
        if (fd < 0) {
            char path[] = "/tmp/tern-mpv-osd-XXXXXX";
            fd = mkstemp(path);
            if (fd >= 0) {
                unlink(path);
                fcntl(fd, F_SETFD, FD_CLOEXEC);
            }
        }
        if (fd >= 0 && ftruncate(fd, static_cast<off_t>(size)) < 0) {
            ::close(fd);
            fd = -1;
        }
        return fd;
    }

} // namespace

NativeAppWindow::NativeAppWindow(const QString& appId, QWindow *parent)
    : QQuickView(parent)
    , m_appId(appId)
{
    setColor(Qt::transparent);
    setResizeMode(QQuickView::SizeRootObjectToView);
    setFlags(Qt::FramelessWindowHint | Qt::Window);
    setTitle(QStringLiteral("Jellyfin Native"));
    resize(1920, 1080);
    // Note: cursor suppression on webOS happens in main.cpp via
    // XCURSOR_PATH=/dev/null. Setting Qt::BlankCursor here would make Qt
    // call wl_pointer_set_cursor(NULL), which the LSM honours as "hide
    // cursor" — that hides the LG remote pointer too. Forcing the cursor
    // theme load to fail makes Qt early-return from updateCursor and
    // never touch the cursor, leaving the LSM pointer untouched.
    starfish_overlay_set_callbacks(
        &NativeAppWindow::overlayAcquireCallback, &NativeAppWindow::overlayPresentCallback, this);
    starfish_exported_set_crop_cb(&NativeAppWindow::exportedCropCallback, this);
}

NativeAppWindow::~NativeAppWindow()
{
    starfish_exported_set_crop_cb(nullptr, nullptr);
    starfish_overlay_set_callbacks(nullptr, nullptr, nullptr);
    if (m_overlaySubsurface)
        wl_subsurface_destroy(m_overlaySubsurface);
    if (m_overlaySurface)
        wl_surface_destroy(m_overlaySurface);
    if (m_exported)
        wl_webos_exported_destroy(m_exported);
    if (m_webosShellSurface)
        wl_webos_shell_surface_destroy(m_webosShellSurface);
    if (m_webosShell)
        wl_webos_shell_destroy(m_webosShell);
    if (m_webosForeign)
        wl_webos_foreign_destroy(m_webosForeign);
    if (m_shm)
        wl_shm_destroy(m_shm);
    if (m_registry)
        wl_registry_destroy(m_registry);
}

bool NativeAppWindow::prepareForUiSurface()
{
    showFullScreen();
    requestActivate();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    // Do not block app startup for seconds waiting on the shell surface.
    // The login UI can appear first; playback still performs a stricter wait
    // later when the video export surface is actually needed.
    if (ensureShellSurface())
        return true;

    return handle() != nullptr;
}

void NativeAppWindow::bringToFront()
{
    // showFullScreen() is a Qt-level call that flips the Qt window state
    // and ensures we have a shell surface; on its own it does NOT bring
    // the surface to the foreground on webOS — the LSM only honours
    // wl_webos_shell_surface_set_state(FULLSCREEN). xbmc does the same
    // thing on lifecycle relaunch (see CShellSurfaceWebOSShell::SetFullScreen).
    if (!isVisible())
        showFullScreen();
    if (!ensureShellSurface())
        return;
    wl_webos_shell_surface_set_state(m_webosShellSurface, WL_WEBOS_SHELL_SURFACE_STATE_FULLSCREEN);
    if (m_surface)
        wl_surface_commit(m_surface);
    if (m_display)
        wl_display_flush(m_display);
}

void NativeAppWindow::toggleFullScreen()
{
    if (!fullScreen())
        showFullScreen();
    emit fullScreenChanged();
}

bool NativeAppWindow::prepareForPlaybackSurface()
{
    if (!prepareForUiSurface())
        return false;

    const int timeoutMs = 5000;
    const int stepMs = 25;
    for (int waited = 0; waited < timeoutMs; waited += stepMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, stepMs);
        if (ensureVideoSurface() && !m_windowId.empty())
            break;
        QThread::msleep(stepMs);
    }

    if (m_windowId.empty())
        return false;

    return true;
}

QString NativeAppWindow::windowId() const
{
    return QString::fromStdString(m_windowId);
}

void NativeAppWindow::exposeEvent(QExposeEvent *event)
{
    QQuickView::exposeEvent(event);
    if (isExposed())
        ensureShellSurface();
}

void NativeAppWindow::resizeEvent(QResizeEvent *event)
{
    QQuickView::resizeEvent(event);
    if (!m_windowId.empty()) {
        updateCropRegion();
    }
}

bool NativeAppWindow::ensureVideoSurface()
{
    if (m_exported || !handle())
        return true;

    if (!ensureShellSurface())
        return false;

    if (!m_webosForeign || !m_surface)
        return false;

    m_exported
        = wl_webos_foreign_export_element(m_webosForeign, m_surface, WL_WEBOS_FOREIGN_WEBOS_EXPORTED_TYPE_VIDEO_OBJECT);
    if (!m_exported)
        return false;

    wl_webos_exported_add_listener(m_exported, &s_exportedListener, this);
    wl_surface_commit(m_surface);
    wl_display_roundtrip(m_display);
    updateCropRegion();
    return true;
}

bool NativeAppWindow::ensureShellSurface()
{
    if (m_webosShellSurface && m_surface) {
        ensureOverlaySurface();
        return true;
    }

    if (!handle())
        return false;

    auto *native = QGuiApplication::platformNativeInterface();
    if (!native)
        return false;

    m_display = static_cast<wl_display *>(native->nativeResourceForIntegration(QByteArrayLiteral("display")));
    m_surface = static_cast<wl_surface *>(native->nativeResourceForWindow(QByteArrayLiteral("surface"), this));
    if (!m_display || !m_surface)
        return false;

    if (!bindGlobals())
        return false;

    if (!m_webosShell || !m_surface)
        return false;

    m_webosShellSurface = wl_webos_shell_get_shell_surface(m_webosShell, m_surface);
    if (!m_webosShellSurface)
        return false;

    wl_webos_shell_surface_set_property(m_webosShellSurface, "_WEBOS_ACCESS_POLICY_KEYS_GUIDE", "true");
    wl_webos_shell_surface_set_property(m_webosShellSurface, "_WEBOS_ACCESS_POLICY_KEYS_BACK", "true");
    // The string access-policy properties predate set_key_mask(). Some LSM
    // versions only apply them opportunistically during the launch handoff,
    // leaving Back owned by the compositor even though navigation keys reach
    // the client. Keep the properties for old firmware, and use the protocol
    // request as the authoritative declaration where it is supported.
    wl_webos_shell_surface_set_key_mask(
        m_webosShellSurface, WL_WEBOS_SHELL_SURFACE_WEBOS_KEY_DEFAULT | WL_WEBOS_SHELL_SURFACE_WEBOS_KEY_BACK);
    wl_webos_shell_surface_set_property(m_webosShellSurface, "appId", m_appId.toUtf8().constData());
    wl_webos_shell_surface_set_property(
        m_webosShellSurface, "displayAffinity", getenv("DISPLAY_ID") ? getenv("DISPLAY_ID") : "0");
    wl_webos_shell_surface_set_state(m_webosShellSurface, WL_WEBOS_SHELL_SURFACE_STATE_FULLSCREEN);
    wl_surface_commit(m_surface);
    wl_display_roundtrip(m_display);
    ensureOverlaySurface();
    return true;
}

bool NativeAppWindow::ensureOverlaySurface()
{
    if (m_overlaySurface && m_overlaySubsurface)
        return true;
    if (!m_compositor || !m_subcompositor || !m_surface)
        return false;

    m_overlaySurface = wl_compositor_create_surface(m_compositor);
    if (!m_overlaySurface)
        return false;
    m_overlaySubsurface = wl_subcompositor_get_subsurface(m_subcompositor, m_overlaySurface, m_surface);
    if (!m_overlaySubsurface) {
        wl_surface_destroy(m_overlaySurface);
        m_overlaySurface = nullptr;
        return false;
    }

    // OSD is visual only. An empty input region keeps remote and pointer input
    // routed to Qt even though this child surface sits above the UI surface.
    wl_region *emptyInput = wl_compositor_create_region(m_compositor);
    if (emptyInput) {
        wl_surface_set_input_region(m_overlaySurface, emptyInput);
        wl_region_destroy(emptyInput);
    }
    wl_subsurface_set_desync(m_overlaySubsurface);
    wl_subsurface_set_position(m_overlaySubsurface, 0, 0);
    wl_surface_commit(m_overlaySurface);
    wl_display_flush(m_display);
    return true;
}

bool NativeAppWindow::bindGlobals()
{
    if (m_webosForeign && m_compositor && m_subcompositor && m_shm && m_webosShell)
        return true;

    m_registry = wl_display_get_registry(m_display);
    if (!m_registry)
        return false;

    wl_registry_add_listener(m_registry, &s_registryListener, this);
    wl_display_roundtrip(m_display);
    wl_display_roundtrip(m_display);

    return m_compositor && m_subcompositor && m_shm && m_webosForeign && m_webosShell;
}

void NativeAppWindow::updateCropRegion()
{
    if (!m_exported || !m_compositor)
        return;

    const int origW = m_cropOrigW > 0 ? m_cropOrigW : width();
    const int origH = m_cropOrigH > 0 ? m_cropOrigH : height();
    const int srcX = m_cropSrcW > 0 ? m_cropSrcX : 0;
    const int srcY = m_cropSrcH > 0 ? m_cropSrcY : 0;
    const int srcW = m_cropSrcW > 0 ? m_cropSrcW : origW;
    const int srcH = m_cropSrcH > 0 ? m_cropSrcH : origH;
    const int dstX = m_cropDstW > 0 ? m_cropDstX : 0;
    const int dstY = m_cropDstH > 0 ? m_cropDstY : 0;
    const int dstW = m_cropDstW > 0 ? m_cropDstW : width();
    const int dstH = m_cropDstH > 0 ? m_cropDstH : height();

    wl_region *orig = wl_compositor_create_region(m_compositor);
    wl_region *src = wl_compositor_create_region(m_compositor);
    wl_region *dst = wl_compositor_create_region(m_compositor);
    if (!orig || !src || !dst)
        return;

    wl_region_add(orig, 0, 0, origW, origH);
    wl_region_add(src, srcX, srcY, srcW, srcH);
    wl_region_add(dst, dstX, dstY, dstW, dstH);
    wl_webos_exported_set_crop_region(m_exported, orig, src, dst);
    wl_region_destroy(dst);
    wl_region_destroy(src);
    wl_region_destroy(orig);
    wl_display_flush(m_display);
}

void NativeAppWindow::setVideoCrop(
    int origW, int origH, int srcX, int srcY, int srcW, int srcH, int dstX, int dstY, int dstW, int dstH)
{
    if (origW <= 0 || origH <= 0 || srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0)
        return;

    const bool changed = m_cropOrigW != origW || m_cropOrigH != origH || m_cropSrcX != srcX || m_cropSrcY != srcY
        || m_cropSrcW != srcW || m_cropSrcH != srcH || m_cropDstX != dstX || m_cropDstY != dstY || m_cropDstW != dstW
        || m_cropDstH != dstH;
    if (!changed)
        return;

    m_cropOrigW = origW;
    m_cropOrigH = origH;
    m_cropSrcX = srcX;
    m_cropSrcY = srcY;
    m_cropSrcW = srcW;
    m_cropSrcH = srcH;
    m_cropDstX = dstX;
    m_cropDstY = dstY;
    m_cropDstW = dstW;
    m_cropDstH = dstH;
    updateCropRegion();
}

void NativeAppWindow::scheduleVideoCrop(
    int origW, int origH, int srcX, int srcY, int srcW, int srcH, int dstX, int dstY, int dstW, int dstH)
{
    if (origW <= 0 || origH <= 0 || srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0)
        return;

    if (QThread::currentThread() == thread()) {
        setVideoCrop(origW, origH, srcX, srcY, srcW, srcH, dstX, dstY, dstW, dstH);
        return;
    }

    bool shouldQueue = false;
    {
        QMutexLocker locker(&m_cropMutex);
        m_pendingCrop = { origW, origH, srcX, srcY, srcW, srcH, dstX, dstY, dstW, dstH, true };
        if (!m_cropUpdateQueued) {
            m_cropUpdateQueued = true;
            shouldQueue = true;
        }
    }

    if (shouldQueue) {
        QMetaObject::invokeMethod(this, [this]() { publishPendingVideoCrop(); }, Qt::QueuedConnection);
    }
}

void NativeAppWindow::publishPendingVideoCrop()
{
    CropRegion crop;
    {
        QMutexLocker locker(&m_cropMutex);
        crop = m_pendingCrop;
        m_pendingCrop = CropRegion();
        m_cropUpdateQueued = false;
    }
    if (!crop.valid)
        return;

    setVideoCrop(
        crop.origW, crop.origH, crop.srcX, crop.srcY, crop.srcW, crop.srcH, crop.dstX, crop.dstY, crop.dstW, crop.dstH);
}

void NativeAppWindow::scheduleOverlayImage(QImage image, int x, int y)
{
    bool shouldQueue = false;
    {
        QMutexLocker locker(&m_overlayMutex);
        m_pendingOverlayImage = std::move(image);
        m_pendingOverlayX = x;
        m_pendingOverlayY = y;
        if (!m_overlayPublishQueued) {
            m_overlayPublishQueued = true;
            shouldQueue = true;
        }
    }

    if (shouldQueue) {
        QMetaObject::invokeMethod(this, [this]() { publishPendingOverlayImage(); }, Qt::QueuedConnection);
    }
}

void NativeAppWindow::publishPendingOverlayImage()
{
    bool changed = false;
    {
        QMutexLocker locker(&m_overlayMutex);
        QImage image = std::move(m_pendingOverlayImage);
        const int x = m_pendingOverlayX;
        const int y = m_pendingOverlayY;
        m_pendingOverlayImage = QImage();
        m_overlayPublishQueued = false;
        if (image.isNull() && m_overlayImage.isNull())
            return;
        m_overlayImage = std::move(image);
        m_overlayX = x;
        m_overlayY = y;
        m_overlayRevision += 1;
        changed = true;
    }
    if (changed)
        emit overlayRevisionChanged();
}

void NativeAppWindow::registryGlobal(
    void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    auto *self = static_cast<NativeAppWindow *>(data);
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        const uint32_t bindVersion = version < 3 ? version : 3;
        self->m_compositor
            = static_cast<wl_compositor *>(wl_registry_bind(registry, name, &wl_compositor_interface, bindVersion));
    } else if (strcmp(interface, wl_subcompositor_interface.name) == 0) {
        self->m_subcompositor
            = static_cast<wl_subcompositor *>(wl_registry_bind(registry, name, &wl_subcompositor_interface, 1));
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        self->m_shm = static_cast<wl_shm *>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
    } else if (strcmp(interface, wl_webos_shell_interface.name) == 0) {
        const uint32_t bindVersion = version < 2 ? version : 2;
        self->m_webosShell
            = static_cast<wl_webos_shell *>(wl_registry_bind(registry, name, &wl_webos_shell_interface, bindVersion));
    } else if (strcmp(interface, wl_webos_foreign_interface.name) == 0) {
        const uint32_t bindVersion = version < 2 ? version : 2;
        self->m_webosForeign = static_cast<wl_webos_foreign *>(
            wl_registry_bind(registry, name, &wl_webos_foreign_interface, bindVersion));
    }
}

void NativeAppWindow::registryRemove(void *, wl_registry *, uint32_t) { }

void NativeAppWindow::exportedWindowIdAssigned(void *data, wl_webos_exported *, const char *window_id, uint32_t)
{
    auto *self = static_cast<NativeAppWindow *>(data);
    self->m_windowId = window_id ? window_id : "";
}

uint8_t *NativeAppWindow::overlayAcquireCallback(
    void *data, int x, int y, int width, int height, int *stride, void **buffer)
{
    auto *self = static_cast<NativeAppWindow *>(data);
    if (!self || !stride || !buffer || width <= 0 || height <= 0)
        return nullptr;

    if (self->m_shm && self->m_overlaySurface) {
        const int waylandStride = width * 4;
        const size_t size = static_cast<size_t>(waylandStride) * static_cast<size_t>(height);
        const int fd = createOverlayFd(size);
        if (fd >= 0) {
            auto *frame = new OverlayBuffer;
            frame->kind = OverlayBufferKind::Wayland;
            frame->x = x;
            frame->y = y;
            frame->size = size;
            frame->pixels = static_cast<uint8_t *>(mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
            if (frame->pixels != MAP_FAILED) {
                frame->pool = wl_shm_create_pool(self->m_shm, fd, static_cast<int>(size));
                if (frame->pool) {
                    frame->waylandBuffer = wl_shm_pool_create_buffer(
                        frame->pool, 0, width, height, waylandStride, WL_SHM_FORMAT_ARGB8888);
                }
            } else {
                frame->pixels = nullptr;
            }
            ::close(fd);
            if (frame->waylandBuffer) {
                wl_buffer_add_listener(frame->waylandBuffer, &kOverlayBufferListener, frame);
                *stride = waylandStride;
                *buffer = frame;
                return frame->pixels;
            }
            destroyOverlayBuffer(frame);
        }
    }

    // Fallback for compositors where a child Wayland surface is unavailable.
    // QImage::Format_ARGB32_Premultiplied has the BGRA layout mpv expects.
    static_assert(Q_BYTE_ORDER == Q_LITTLE_ENDIAN, "OSD direct path assumes little-endian QImage layout");
    auto *frame = new OverlayBuffer;
    frame->kind = OverlayBufferKind::Image;
    frame->image = QImage(width, height, QImage::Format_ARGB32_Premultiplied);
    frame->x = x;
    frame->y = y;
    if (frame->image.isNull()) {
        delete frame;
        return nullptr;
    }
    *stride = frame->image.bytesPerLine();
    *buffer = frame;
    return frame->image.bits();
}

void NativeAppWindow::overlayPresentCallback(void *data, void *buffer, bool visible)
{
    auto *self = static_cast<NativeAppWindow *>(data);
    auto *frame = static_cast<OverlayBuffer *>(buffer);
    if (!self) {
        destroyOverlayBuffer(frame);
        return;
    }

    if (frame && frame->kind == OverlayBufferKind::Wayland) {
        QMetaObject::invokeMethod(
            self, [self, frame, visible]() { self->presentOverlaySurface(frame, visible); }, Qt::QueuedConnection);
        return;
    }

    if (self->m_overlaySurface) {
        QMetaObject::invokeMethod(
            self, [self]() { self->presentOverlaySurface(nullptr, false); }, Qt::QueuedConnection);
    }

    if (!visible || !frame || frame->image.isNull()) {
        destroyOverlayBuffer(frame);
        self->scheduleOverlayImage(QImage());
        return;
    }

    self->scheduleOverlayImage(std::move(frame->image), frame->x, frame->y);
    destroyOverlayBuffer(frame);
}

void NativeAppWindow::presentOverlaySurface(void *opaqueBuffer, bool visible)
{
    auto *frame = static_cast<OverlayBuffer *>(opaqueBuffer);
    if (!m_overlaySurface || !m_overlaySubsurface || !visible || !frame || !frame->waylandBuffer) {
        if (m_overlaySurface) {
            wl_surface_attach(m_overlaySurface, nullptr, 0, 0);
            wl_surface_commit(m_overlaySurface);
            if (m_display)
                wl_display_flush(m_display);
        }
        destroyOverlayBuffer(frame);
        return;
    }

    // mpv has already drawn into this exact shared-memory buffer. Attaching it
    // lets the webOS compositor consume those pixels directly, bypassing the
    // Qt image provider and its CPU-to-texture upload.
    wl_subsurface_set_position(m_overlaySubsurface, frame->x, frame->y);
    wl_surface_attach(m_overlaySurface, frame->waylandBuffer, 0, 0);
    wl_surface_damage(m_overlaySurface, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_commit(m_overlaySurface);
    wl_display_flush(m_display);
    scheduleOverlayImage(QImage());
}

void NativeAppWindow::exportedCropCallback(
    void *data, int origW, int origH, int srcX, int srcY, int srcW, int srcH, int dstX, int dstY, int dstW, int dstH)
{
    auto *self = static_cast<NativeAppWindow *>(data);
    if (!self)
        return;

    self->scheduleVideoCrop(origW, origH, srcX, srcY, srcW, srcH, dstX, dstY, dstW, dstH);
}

const wl_registry_listener NativeAppWindow::s_registryListener = {
    &NativeAppWindow::registryGlobal,
    &NativeAppWindow::registryRemove,
};

const wl_webos_exported_listener NativeAppWindow::s_exportedListener = {
    &NativeAppWindow::exportedWindowIdAssigned,
};

} // namespace JellyfinNative
