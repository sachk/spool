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
    setTitle(QStringLiteral("Spool for Jellyfin"));
}

NativeAppWindow::~NativeAppWindow() = default;

bool NativeAppWindow::prepareForUiSurface()
{
    if (!isVisible())
        setImmersive(false);
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
        setImmersive(m_immersive);
    requestActivate();
}

void NativeAppWindow::toggleFullScreen()
{
    setImmersive(!m_immersive);
}

// Browsing shares the screen with the system: the clock, the notifications
// and the gesture bar all stay where the user expects them, and the interface
// is inset to clear them. Only playback takes the whole panel.
void NativeAppWindow::setImmersive(bool immersive)
{
    m_immersive = immersive;
    if (immersive)
        showFullScreen();
    else
        showMaximized();
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
