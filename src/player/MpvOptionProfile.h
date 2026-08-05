#pragma once

#include "../common/JellyfinTypes.h"
#include "../platform/MpvConfigPolicy.h"

#include <QByteArray>
#include <QString>

#include <vector>

namespace JellyfinNative {

struct MpvOption {
    QByteArray name;
    QByteArray value;
};

class MpvOptionProfile final {
public:
    enum class Platform {
        Desktop,
        WebOS,
    };

    struct NetworkProfile {
        int ringBytes;
        int rangeBytes;
        int parallelRequests;
    };

    static NetworkProfile networkProfile(Platform platform, int parallelRequests = 1);
    static bool isHdrPlayback(const QList<MediaStreamInfo>& streams);
    static bool isHdrTransfer(const QByteArray& transfer);
    static bool isHdrOutput(bool starfishOutput, bool hdrInput, const QByteArray& targetTransfer);
    static QByteArray preloadedSubtitleStreams(const PlaybackSession& session, const QString& preferredLanguage);
    static QByteArray loadFileOptions(const PlaybackSession& session);
    static bool useWebOSSoftwareVideo(const PlaybackSession& session);

    static std::vector<MpvOption> preInitializeOptions(const MpvConfigPolicy& policy);
    static std::vector<MpvOption> applicationOptions(Platform platform, const QString& audioOutputMode,
        const QByteArray& logPath, const QByteArray& demuxerMaxBytes = QByteArrayLiteral("64M"),
        const QByteArray& demuxerMaxBackBytes = QByteArrayLiteral("32M"), int parallelRequests = 1,
        bool softwareVideo = false, const QByteArray& shaderCachePath = {});
    static std::vector<MpvOption> subtitleOptions(
        const SubtitlePreferences& preferences, bool subtitlesEnabled, bool hdrPlayback = false);
};

} // namespace JellyfinNative
