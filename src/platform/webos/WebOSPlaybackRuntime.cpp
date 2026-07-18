#include "platform/PlatformPlaybackRuntime.h"

#include "api/JellyfinApiFacade.h"
#include "platform/webos/WebOSMpvRuntime.h"

#include <QDebug>
#include <QMetaObject>
#include <QPointer>

namespace JellyfinNative {

qint64 platformAudioDecodeCpuTimeNs()
{
    return WebOSMpvRuntime::audioDecodeCpuTimeNs();
}

void configurePlatformPlaybackCapabilities(JellyfinApiFacade& api, QObject& callbackContext)
{
    api.setVideoCodecCapabilities({ QStringLiteral("h264") }, true);
    QPointer<JellyfinApiFacade> guardedApi(&api);
    QPointer<QObject> guardedContext(&callbackContext);
    WebOSMpvRuntime::probeStarfishVideoCodecsAsync([guardedApi, guardedContext](const QStringList& codecs) {
        if (!guardedContext)
            return;
        QMetaObject::invokeMethod(
            guardedContext,
            [guardedApi, codecs] {
                if (!guardedApi || codecs.isEmpty()) {
                    qWarning() << "playback capabilities: Starfish probe returned no codecs; retaining h264";
                    return;
                }
                guardedApi->setVideoCodecCapabilities(codecs, true);
            },
            Qt::QueuedConnection);
    });
}

} // namespace JellyfinNative
