#pragma once

#include "../common/JellyfinTypes.h"

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
    static QByteArray preloadedSubtitleStreams(const PlaybackSession& session, const QString& preferredLanguage);
    static QByteArray loadFileOptions(const PlaybackSession& session);

    static std::vector<MpvOption> startupOptions(Platform platform, const QString& audioOutputMode,
        const QByteArray& logPath, const QByteArray& demuxerMaxBytes = QByteArrayLiteral("64M"),
        const QByteArray& demuxerMaxBackBytes = QByteArrayLiteral("32M"), int parallelRequests = 1);
    static std::vector<MpvOption> subtitleOptions(
        const SubtitlePreferences& preferences, bool subtitlesEnabled, bool hdrPlayback = false);
};

} // namespace JellyfinNative
