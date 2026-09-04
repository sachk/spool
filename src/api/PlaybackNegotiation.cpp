#include "PlaybackNegotiation.h"

#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace JellyfinNative {

namespace {

    constexpr auto kDirectPlay = "DirectPlay";
    constexpr auto kDirectStream = "DirectStream";
    constexpr auto kTranscode = "Transcode";
    const QStringList kHlsVideoCodecPreference { QStringLiteral("hevc"), QStringLiteral("h264"), QStringLiteral("av1"),
        QStringLiteral("vp9") };

    qint64 sourceBitrate(const QJsonObject& source)
    {
        qint64 bitrate = source.value(QStringLiteral("Bitrate")).toInteger();
        if (bitrate <= 0)
            bitrate = source.value(QStringLiteral("BitRate")).toInteger();
        return bitrate;
    }

    int methodRank(const QJsonObject& source, bool preferRemux, QString *method)
    {
        if (source.value(QStringLiteral("SupportsDirectPlay")).toBool()) {
            *method = QString::fromLatin1(kDirectPlay);
            return 3;
        }

        const bool directStream = source.value(QStringLiteral("SupportsDirectStream")).toBool();
        const bool transcode = !source.value(QStringLiteral("TranscodingUrl")).toString().isEmpty();
        if (preferRemux && directStream) {
            *method = QString::fromLatin1(kDirectStream);
            return 2;
        }
        if (transcode) {
            *method = QString::fromLatin1(kTranscode);
            return 2;
        }
        if (directStream) {
            *method = QString::fromLatin1(kDirectStream);
            return 1;
        }
        return -1;
    }

    QUrl serverRelativeUrl(const QString& serverUrl, const QString& path)
    {
        const QUrl candidate(path);
        if (!candidate.scheme().isEmpty() || !candidate.host().isEmpty())
            return candidate;

        QUrl result(serverUrl);
        QString basePath = result.path();
        while (basePath.endsWith(QLatin1Char('/')))
            basePath.chop(1);

        QString relativePath = candidate.path();
        if (!relativePath.startsWith(QLatin1Char('/')))
            relativePath.prepend(QLatin1Char('/'));
        if (!basePath.isEmpty() && basePath != QStringLiteral("/")
            && !relativePath.startsWith(basePath + QLatin1Char('/'))) {
            relativePath.prepend(basePath);
        }
        result.setPath(relativePath);
        result.setQuery(candidate.query());
        result.setFragment(candidate.fragment());
        return result;
    }
    QUrl videoStreamUrl(const QString& serverUrl, const QString& itemId, const QString& mediaSourceId, bool isStatic)
    {
        QUrl url(serverUrl);
        QString path = url.path();
        while (path.endsWith(QLatin1Char('/')))
            path.chop(1);
        url.setPath(QStringLiteral("%1/Videos/%2/stream").arg(path, itemId));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("static"), isStatic ? QStringLiteral("true") : QStringLiteral("false"));
        query.addQueryItem(QStringLiteral("MediaSourceId"), mediaSourceId);
        url.setQuery(query);
        return url;
    }

    QStringList normalizedCodecs(const QStringList& codecs)
    {
        QStringList result;
        result.reserve(codecs.size());
        for (const QString& codec : codecs) {
            const QString normalized = codec.trimmed().toLower();
            if (!normalized.isEmpty() && !result.contains(normalized))
                result.push_back(normalized);
        }
        return result;
    }

    QStringList transcodeVideoCodecs(const QStringList& directVideoCodecs, bool restrictVideoCodecs)
    {
        if (!restrictVideoCodecs)
            return kHlsVideoCodecPreference;

        const QStringList supported = normalizedCodecs(directVideoCodecs);
        QStringList result;
        for (const QString& preferred : kHlsVideoCodecPreference) {
            if (supported.contains(preferred))
                result.push_back(preferred);
        }
        return result;
    }

    QJsonArray localSubtitleProfiles()
    {
        const QStringList formats { QStringLiteral("srt"), QStringLiteral("ass"), QStringLiteral("ssa"),
            QStringLiteral("vtt"), QStringLiteral("pgssub"), QStringLiteral("dvdsub") };
        QJsonArray profiles;
        for (const QString& format : formats) {
            profiles.push_back(QJsonObject {
                { QStringLiteral("Format"), format },
                { QStringLiteral("Method"), QStringLiteral("Embed") },
            });
            profiles.push_back(QJsonObject {
                { QStringLiteral("Format"), format },
                { QStringLiteral("Method"), QStringLiteral("External") },
            });
        }
        return profiles;
    }

} // namespace

PlaybackSelection PlaybackNegotiation::selectSource(const QJsonArray& mediaSources, bool preferRemux)
{
    PlaybackSelection selected;
    int bestRank = -1;
    qint64 bestBitrate = -1;

    for (const QJsonValue& value : mediaSources) {
        const QJsonObject source = value.toObject();
        QString method;
        const int rank = methodRank(source, preferRemux, &method);
        const qint64 bitrate = sourceBitrate(source);
        if (rank > bestRank || (rank == bestRank && bitrate > bestBitrate)) {
            selected = { source, method };
            bestRank = rank;
            bestBitrate = bitrate;
        }
    }

    if (bestRank < 0 || selected.source.isEmpty())
        throw std::runtime_error("No playable media source available");
    return selected;
}

TrickplayInfo PlaybackNegotiation::selectTrickplay(
    const QJsonObject& trickplay, const QString& mediaSourceId, int preferredWidth)
{
    QJsonObject widths;
    const QJsonObject first
        = trickplay.constBegin() == trickplay.constEnd() ? QJsonObject {} : trickplay.constBegin().value().toObject();
    const bool widthMap = first.contains(QStringLiteral("Width")) || first.contains(QStringLiteral("TileWidth"))
        || first.contains(QStringLiteral("Interval"));
    if (widthMap) {
        widths = trickplay;
    } else if (!mediaSourceId.isEmpty() && trickplay.contains(mediaSourceId)) {
        widths = trickplay.value(mediaSourceId).toObject();
    } else if (!trickplay.isEmpty()) {
        widths = first;
    }

    TrickplayInfo best;
    int bestDiff = std::numeric_limits<int>::max();
    for (auto it = widths.constBegin(); it != widths.constEnd(); ++it) {
        const QJsonObject info = it.value().toObject();
        const int width = info.value(QStringLiteral("Width")).toInt();
        if (width <= 0)
            continue;
        const int diff = std::abs(width - preferredWidth);
        if (diff >= bestDiff)
            continue;
        bestDiff = diff;
        best.width = width;
        best.height = info.value(QStringLiteral("Height")).toInt();
        best.tileWidth = info.value(QStringLiteral("TileWidth")).toInt();
        best.tileHeight = info.value(QStringLiteral("TileHeight")).toInt();
        best.thumbnailCount = info.value(QStringLiteral("ThumbnailCount")).toInt();
        best.intervalMs = info.value(QStringLiteral("Interval")).toInt();
        best.bandwidth = info.value(QStringLiteral("Bandwidth")).toInt();
    }
    return best;
}

QString PlaybackNegotiation::buildUrl(
    const QString& serverUrl, const QString& itemId, const PlaybackSelection& selection)
{
    const QString mediaSourceId = selection.source.value(QStringLiteral("Id")).toString();
    if (mediaSourceId.isEmpty())
        throw std::runtime_error("Selected media source has no id");

    QUrl url;
    if (selection.playMethod == QString::fromLatin1(kDirectPlay)) {
        url = videoStreamUrl(serverUrl, itemId, mediaSourceId, true);
    } else {
        const QString responseUrl = selection.playMethod == QString::fromLatin1(kTranscode)
            ? selection.source.value(QStringLiteral("TranscodingUrl")).toString()
            : selection.source.value(QStringLiteral("DirectStreamUrl")).toString();
        if (!responseUrl.isEmpty()) {
            url = serverRelativeUrl(serverUrl, responseUrl);
        } else {
            url = videoStreamUrl(serverUrl, itemId, mediaSourceId, false);
        }
    }

    return url.toString(QUrl::FullyEncoded);
}

QJsonObject PlaybackNegotiation::buildDeviceProfile(
    qint64 maxStreamingBitrate, int maxHeight, const QStringList& videoCodecs, bool restrictVideoCodecs)
{
    const qint64 bitrate = std::clamp<qint64>(maxStreamingBitrate, 1'000'000, 1'000'000'000);
    QJsonArray codecProfiles;
    if (maxHeight > 0) {
        codecProfiles.push_back(QJsonObject {
            { QStringLiteral("Type"), QStringLiteral("Video") },
            { QStringLiteral("Conditions"),
                QJsonArray { QJsonObject {
                    { QStringLiteral("Condition"), QStringLiteral("LessThanEqual") },
                    { QStringLiteral("Property"), QStringLiteral("Height") },
                    { QStringLiteral("Value"), QString::number(maxHeight) },
                    // Not required: a source already below the ceiling should
                    // direct play rather than be transcoded up to it.
                    { QStringLiteral("IsRequired"), false },
                } } },
        });
    }
    QJsonArray directPlayProfiles;
    const QStringList normalizedVideoCodecs = normalizedCodecs(videoCodecs);
    if (!restrictVideoCodecs) {
        directPlayProfiles.push_back(QJsonObject { { QStringLiteral("Type"), QStringLiteral("Video") } });
    } else if (!normalizedVideoCodecs.isEmpty()) {
        directPlayProfiles.push_back(QJsonObject {
            { QStringLiteral("Type"), QStringLiteral("Video") },
            { QStringLiteral("VideoCodec"), normalizedVideoCodecs.join(QLatin1Char(',')) },
        });
    }
    // Audio is always decoded to PCM by mpv. Keep this unrestricted even on
    // webOS so an unsupported compressed-output format never forces a video
    // transcode merely because Jellyfin could not copy the audio stream.
    directPlayProfiles.push_back(QJsonObject { { QStringLiteral("Type"), QStringLiteral("Audio") } });

    QStringList outputVideoCodecs = transcodeVideoCodecs(normalizedVideoCodecs, restrictVideoCodecs);
    if (outputVideoCodecs.isEmpty())
        outputVideoCodecs.push_back(QStringLiteral("h264"));

    return {
        { QStringLiteral("Name"), QStringLiteral("JellyfinNative") },
        { QStringLiteral("MaxStreamingBitrate"), bitrate },
        { QStringLiteral("MaxStaticBitrate"), bitrate },
        { QStringLiteral("MusicStreamingTranscodingBitrate"), 1'280'000 },
        { QStringLiteral("DirectPlayProfiles"), directPlayProfiles },
        { QStringLiteral("TranscodingProfiles"),
            QJsonArray {
                QJsonObject {
                    { QStringLiteral("Type"), QStringLiteral("Video") },
                    { QStringLiteral("Container"), QStringLiteral("mp4") },
                    { QStringLiteral("Protocol"), QStringLiteral("hls") },
                    { QStringLiteral("AudioCodec"), QStringLiteral("aac,ac3,eac3,mp3,flac,opus,dts,truehd") },
                    { QStringLiteral("VideoCodec"), outputVideoCodecs.join(QLatin1Char(',')) },
                    { QStringLiteral("Context"), QStringLiteral("Streaming") },
                    { QStringLiteral("MaxAudioChannels"), QStringLiteral("6") },
                    { QStringLiteral("MinSegments"), 2 },
                    { QStringLiteral("BreakOnNonKeyFrames"), false },
                },
            } },
        { QStringLiteral("ContainerProfiles"), QJsonArray {} },
        { QStringLiteral("CodecProfiles"), codecProfiles },
        { QStringLiteral("SubtitleProfiles"), localSubtitleProfiles() },
        { QStringLiteral("ResponseProfiles"), QJsonArray {} },
    };
}

} // namespace JellyfinNative
