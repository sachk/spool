#include "NativeAppWindow.h"

#include <QExposeEvent>
#include <QMutexLocker>
#include <QQuickImageProvider>
#include <QResizeEvent>

namespace JellyfinNative {

namespace {

class HostOverlayImageProvider final : public QQuickImageProvider
{
public:
    explicit HostOverlayImageProvider(const NativeAppWindow *window)
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
    setColor(Qt::black);
    setResizeMode(QQuickView::SizeRootObjectToView);
    setTitle(QStringLiteral("Jellyfin Native"));
    resize(1280, 720);
}

NativeAppWindow::~NativeAppWindow() = default;

bool NativeAppWindow::prepareForUiSurface()
{
    if (!isVisible())
        show();
    requestActivate();
    return true;
}

bool NativeAppWindow::prepareForPlaybackSurface()
{
    return prepareForUiSurface();
}

void NativeAppWindow::bringToFront()
{
    if (!isVisible())
        show();
    raise();
    requestActivate();
}

QString NativeAppWindow::windowId() const
{
    return {};
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
    return new HostOverlayImageProvider(this);
}

QImage NativeAppWindow::copyOverlayImage() const
{
    QMutexLocker locker(&m_overlayMutex);
    return m_overlayImage;
}

void NativeAppWindow::exposeEvent(QExposeEvent *event)
{
    QQuickView::exposeEvent(event);
}

void NativeAppWindow::resizeEvent(QResizeEvent *event)
{
    QQuickView::resizeEvent(event);
}

} // namespace JellyfinNative
