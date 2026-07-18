#pragma once

#ifdef JELLYFIN_NATIVE_WEBOS
extern "C" {
#include <wayland-client.h>
#include <wayland-webos-foreign-client-protocol.h>
#include <wayland-webos-shell-client-protocol.h>
}
#endif

#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>
#include <QQuickView>

#include <string>

namespace JellyfinNative {

class InputLatencyMonitor;
class NativeAppWindow final : public QQuickView {
    Q_OBJECT
    Q_PROPERTY(int overlayRevision READ overlayRevision NOTIFY overlayRevisionChanged)
    Q_PROPERTY(int overlayX READ overlayX NOTIFY overlayRevisionChanged)
    Q_PROPERTY(int overlayY READ overlayY NOTIFY overlayRevisionChanged)
    Q_PROPERTY(int overlayWidth READ overlayWidth NOTIFY overlayRevisionChanged)
    Q_PROPERTY(int overlayHeight READ overlayHeight NOTIFY overlayRevisionChanged)
    Q_PROPERTY(bool tvPlatform READ tvPlatform CONSTANT)
    Q_PROPERTY(bool smartTvPlatform READ smartTvPlatform CONSTANT)
    Q_PROPERTY(qint64 systemMemoryBytes READ systemMemoryBytes CONSTANT)
    Q_PROPERTY(bool fullScreen READ fullScreen NOTIFY fullScreenChanged)

public:
    explicit NativeAppWindow(const QString& appId, QWindow *parent = nullptr);
    ~NativeAppWindow() override;

    bool prepareForUiSurface();
    void setInputLatencyMonitor(InputLatencyMonitor *monitor);
    bool prepareForPlaybackSurface();
    // Bring the surface to the foreground. On webOS this re-issues
    // wl_webos_shell_surface_set_state(FULLSCREEN); on host Qt it
    // falls back to show()/requestActivate(). Safe to call from the
    // GUI thread at any point after prepareForUiSurface().
    void bringToFront();
    QString windowId() const;
    int overlayRevision() const;
    int overlayX() const
    {
        return m_overlayX;
    }
    int overlayY() const
    {
        return m_overlayY;
    }
    int overlayWidth() const
    {
        return m_overlayImage.width();
    }
    int overlayHeight() const
    {
        return m_overlayImage.height();
    }
    bool fullScreen() const
    {
        return visibility() == QWindow::FullScreen || windowStates().testFlag(Qt::WindowFullScreen);
    }
    bool smartTvPlatform() const
    {
#ifdef JELLYFIN_NATIVE_WEBOS
        return true;
#else
        return false;
#endif
    }
    bool tvPlatform() const
    {
        return smartTvPlatform();
    }
    qint64 systemMemoryBytes() const
    {
        return m_systemMemoryBytes;
    }
    void setSystemMemoryBytes(qint64 bytes)
    {
        m_systemMemoryBytes = qMax<qint64>(0, bytes);
    }
    Q_INVOKABLE void toggleFullScreen();
    void clearOverlay();
    QQuickImageProvider *createOverlayImageProvider();
    QImage copyOverlayImage() const;

signals:
    void closeRequested();
    void overlayRevisionChanged();
    void fullScreenChanged();
#ifdef JELLYFIN_NATIVE_WEBOS
    void webOsShellStateChanged(int state);
    void webOsShellExposed(bool exposed);
    void webOsShellCloseRequested();
#endif

protected:
    bool event(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void exposeEvent(QExposeEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
#ifdef JELLYFIN_NATIVE_WEBOS
    bool ensureShellSurface();
    bool ensureVideoSurface();
    bool bindGlobals();
    void releasePlatformSurface();
    void requestWebOsFullscreen();
    void applyWebOsKeyMask();
    void updateCropRegion();
    void setVideoCrop(
        int origW, int origH, int srcX, int srcY, int srcW, int srcH, int dstX, int dstY, int dstW, int dstH);
    void scheduleVideoCrop(
        int origW, int origH, int srcX, int srcY, int srcW, int srcH, int dstX, int dstY, int dstW, int dstH);
    void publishPendingVideoCrop();
    void scheduleOverlayImage(QImage image, int x = 0, int y = 0);
    void publishPendingOverlayImage();

    static void registryGlobal(
        void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
    static void registryRemove(void *, wl_registry *, uint32_t);
    static void exportedWindowIdAssigned(void *data, wl_webos_exported *, const char *window_id, uint32_t);
    static void shellStateChanged(void *data, wl_webos_shell_surface *, uint32_t state);
    static void shellPositionChanged(void *, wl_webos_shell_surface *, int32_t, int32_t);
    static void shellClose(void *data, wl_webos_shell_surface *);
    static void shellExposed(void *data, wl_webos_shell_surface *, wl_array *rectangles);
    static void shellStateAboutToChange(void *, wl_webos_shell_surface *, uint32_t);
    static void shellAddonStatusChanged(void *, wl_webos_shell_surface *, uint32_t);
    static uint8_t *overlayAcquireCallback(void *data, int x, int y, int width, int height, int *stride, void **buffer);
    static void overlayPresentCallback(void *data, void *buffer, bool visible);
    static void exportedCropCallback(void *data, int origW, int origH, int srcX, int srcY, int srcW, int srcH, int dstX,
        int dstY, int dstW, int dstH);
#endif

    InputLatencyMonitor *m_inputLatencyMonitor = nullptr;
    QString m_appId;
    mutable QMutex m_overlayMutex;
    QImage m_overlayImage;
    QImage m_pendingOverlayImage;
    int m_overlayX = 0;
    int m_overlayY = 0;
    int m_pendingOverlayX = 0;
    int m_pendingOverlayY = 0;
    bool m_overlayPublishQueued = false;
    int m_overlayRevision = 0;
    qint64 m_systemMemoryBytes = 0;
#ifdef JELLYFIN_NATIVE_WEBOS
    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    wl_compositor *m_compositor = nullptr;
    wl_subcompositor *m_subcompositor = nullptr;
    wl_surface *m_surface = nullptr;
    wl_webos_shell *m_webosShell = nullptr;
    wl_webos_shell_surface *m_webosShellSurface = nullptr;
    wl_webos_foreign *m_webosForeign = nullptr;
    wl_webos_exported *m_exported = nullptr;
    std::string m_windowId;
    int m_fullscreenRequestGeneration = 0;
    bool m_fullscreenConfirmationPending = false;
    struct CropRegion {
        int origW = 0;
        int origH = 0;
        int srcX = 0;
        int srcY = 0;
        int srcW = 0;
        int srcH = 0;
        int dstX = 0;
        int dstY = 0;
        int dstW = 0;
        int dstH = 0;
        bool valid = false;
    };
    QMutex m_cropMutex;
    CropRegion m_pendingCrop;
    bool m_cropUpdateQueued = false;
    int m_cropOrigW = 0;
    int m_cropOrigH = 0;
    int m_cropSrcX = 0;
    int m_cropSrcY = 0;
    int m_cropSrcW = 0;
    int m_cropSrcH = 0;
    int m_cropDstX = 0;
    int m_cropDstY = 0;
    int m_cropDstW = 0;
    int m_cropDstH = 0;

    static const wl_registry_listener s_registryListener;
    static const wl_webos_exported_listener s_exportedListener;
    static const wl_webos_shell_surface_listener s_shellSurfaceListener;
#endif
};

} // namespace JellyfinNative
