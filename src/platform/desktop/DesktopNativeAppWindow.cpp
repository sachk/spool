#include "platform/NativeAppWindow.h"

#include <QExposeEvent>
#include <QResizeEvent>

namespace JellyfinNative {
struct NativeAppWindow::PlatformData { };

NativeAppWindow::NativeAppWindow(const QString& appId, QWindow *parent)
    : QQuickView(parent)
    , m_appId(appId)
    , m_platform(std::make_unique<PlatformData>())
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

void NativeAppWindow::toggleFullScreen()
{
    if (fullScreen())
        showNormal();
    else
        showFullScreen();
    requestActivate();
    emit fullScreenChanged();
}

QString NativeAppWindow::windowId() const
{
    return {};
}

void NativeAppWindow::exposeEvent(QExposeEvent *event)
{
    QQuickView::exposeEvent(event);
}

void NativeAppWindow::resizeEvent(QResizeEvent *event)
{
    QQuickView::resizeEvent(event);
}

void NativeAppWindow::handlePlatformSurfaceCreated() { }

void NativeAppWindow::handlePlatformSurfaceAboutToBeDestroyed() { }

} // namespace JellyfinNative
