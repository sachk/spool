#include "NativeAppWindow.h"

#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QExposeEvent>
#include <QGuiApplication>
#include <QMetaObject>
#include <QMutexLocker>
#include <QResizeEvent>
#include <QThread>
#include <QTimer>

#include <qpa/qplatformnativeinterface.h>

#include <cstring>

extern "C" {
#include "video/out/starfish/starfish_ctx.h"
}

namespace JellyfinNative {

namespace {

    struct OverlayImageBuffer {
        QImage image;
        int x = 0;
        int y = 0;
    };

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
    releasePlatformSurface();
    if (m_webosShell)
        wl_webos_shell_destroy(m_webosShell);
    if (m_webosForeign)
        wl_webos_foreign_destroy(m_webosForeign);
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
    showFullScreen();
    if (!ensureShellSurface())
        return;
    m_fullscreenConfirmationPending = true;
    const int generation = ++m_fullscreenRequestGeneration;
    requestWebOsFullscreen();
    QTimer::singleShot(250, this, [this, generation]() {
        if (!m_fullscreenConfirmationPending || generation != m_fullscreenRequestGeneration)
            return;
        qInfo() << "webOS shell: fullscreen not yet confirmed; retrying";
        requestWebOsFullscreen();
        QTimer::singleShot(500, this, [this, generation]() {
            if (m_fullscreenConfirmationPending && generation == m_fullscreenRequestGeneration)
                qWarning() << "webOS shell: fullscreen request remained unconfirmed";
        });
    });
}

void NativeAppWindow::requestWebOsFullscreen()
{
    if (!m_webosShellSurface)
        return;
    wl_webos_shell_surface_set_state(m_webosShellSurface, WL_WEBOS_SHELL_SURFACE_STATE_FULLSCREEN);
    applyWebOsKeyMask();
    if (m_surface)
        wl_surface_commit(m_surface);
    if (m_display)
        wl_display_flush(m_display);
}

void NativeAppWindow::releasePlatformSurface()
{
    ++m_fullscreenRequestGeneration;
    m_fullscreenConfirmationPending = false;
    if (m_exported) {
        wl_webos_exported_destroy(m_exported);
        m_exported = nullptr;
    }
    if (m_webosShellSurface) {
        wl_webos_shell_surface_destroy(m_webosShellSurface);
        m_webosShellSurface = nullptr;
    }
    m_surface = nullptr;
    m_windowId.clear();
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
    if (m_webosShellSurface && m_surface)
        return true;

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

    wl_webos_shell_surface_add_listener(m_webosShellSurface, &s_shellSurfaceListener, this);

    wl_webos_shell_surface_set_property(m_webosShellSurface, "_WEBOS_ACCESS_POLICY_KEYS_GUIDE", "true");
    wl_webos_shell_surface_set_property(m_webosShellSurface, "_WEBOS_ACCESS_POLICY_KEYS_BACK", "true");
    wl_webos_shell_surface_set_property(m_webosShellSurface, "appId", m_appId.toUtf8().constData());
    wl_webos_shell_surface_set_property(
        m_webosShellSurface, "displayAffinity", getenv("DISPLAY_ID") ? getenv("DISPLAY_ID") : "0");
    wl_webos_shell_surface_set_state(m_webosShellSurface, WL_WEBOS_SHELL_SURFACE_STATE_FULLSCREEN);
    applyWebOsKeyMask();
    wl_surface_commit(m_surface);
    wl_display_roundtrip(m_display);
    return true;
}

void NativeAppWindow::applyWebOsKeyMask()
{
    if (!m_webosShellSurface)
        return;

    // Apply this after the fullscreen state request so any state-transition
    // defaults are established before we claim Back. Reapply on relaunch
    // because the same shell surface remains alive across backgrounding.
    constexpr uint32_t keyMask = WL_WEBOS_SHELL_SURFACE_WEBOS_KEY_DEFAULT | WL_WEBOS_SHELL_SURFACE_WEBOS_KEY_BACK;
    wl_webos_shell_surface_set_key_mask(m_webosShellSurface, keyMask);
    qInfo().nospace() << "webOS shell key mask applied mask=0x" << QString::number(keyMask, 16);
}

bool NativeAppWindow::bindGlobals()
{
    if (m_webosForeign && m_compositor)
        return true;

    m_registry = wl_display_get_registry(m_display);
    if (!m_registry)
        return false;

    wl_registry_add_listener(m_registry, &s_registryListener, this);
    wl_display_roundtrip(m_display);
    wl_display_roundtrip(m_display);

    return m_compositor && m_subcompositor && m_webosForeign && m_webosShell;
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

void NativeAppWindow::shellStateChanged(void *data, wl_webos_shell_surface *, uint32_t state)
{
    auto *self = static_cast<NativeAppWindow *>(data);
    if (!self)
        return;
    QMetaObject::invokeMethod(
        self,
        [self, state]() {
            qInfo() << "webOS shell state changed:" << state;
            if (state == WL_WEBOS_SHELL_SURFACE_STATE_FULLSCREEN)
                self->m_fullscreenConfirmationPending = false;
            emit self->webOsShellStateChanged(static_cast<int>(state));
        },
        Qt::QueuedConnection);
}

void NativeAppWindow::shellPositionChanged(void *, wl_webos_shell_surface *, int32_t, int32_t) { }

void NativeAppWindow::shellClose(void *data, wl_webos_shell_surface *)
{
    auto *self = static_cast<NativeAppWindow *>(data);
    if (self)
        QMetaObject::invokeMethod(self, [self]() { emit self->webOsShellCloseRequested(); }, Qt::QueuedConnection);
}

void NativeAppWindow::shellExposed(void *data, wl_webos_shell_surface *, wl_array *rectangles)
{
    auto *self = static_cast<NativeAppWindow *>(data);
    if (!self)
        return;
    bool exposed = false;
    if (rectangles && rectangles->data && rectangles->size >= sizeof(int32_t)) {
        const auto *values = static_cast<const int32_t *>(rectangles->data);
        exposed = values[0] >= 0;
    }
    QMetaObject::invokeMethod(
        self,
        [self, exposed]() {
            if (exposed)
                self->m_fullscreenConfirmationPending = false;
            qInfo() << "webOS shell exposed:" << exposed;
            emit self->webOsShellExposed(exposed);
        },
        Qt::QueuedConnection);
}

void NativeAppWindow::shellStateAboutToChange(void *, wl_webos_shell_surface *, uint32_t) { }

void NativeAppWindow::shellAddonStatusChanged(void *, wl_webos_shell_surface *, uint32_t) { }

uint8_t *NativeAppWindow::overlayAcquireCallback(
    void *data, int x, int y, int width, int height, int *stride, void **buffer)
{
    auto *self = static_cast<NativeAppWindow *>(data);
    if (!self || !stride || !buffer || width <= 0 || height <= 0)
        return nullptr;

    // QImage::Format_ARGB32_Premultiplied stores pixels in memory as B,G,R,A
    // on little-endian — the same byte layout vo_starfish writes (IMGFMT_BGRA,
    // premultiplied). mpv writes directly into this Qt-owned storage, so the
    // callback path has no full-frame CPU copy.
    static_assert(Q_BYTE_ORDER == Q_LITTLE_ENDIAN, "OSD direct path assumes little-endian QImage layout");
    auto *frame = new OverlayImageBuffer { QImage(width, height, QImage::Format_ARGB32_Premultiplied), x, y };
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
    auto *frame = static_cast<OverlayImageBuffer *>(buffer);
    if (!self) {
        delete frame;
        return;
    }

    if (!visible || !frame || frame->image.isNull()) {
        delete frame;
        self->scheduleOverlayImage(QImage());
        return;
    }

    self->scheduleOverlayImage(std::move(frame->image), frame->x, frame->y);
    delete frame;
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

const wl_webos_shell_surface_listener NativeAppWindow::s_shellSurfaceListener = {
    &NativeAppWindow::shellStateChanged,
    &NativeAppWindow::shellPositionChanged,
    &NativeAppWindow::shellClose,
    &NativeAppWindow::shellExposed,
    &NativeAppWindow::shellStateAboutToChange,
    &NativeAppWindow::shellAddonStatusChanged,
};

} // namespace JellyfinNative
