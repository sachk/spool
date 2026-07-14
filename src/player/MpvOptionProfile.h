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

    static NetworkProfile networkProfile(Platform platform);

    static std::vector<MpvOption> startupOptions(Platform platform, const QString& audioOutputMode,
        const QByteArray& logPath, const QByteArray& demuxerMaxBytes = QByteArrayLiteral("64M"),
        const QByteArray& demuxerMaxBackBytes = QByteArrayLiteral("32M"));
    static std::vector<MpvOption> subtitleOptions(
        const SubtitlePreferences& preferences, bool subtitlesEnabled, bool hdrPlayback = false);
};

} // namespace JellyfinNative
