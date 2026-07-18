#include "platform/PlatformPlaybackRuntime.h"

#include "api/JellyfinApiFacade.h"
#include "platform/webos/WebOSMpvRuntime.h"

#include <QDebug>
#include <QMetaObject>
#include <QPointer>

namespace JellyfinNative {
namespace {

    const QStringList kSoftwareVideoCodecs { QStringLiteral("mpeg1video"), QStringLiteral("mpeg2video"),
        QStringLiteral("mpeg4"), QStringLiteral("h263"), QStringLiteral("vc1") };

    QStringList withSoftwareVideoCodecs(QStringList codecs)
    {
        for (const QString& codec : kSoftwareVideoCodecs) {
            if (!codecs.contains(codec, Qt::CaseInsensitive))
                codecs.push_back(codec);
        }
        return codecs;
    }

} // namespace

qint64 platformAudioDecodeCpuTimeNs()
{
    return WebOSMpvRuntime::audioDecodeCpuTimeNs();
}

void configurePlatformPlaybackCapabilities(JellyfinApiFacade& api, QObject& callbackContext)
{
    api.setVideoCodecCapabilities(withSoftwareVideoCodecs({ QStringLiteral("h264") }), true);
    QPointer<JellyfinApiFacade> guardedApi(&api);
    QPointer<QObject> guardedContext(&callbackContext);
    WebOSMpvRuntime::probeStarfishVideoCodecsAsync([guardedApi, guardedContext](const QStringList& codecs) {
        if (!guardedContext)
            return;
        QMetaObject::invokeMethod(
            guardedContext,
            [guardedApi, codecs] {
                if (!guardedApi)
                    return;
                if (codecs.isEmpty()) {
                    qWarning() << "playback capabilities: Starfish probe returned no codecs; retaining software set";
                    return;
                }
                guardedApi->setVideoCodecCapabilities(withSoftwareVideoCodecs(codecs), true);
            },
            Qt::QueuedConnection);
    });
}

} // namespace JellyfinNative
