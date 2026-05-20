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

class NativeAppWindow final : public QQuickView
{
    Q_OBJECT
    Q_PROPERTY(int overlayRevision READ overlayRevision NOTIFY overlayRevisionChanged)

public:
    explicit NativeAppWindow(const QString &appId, QWindow *parent = nullptr);
    ~NativeAppWindow() override;

    bool prepareForUiSurface();
    bool prepareForPlaybackSurface();
    // Bring the surface to the foreground. On webOS this re-issues
    // wl_webos_shell_surface_set_state(FULLSCREEN); on host Qt it
    // falls back to show()/requestActivate(). Safe to call from the
    // GUI thread at any point after prepareForUiSurface().
    void bringToFront();
    QString windowId() const;
    int overlayRevision() const;
    void clearOverlay();
    QQuickImageProvider *createOverlayImageProvider();
    QImage copyOverlayImage() const;

signals:
    void overlayRevisionChanged();

protected:
    void exposeEvent(QExposeEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
#ifdef JELLYFIN_NATIVE_WEBOS
    bool ensureShellSurface();
    bool ensureVideoSurface();
    bool bindGlobals();
    void updateCropRegion();
    void setVideoCrop(int origW, int origH, int srcX, int srcY, int srcW, int srcH,
                      int dstX, int dstY, int dstW, int dstH);
    void presentOverlayCopy(const uint8_t *pixels, int width, int height, int stride);

    static void registryGlobal(void *data, wl_registry *registry, uint32_t name,
                               const char *interface, uint32_t version);
    static void registryRemove(void *, wl_registry *, uint32_t);
    static void exportedWindowIdAssigned(void *data, wl_webos_exported *, const char *window_id, uint32_t);
    static void overlayPresentCallback(void *data, const uint8_t *pixels,
                                       int width, int height, int stride);
    static void exportedCropCallback(void *data, int origW, int origH,
                                     int srcX, int srcY, int srcW, int srcH,
                                     int dstX, int dstY, int dstW, int dstH);
#endif

    QString m_appId;
    mutable QMutex m_overlayMutex;
    QImage m_overlayImage;
    int m_overlayRevision = 0;
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
#endif
};

} // namespace JellyfinNative
