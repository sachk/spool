#include "NativeAppWindow.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QExposeEvent>
#include <QGuiApplication>
#include <QMetaObject>
#include <QMutexLocker>
#include <QQuickImageProvider>
#include <QResizeEvent>
#include <QThread>

#include <qpa/qplatformnativeinterface.h>

#include <cstring>

extern "C" {
#include "../../mpv/video/out/starfish/starfish_ctx.h"
}

namespace JellyfinNative {

namespace {

class NativeOverlayImageProvider final : public QQuickImageProvider
{
public:
    explicit NativeOverlayImageProvider(const NativeAppWindow *window)
        : QQuickImageProvider(QQuickImageProvider::Image)
        , m_window(window)
    {
    }

    QImage requestImage(const QString &, QSize *size, const QSize &requestedSize) override
    {
        QImage image = m_window->copyOverlayImage();
        if (image.isNull()) {
            image = QImage(1, 1, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
        }
        if (requestedSize.isValid() && !image.isNull())
            image = image.scaled(requestedSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        if (size)
            *size = image.size();
        return image;
    }

private:
    const NativeAppWindow *m_window;
};

} // namespace

NativeAppWindow::NativeAppWindow(const QString &appId, QWindow *parent)
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
    starfish_overlay_set_present_cb(&NativeAppWindow::overlayPresentCallback, this);
    starfish_exported_set_crop_cb(&NativeAppWindow::exportedCropCallback, this);
}

NativeAppWindow::~NativeAppWindow()
{
    starfish_exported_set_crop_cb(nullptr, nullptr);
    starfish_overlay_set_present_cb(nullptr, nullptr);
    if (m_exported)
        wl_webos_exported_destroy(m_exported);
    if (m_webosShellSurface)
        wl_webos_shell_surface_destroy(m_webosShellSurface);
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

    setenv("STARFISH_WINDOW_ID", m_windowId.c_str(), 1);
    setenv("STARFISH_WINDOW_WIDTH", QByteArray::number(width()).constData(), 1);
    setenv("STARFISH_WINDOW_HEIGHT", QByteArray::number(height()).constData(), 1);
    return true;
}

QString NativeAppWindow::windowId() const
{
    return QString::fromStdString(m_windowId);
}

int NativeAppWindow::overlayRevision() const
{
    return m_overlayRevision;
}

void NativeAppWindow::clearOverlay()
{
    bool changed = false;
    {
        QMutexLocker locker(&m_overlayMutex);
        m_pendingOverlayImage = QImage();
        if (m_overlayImage.isNull())
            return;
        m_overlayImage = QImage();
        m_overlayRevision += 1;
        changed = true;
    }
    if (changed)
        emit overlayRevisionChanged();
}

QQuickImageProvider *NativeAppWindow::createOverlayImageProvider()
{
    return new NativeOverlayImageProvider(this);
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
        setenv("STARFISH_WINDOW_WIDTH", QByteArray::number(width()).constData(), 1);
        setenv("STARFISH_WINDOW_HEIGHT", QByteArray::number(height()).constData(), 1);
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

    m_exported =
        wl_webos_foreign_export_element(m_webosForeign, m_surface, WL_WEBOS_FOREIGN_WEBOS_EXPORTED_TYPE_VIDEO_OBJECT);
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

    wl_webos_shell_surface_set_property(m_webosShellSurface, "_WEBOS_ACCESS_POLICY_KEYS_GUIDE", "true");
    wl_webos_shell_surface_set_property(m_webosShellSurface, "_WEBOS_ACCESS_POLICY_KEYS_BACK", "true");
    wl_webos_shell_surface_set_property(m_webosShellSurface, "appId", m_appId.toUtf8().constData());
    wl_webos_shell_surface_set_property(
        m_webosShellSurface, "displayAffinity", getenv("DISPLAY_ID") ? getenv("DISPLAY_ID") : "0");
    wl_webos_shell_surface_set_state(m_webosShellSurface, WL_WEBOS_SHELL_SURFACE_STATE_FULLSCREEN);
    wl_surface_commit(m_surface);
    wl_display_roundtrip(m_display);
    return true;
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

void NativeAppWindow::setVideoCrop(int origW, int origH, int srcX, int srcY,
                                   int srcW, int srcH, int dstX, int dstY,
                                   int dstW, int dstH)
{
    if (origW <= 0 || origH <= 0 || srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0)
        return;

    const bool changed = m_cropOrigW != origW || m_cropOrigH != origH ||
                         m_cropSrcX != srcX || m_cropSrcY != srcY ||
                         m_cropSrcW != srcW || m_cropSrcH != srcH ||
                         m_cropDstX != dstX || m_cropDstY != dstY ||
                         m_cropDstW != dstW || m_cropDstH != dstH;
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

void NativeAppWindow::scheduleOverlayImage(QImage image)
{
    bool shouldQueue = false;
    {
        QMutexLocker locker(&m_overlayMutex);
        m_pendingOverlayImage = std::move(image);
        if (!m_overlayPublishQueued) {
            m_overlayPublishQueued = true;
            shouldQueue = true;
        }
    }

    if (shouldQueue) {
        QMetaObject::invokeMethod(
            this, [this]() { publishPendingOverlayImage(); }, Qt::QueuedConnection);
    }
}

void NativeAppWindow::publishPendingOverlayImage()
{
    bool changed = false;
    {
        QMutexLocker locker(&m_overlayMutex);
        QImage image = std::move(m_pendingOverlayImage);
        m_pendingOverlayImage = QImage();
        m_overlayPublishQueued = false;
        if (image.isNull() && m_overlayImage.isNull())
            return;
        m_overlayImage = std::move(image);
        m_overlayRevision += 1;
        changed = true;
    }
    if (changed)
        emit overlayRevisionChanged();
}

QImage NativeAppWindow::copyOverlayImage() const
{
    // QImage is implicitly shared — returning by value bumps the refcount.
    // The producer's next assignment replaces m_overlayImage with new storage
    // while ours stays alive via COW, so we don't need a deep copy here.
    QMutexLocker locker(&m_overlayMutex);
    return m_overlayImage;
}

void NativeAppWindow::registryGlobal(void *data, wl_registry *registry, uint32_t name,
                                     const char *interface, uint32_t version)
{
    auto *self = static_cast<NativeAppWindow *>(data);
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        const uint32_t bindVersion = version < 3 ? version : 3;
        self->m_compositor =
            static_cast<wl_compositor *>(wl_registry_bind(registry, name, &wl_compositor_interface, bindVersion));
    } else if (strcmp(interface, wl_subcompositor_interface.name) == 0) {
        self->m_subcompositor =
            static_cast<wl_subcompositor *>(wl_registry_bind(registry, name, &wl_subcompositor_interface, 1));
    } else if (strcmp(interface, wl_webos_shell_interface.name) == 0) {
        const uint32_t bindVersion = version < 2 ? version : 2;
        self->m_webosShell =
            static_cast<wl_webos_shell *>(wl_registry_bind(registry, name, &wl_webos_shell_interface, bindVersion));
    } else if (strcmp(interface, wl_webos_foreign_interface.name) == 0) {
        const uint32_t bindVersion = version < 2 ? version : 2;
        self->m_webosForeign =
            static_cast<wl_webos_foreign *>(wl_registry_bind(registry, name, &wl_webos_foreign_interface, bindVersion));
    }
}

void NativeAppWindow::registryRemove(void *, wl_registry *, uint32_t)
{
}

void NativeAppWindow::exportedWindowIdAssigned(void *data, wl_webos_exported *, const char *window_id, uint32_t)
{
    auto *self = static_cast<NativeAppWindow *>(data);
    self->m_windowId = window_id ? window_id : "";
}

void NativeAppWindow::overlayPresentCallback(void *data, const uint8_t *pixels,
                                             int width, int height, int stride)
{
    auto *self = static_cast<NativeAppWindow *>(data);
    if (!self)
        return;

    // Reject obviously-invalid buffers and clear the overlay. Callers send a
    // zeroed (or NULL) buffer when there is no OSD content; we treat both the
    // same.
    if (!pixels || width <= 0 || height <= 0 || stride < width * 4) {
        self->scheduleOverlayImage(QImage());
        return;
    }

    // vo_starfish memsets the buffer to zero before rasterising OSD, so an
    // all-zero buffer means "no overlay content". Scan in 64-bit chunks so a
    // 1920x1080 empty frame finishes in a few hundred microseconds instead of
    // ~8M byte comparisons.
    const size_t byteCount = static_cast<size_t>(height) * stride;
    bool hasContent = false;
    {
        const auto *words = reinterpret_cast<const uint64_t *>(pixels);
        const size_t wordCount = byteCount / sizeof(uint64_t);
        for (size_t i = 0; i < wordCount; ++i) {
            if (words[i]) {
                hasContent = true;
                break;
            }
        }
        if (!hasContent) {
            for (size_t i = wordCount * sizeof(uint64_t); i < byteCount; ++i) {
                if (pixels[i]) {
                    hasContent = true;
                    break;
                }
            }
        }
    }
    if (!hasContent) {
        self->scheduleOverlayImage(QImage());
        return;
    }

    // QImage::Format_ARGB32_Premultiplied stores pixels in memory as B,G,R,A
    // on little-endian — the same byte layout vo_starfish writes (IMGFMT_BGRA,
    // premultiplied). A single per-scanline memcpy is correct; the old
    // per-pixel unpack-and-repack via qRgba(r,g,b,a) was a no-op that ran
    // ~2M times per frame on the GUI thread.
    static_assert(Q_BYTE_ORDER == Q_LITTLE_ENDIAN,
                  "OSD copy fast-path assumes little-endian QImage layout");
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    const int dstStride = image.bytesPerLine();
    if (dstStride == stride) {
        memcpy(image.bits(), pixels, byteCount);
    } else {
        const int rowBytes = qMin(dstStride, stride);
        for (int y = 0; y < height; ++y) {
            memcpy(image.scanLine(y),
                   pixels + static_cast<size_t>(y) * stride,
                   rowBytes);
        }
    }

    self->scheduleOverlayImage(std::move(image));
}

void NativeAppWindow::exportedCropCallback(void *data, int origW, int origH,
                                            int srcX, int srcY, int srcW, int srcH,
                                            int dstX, int dstY, int dstW, int dstH)
{
    auto *self = static_cast<NativeAppWindow *>(data);
    if (!self)
        return;

    if (QThread::currentThread() == self->thread()) {
        self->setVideoCrop(origW, origH, srcX, srcY, srcW, srcH,
                           dstX, dstY, dstW, dstH);
        return;
    }

    QMetaObject::invokeMethod(
        self,
        [self, origW, origH, srcX, srcY, srcW, srcH, dstX, dstY, dstW, dstH]() {
            self->setVideoCrop(origW, origH, srcX, srcY, srcW, srcH,
                               dstX, dstY, dstW, dstH);
        },
        Qt::BlockingQueuedConnection);
}

const wl_registry_listener NativeAppWindow::s_registryListener = {
    &NativeAppWindow::registryGlobal,
    &NativeAppWindow::registryRemove,
};

const wl_webos_exported_listener NativeAppWindow::s_exportedListener = {
    &NativeAppWindow::exportedWindowIdAssigned,
};

} // namespace JellyfinNative
