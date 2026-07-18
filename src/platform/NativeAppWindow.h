#pragma once

#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>
#include <QQuickView>

#include <memory>

namespace JellyfinNative {

class InputLatencyMonitor;
class NativeAppWindow final : public QQuickView {
    Q_OBJECT
    Q_PROPERTY(int overlayRevision READ overlayRevision NOTIFY overlayRevisionChanged)
    Q_PROPERTY(int overlayX READ overlayX NOTIFY overlayRevisionChanged)
    Q_PROPERTY(int overlayY READ overlayY NOTIFY overlayRevisionChanged)
    Q_PROPERTY(int overlayWidth READ overlayWidth NOTIFY overlayRevisionChanged)
    Q_PROPERTY(int overlayHeight READ overlayHeight NOTIFY overlayRevisionChanged)
    Q_PROPERTY(qint64 systemMemoryBytes READ systemMemoryBytes CONSTANT)
    Q_PROPERTY(bool fullScreen READ fullScreen NOTIFY fullScreenChanged)

public:
    explicit NativeAppWindow(const QString& appId, QWindow *parent = nullptr);
    ~NativeAppWindow() override;

    bool prepareForUiSurface();
    void setInputLatencyMonitor(InputLatencyMonitor *monitor);
    void completeUiSurface();
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
    void platformSurfaceStateChanged(int state);
    void platformSurfaceExposed(bool exposed);
    void platformCloseRequested();
    void pointerBackRequested();
    void pointerForwardRequested();

protected:
    bool event(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void exposeEvent(QExposeEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    bool ensureShellSurface();
    bool ensureVideoSurface();
    bool bindPlatformGlobals();
    void releasePlatformSurface();
    void requestPlatformFullscreen();
    void applyPlatformKeyMask();
    void updateCropRegion();
    void setVideoCrop(
        int origW, int origH, int srcX, int srcY, int srcW, int srcH, int dstX, int dstY, int dstW, int dstH);
    void scheduleVideoCrop(
        int origW, int origH, int srcX, int srcY, int srcW, int srcH, int dstX, int dstY, int dstW, int dstH);
    void publishPendingVideoCrop();
    void scheduleOverlayImage(QImage image, int x = 0, int y = 0);
    void publishPendingOverlayImage();
    void handlePlatformSurfaceCreated();
    void handlePlatformSurfaceAboutToBeDestroyed();

    struct PlatformData;
    friend struct PlatformData;

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
    Qt::MouseButton m_pointerNavigationButton = Qt::NoButton;
    std::unique_ptr<PlatformData> m_platform;
};

} // namespace JellyfinNative
