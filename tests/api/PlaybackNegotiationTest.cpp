#include "api/PlaybackNegotiation.h"

#include "TestMain.h"

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

JELLYFIN_TEST_MAIN("playback-negotiation")
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
    const QUrlQuery playbackQuery(parsed);
    require(!playbackQuery.hasQueryItem(QStringLiteral("api_key")),
        "playback URLs must not expose the access token in diagnostics or mpv logs");

    const QJsonObject smallTrickplay {
        { QStringLiteral("Width"), 160 },
        { QStringLiteral("Height"), 90 },
        { QStringLiteral("TileWidth"), 10 },
        { QStringLiteral("TileHeight"), 10 },
        { QStringLiteral("ThumbnailCount"), 123 },
        { QStringLiteral("Interval"), 10'000 },
    };
    const QJsonObject preferredTrickplay {
        { QStringLiteral("Width"), 320 },
        { QStringLiteral("Height"), 180 },
        { QStringLiteral("TileWidth"), 5 },
        { QStringLiteral("TileHeight"), 5 },
        { QStringLiteral("ThumbnailCount"), 50 },
        { QStringLiteral("Interval"), 5'000 },
    };
    const QJsonObject itemTrickplay {
        { QStringLiteral("media-source"),
            QJsonObject {
                { QStringLiteral("160"), smallTrickplay },
                { QStringLiteral("320"), preferredTrickplay },
            } },
    };
    const TrickplayInfo trickplay
        = PlaybackNegotiation::selectTrickplay(itemTrickplay, QStringLiteral("media-source"), 320);
    require(trickplay.width == 320 && trickplay.height == 180,
        "item trickplay metadata should select the closest requested width");
    require(trickplay.tileWidth == 5 && trickplay.tileHeight == 5 && trickplay.intervalMs == 5'000,
        "item trickplay metadata should retain sprite layout and interval");

    const QJsonObject profile = PlaybackNegotiation::buildDeviceProfile(25'000'000);
    require(profile.value(QStringLiteral("MaxStreamingBitrate")).toInteger() == 25'000'000,
        "device profile should carry the configured bitrate");
    require(!profile.value(QStringLiteral("TranscodingProfiles")).toArray().isEmpty(),
        "device profile should advertise a transcode fallback");
    const QJsonObject transcodeProfile
        = profile.value(QStringLiteral("TranscodingProfiles")).toArray().first().toObject();
    require(transcodeProfile.value(QStringLiteral("Container")).toString() == QStringLiteral("mp4"),
        "video transcodes should use fragmented MP4 HLS segments");
    require(transcodeProfile.value(QStringLiteral("Protocol")).toString() == QStringLiteral("hls"),
        "fragmented MP4 transcodes should retain HLS delivery");
    require(!transcodeProfile.value(QStringLiteral("BreakOnNonKeyFrames")).toBool(),
        "fragmented MP4 segments should only break on keyframes");
    bool foundSrtExternal = false;
    bool foundPgsExternal = false;
    bool foundSsaExternal = false;
    const QJsonArray subtitleProfiles = profile.value(QStringLiteral("SubtitleProfiles")).toArray();
    for (const QJsonValue& value : subtitleProfiles) {
        const QJsonObject subtitleProfile = value.toObject();
        const QString format = subtitleProfile.value(QStringLiteral("Format")).toString();
        const QString method = subtitleProfile.value(QStringLiteral("Method")).toString();
        foundSrtExternal |= format == QStringLiteral("srt") && method == QStringLiteral("External");
        foundPgsExternal |= format == QStringLiteral("pgssub") && method == QStringLiteral("External");
        foundSsaExternal |= format == QStringLiteral("ssa") && method == QStringLiteral("External");
    }
    require(foundSrtExternal && foundPgsExternal && foundSsaExternal,
        "device profile should advertise all locally rendered subtitle formats");

    const QJsonObject desktopProfile = PlaybackNegotiation::buildDeviceProfile(25'000'000);
    const QJsonObject desktopDirectVideo
        = desktopProfile.value(QStringLiteral("DirectPlayProfiles")).toArray().first().toObject();
    require(!desktopDirectVideo.contains(QStringLiteral("VideoCodec")),
        "desktop playback should remain unrestricted for software decoding");
    require(desktopProfile.value(QStringLiteral("TranscodingProfiles"))
                .toArray()
                .first()
                .toObject()
                .value(QStringLiteral("VideoCodec"))
                .toString()
            == QStringLiteral("hevc,h264,av1,vp9"),
        "desktop transcodes should advertise every fMP4 HLS video codec in preference order");

    const QJsonObject webOsProfile = PlaybackNegotiation::buildDeviceProfile(25'000'000,
        { QStringLiteral("VP9"), QStringLiteral("h264"), QStringLiteral("hevc"), QStringLiteral("mpeg1video"),
            QStringLiteral("mpeg2video"), QStringLiteral("mpeg4"), QStringLiteral("h263"), QStringLiteral("vc1") },
        true);
    const QJsonObject webOsDirectVideo
        = webOsProfile.value(QStringLiteral("DirectPlayProfiles")).toArray().first().toObject();
    require(webOsDirectVideo.value(QStringLiteral("VideoCodec")).toString()
            == QStringLiteral("vp9,h264,hevc,mpeg1video,mpeg2video,mpeg4,h263,vc1"),
        "webOS direct play should combine probed Starfish codecs with the explicit software-decoder set");
    require(webOsProfile.value(QStringLiteral("TranscodingProfiles"))
                .toArray()
                .first()
                .toObject()
                .value(QStringLiteral("VideoCodec"))
                .toString()
            == QStringLiteral("hevc,h264,vp9"),
        "webOS transcode outputs should intersect the Starfish codecs with the HLS preference order");
    return 0;
}
