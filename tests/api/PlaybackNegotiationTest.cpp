#include "api/PlaybackNegotiation.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>

#include <cstdlib>
#include <iostream>

using namespace JellyfinNative;

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << message << '\n';
    std::exit(1);
}

QJsonObject source(const QString& id, qint64 bitrate, bool directPlay, bool directStream,
    const QString& directStreamUrl = {}, const QString& transcodingUrl = {})
{
    return {
        { QStringLiteral("Id"), id },
        { QStringLiteral("Bitrate"), bitrate },
        { QStringLiteral("SupportsDirectPlay"), directPlay },
        { QStringLiteral("SupportsDirectStream"), directStream },
        { QStringLiteral("DirectStreamUrl"), directStreamUrl },
        { QStringLiteral("TranscodingUrl"), transcodingUrl },
    };
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const QJsonArray sources {
        source(QStringLiteral("transcode"), 80'000'000, false, false, {},
            QStringLiteral("/Videos/item/master.m3u8?MediaSourceId=transcode")),
        source(QStringLiteral("direct-low"), 10'000'000, true, true),
        source(QStringLiteral("direct-high"), 20'000'000, true, true),
    };
    const PlaybackSelection direct = PlaybackNegotiation::selectSource(sources, true);
    require(direct.playMethod == QStringLiteral("DirectPlay"), "direct play should outrank fallback methods");
    require(direct.source.value(QStringLiteral("Id")).toString() == QStringLiteral("direct-high"),
        "the highest-bitrate source should win within a method");

    const QJsonArray fallbackSources {
        source(QStringLiteral("fallback"), 12'000'000, false, true,
            QStringLiteral("/Videos/item/stream?MediaSourceId=fallback"),
            QStringLiteral("/Videos/item/master.m3u8?MediaSourceId=fallback")),
    };
    const PlaybackSelection remux = PlaybackNegotiation::selectSource(fallbackSources, true);
    require(remux.playMethod == QStringLiteral("DirectStream"), "remux preference should select direct stream");
    const PlaybackSelection transcode = PlaybackNegotiation::selectSource(fallbackSources, false);
    require(transcode.playMethod == QStringLiteral("Transcode"), "disabling remux preference should select transcode");

    const QString url = PlaybackNegotiation::buildUrl(
        QStringLiteral("https://example.test/jellyfin"), QStringLiteral("item"), transcode);
    const QUrl parsed(url);
    if (parsed.path() != QStringLiteral("/jellyfin/Videos/item/master.m3u8"))
        std::cerr << "resolved playback URL: " << url.toStdString() << '\n';
    require(parsed.path() == QStringLiteral("/jellyfin/Videos/item/master.m3u8"),
        "relative playback URLs should retain the server base path");
    require(
        !QUrlQuery(parsed).hasQueryItem(QStringLiteral("api_key")), "playback URLs should not include access tokens");

    const QJsonObject profile = PlaybackNegotiation::buildDeviceProfile(25'000'000);
    require(profile.value(QStringLiteral("MaxStreamingBitrate")).toInteger() == 25'000'000,
        "device profile should carry the configured bitrate");
    require(!profile.value(QStringLiteral("TranscodingProfiles")).toArray().isEmpty(),
        "device profile should advertise a transcode fallback");
    return 0;
}
