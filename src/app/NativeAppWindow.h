#pragma once

extern "C" {
#include <wayland-client.h>
#include <wayland-webos-foreign-client-protocol.h>
#include <wayland-webos-shell-client-protocol.h>
}

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

    QString m_appId;
    mutable QMutex m_overlayMutex;
    QImage m_overlayImage;
    int m_overlayRevision = 0;
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
};

} // namespace JellyfinNative
