#include "NativeAppWindow.h"
#include "diagnostics/InputLatencyMonitor.h"

#include <QCloseEvent>
#include <QDebug>
#include <QKeyEvent>
#include <QMutexLocker>
#include <QPlatformSurfaceEvent>
#include <QQuickImageProvider>

namespace JellyfinNative {

namespace {

    class OverlayImageProvider final : public QQuickImageProvider {
    public:
        explicit OverlayImageProvider(const NativeAppWindow *window)
            : QQuickImageProvider(QQuickImageProvider::Image)
            , m_window(window)
        {
        }

        QImage requestImage(const QString&, QSize *size, const QSize& requestedSize) override
        {
            QImage image = m_window->copyOverlayImage();
            if (image.isNull()) {
                image = QImage(1, 1, QImage::Format_ARGB32_Premultiplied);
                image.fill(Qt::transparent);
            }
            if (requestedSize.isValid() && requestedSize != image.size())
                image = image.scaled(requestedSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
            if (size)
                *size = image.size();
            return image;
        }

    private:
        const NativeAppWindow *m_window = nullptr;
    };

} // namespace

void NativeAppWindow::setInputLatencyMonitor(InputLatencyMonitor *monitor)
{
    m_inputLatencyMonitor = monitor;
}

bool NativeAppWindow::event(QEvent *event)
{
#ifdef JELLYFIN_NATIVE_WEBOS
    if (event->type() == QEvent::PlatformSurface) {
        const auto *surfaceEvent = static_cast<const QPlatformSurfaceEvent *>(event);
        if (surfaceEvent->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
            releasePlatformSurface();
    }
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        const auto *keyEvent = static_cast<const QKeyEvent *>(event);
        const quint32 scanCode = keyEvent->nativeScanCode();
        const quint32 virtualKey = keyEvent->nativeVirtualKey();
        if (keyEvent->key() == Qt::Key_Back || scanCode == 461 || virtualKey == 461) {
            qInfo() << "webOS Back event" << (event->type() == QEvent::KeyPress ? "press" : "release")
                    << "key=" << keyEvent->key() << "scan=" << scanCode << "virtual=" << virtualKey;
        }
    }
#endif
    const quint64 token = m_inputLatencyMonitor ? m_inputLatencyMonitor->beginInput(event) : 0;
    const bool handled = QQuickView::event(event);
#ifdef JELLYFIN_NATIVE_WEBOS
    if (event->type() == QEvent::PlatformSurface) {
        const auto *surfaceEvent = static_cast<const QPlatformSurfaceEvent *>(event);
        if (surfaceEvent->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated)
            ensureShellSurface();
    }
#endif
    if (m_inputLatencyMonitor)
        m_inputLatencyMonitor->endInput(token);
    return handled;
}

void NativeAppWindow::closeEvent(QCloseEvent *event)
{
    emit closeRequested();
    QQuickView::closeEvent(event);
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
        m_pendingOverlayX = 0;
        m_pendingOverlayY = 0;
        if (m_overlayImage.isNull())
            return;
        m_overlayImage = QImage();
        m_overlayX = 0;
        m_overlayY = 0;
        ++m_overlayRevision;
        changed = true;
    }
    if (changed)
        emit overlayRevisionChanged();
}

QQuickImageProvider *NativeAppWindow::createOverlayImageProvider()
{
    return new OverlayImageProvider(this);
}

QImage NativeAppWindow::copyOverlayImage() const
{
    QMutexLocker locker(&m_overlayMutex);
    return m_overlayImage;
}

} // namespace JellyfinNative
