#include "platform/PlatformPlaybackSurface.h"

#include "player/MpvVideoItem.h"

#include <QDebug>
#include <QMetaObject>
#include <QObject>

namespace JellyfinNative {
namespace {
    QMetaObject::Connection g_renderErrorConnection;
}

bool platformIdleMpvPreparationEnabled()
{
    return false;
}
void runAfterPlatformMpvLoaded(std::function<void()> callback)
{
    callback();
}
MpvOptionProfile::Platform platformMpvOptionProfile()
{
    return MpvOptionProfile::Platform::Desktop;
}
QString platformPlaybackBackendName()
{
    return QStringLiteral("desktop");
}

bool configurePlatformMpvSurface(mpv_handle *, NativeAppWindow&, bool, QString&)
{
    return true;
}

bool attachPlatformMpvSurface(mpv_handle *handle, bool needsVideoSurface, QObject& context,
    std::function<void(const QString&)> errorHandler, QString& errorMessage)
{
    if (!needsVideoSurface)
        return true;
    auto *videoItem = MpvVideoItem::instance();
    if (!videoItem) {
        qCritical() << "playback surface: MpvVideoItem instance is missing";
        errorMessage = QStringLiteral("The video surface is unavailable. Return to the library and try again.");
        return false;
    }
    QObject::disconnect(g_renderErrorConnection);
    g_renderErrorConnection = QObject::connect(videoItem, &MpvVideoItem::renderError, &context,
        [errorHandler = std::move(errorHandler)](const QString& message) { errorHandler(message); });
    videoItem->setMpvHandle(handle);
    return true;
}

bool releasePlatformMpvSurface()
{
    auto *videoItem = MpvVideoItem::instance();
    if (!videoItem)
        return true;
    return videoItem->releaseMpvHandle();
}

QString platformPreparingStatus(bool needsVideoSurface)
{
    return needsVideoSurface ? QStringLiteral("Preparing libmpv...") : QStringLiteral("Preparing audio...");
}

bool applyPlatformSubtitlePreload(mpv_handle *, const PlaybackSession&, const QString&, QString&)
{
    return true;
}
bool platformUsesBackgroundPlaybackPolicy()
{
    return false;
}
void platformAudioTrackChanged(int) { }

} // namespace JellyfinNative
