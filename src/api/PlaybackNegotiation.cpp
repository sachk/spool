#include "PlaybackNegotiation.h"

#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <stdexcept>

namespace JellyfinNative {

namespace {

constexpr auto kDirectPlay = "DirectPlay";
constexpr auto kDirectStream = "DirectStream";
constexpr auto kTranscode = "Transcode";

qint64 sourceBitrate(const QJsonObject &source)
{
    qint64 bitrate = source.value(QStringLiteral("Bitrate")).toInteger();
    if (bitrate <= 0)
        bitrate = source.value(QStringLiteral("BitRate")).toInteger();
    return bitrate;
}

int methodRank(const QJsonObject &source, bool preferRemux, QString *method)
{
    if (source.value(QStringLiteral("SupportsDirectPlay")).toBool()) {
        *method = QString::fromLatin1(kDirectPlay);
        return 3;
    }

    const bool directStream =
        source.value(QStringLiteral("SupportsDirectStream")).toBool();
    const bool transcode =
        !source.value(QStringLiteral("TranscodingUrl")).toString().isEmpty();
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

QUrl serverRelativeUrl(const QString &serverUrl, const QString &path)
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
    if (!basePath.isEmpty() && basePath != QStringLiteral("/") &&
        !relativePath.startsWith(basePath + QLatin1Char('/'))) {
        relativePath.prepend(basePath);
    }
    result.setPath(relativePath);
    result.setQuery(candidate.query());
    result.setFragment(candidate.fragment());
    return result;
}

void addAccessToken(QUrl *url, const QString &accessToken)
{
    QUrlQuery query(*url);
    if (!accessToken.isEmpty() &&
        !query.hasQueryItem(QStringLiteral("api_key"))) {
        query.addQueryItem(QStringLiteral("api_key"), accessToken);
        url->setQuery(query);
    }
}

} // namespace

PlaybackSelection PlaybackNegotiation::selectSource(
    const QJsonArray &mediaSources, bool preferRemux)
{
    PlaybackSelection selected;
    int bestRank = -1;
    qint64 bestBitrate = -1;

    for (const QJsonValue &value : mediaSources) {
        const QJsonObject source = value.toObject();
        QString method;
        const int rank = methodRank(source, preferRemux, &method);
        const qint64 bitrate = sourceBitrate(source);
        if (rank > bestRank || (rank == bestRank && bitrate > bestBitrate)) {
            selected = {source, method};
            bestRank = rank;
            bestBitrate = bitrate;
        }
    }

    if (bestRank < 0 || selected.source.isEmpty())
        throw std::runtime_error("No playable media source available");
    return selected;
}

QString PlaybackNegotiation::buildUrl(
    const QString &serverUrl, const QString &itemId,
    const QString &accessToken, const PlaybackSelection &selection)
{
    const QString mediaSourceId =
        selection.source.value(QStringLiteral("Id")).toString();
    if (mediaSourceId.isEmpty())
        throw std::runtime_error("Selected media source has no id");

    QUrl url;
    if (selection.playMethod == QString::fromLatin1(kDirectPlay)) {
        url = QUrl(serverUrl);
        QString path = url.path();
        while (path.endsWith(QLatin1Char('/')))
            path.chop(1);
        url.setPath(QStringLiteral("%1/Videos/%2/stream").arg(path, itemId));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("static"), QStringLiteral("true"));
        query.addQueryItem(QStringLiteral("MediaSourceId"), mediaSourceId);
        url.setQuery(query);
    } else {
        const QString responseUrl =
            selection.playMethod == QString::fromLatin1(kTranscode)
                ? selection.source.value(QStringLiteral("TranscodingUrl"))
                      .toString()
                : selection.source.value(QStringLiteral("DirectStreamUrl"))
                      .toString();
        if (!responseUrl.isEmpty()) {
            url = serverRelativeUrl(serverUrl, responseUrl);
        } else {
            url = QUrl(serverUrl);
            QString path = url.path();
            while (path.endsWith(QLatin1Char('/')))
                path.chop(1);
            url.setPath(QStringLiteral("%1/Videos/%2/stream").arg(path, itemId));
            QUrlQuery query;
            query.addQueryItem(QStringLiteral("static"),
                               QStringLiteral("false"));
            query.addQueryItem(QStringLiteral("MediaSourceId"), mediaSourceId);
            url.setQuery(query);
        }
    }

    addAccessToken(&url, accessToken);
    return url.toString(QUrl::FullyEncoded);
}

QJsonObject PlaybackNegotiation::buildDeviceProfile(
    qint64 maxStreamingBitrate)
{
    const qint64 bitrate = std::clamp<qint64>(
        maxStreamingBitrate, 1'000'000, 1'000'000'000);
    return {
        {QStringLiteral("Name"), QStringLiteral("JellyfinNative")},
        {QStringLiteral("MaxStreamingBitrate"), bitrate},
        {QStringLiteral("MaxStaticBitrate"), bitrate},
        {QStringLiteral("MusicStreamingTranscodingBitrate"), 1'280'000},
        {QStringLiteral("DirectPlayProfiles"),
         QJsonArray{
             QJsonObject{{QStringLiteral("Type"), QStringLiteral("Video")}},
             QJsonObject{{QStringLiteral("Type"), QStringLiteral("Audio")}},
         }},
        {QStringLiteral("TranscodingProfiles"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("Type"), QStringLiteral("Video")},
                 {QStringLiteral("Container"), QStringLiteral("ts")},
                 {QStringLiteral("Protocol"), QStringLiteral("hls")},
                 {QStringLiteral("AudioCodec"), QStringLiteral("aac,mp3,ac3,eac3")},
                 {QStringLiteral("VideoCodec"), QStringLiteral("h264")},
                 {QStringLiteral("Context"), QStringLiteral("Streaming")},
                 {QStringLiteral("MaxAudioChannels"), QStringLiteral("6")},
                 {QStringLiteral("MinSegments"), 2},
                 {QStringLiteral("BreakOnNonKeyFrames"), true},
             },
         }},
        {QStringLiteral("ContainerProfiles"), QJsonArray{}},
        {QStringLiteral("CodecProfiles"), QJsonArray{}},
        {QStringLiteral("SubtitleProfiles"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("Format"), QStringLiteral("srt")},
                 {QStringLiteral("Method"), QStringLiteral("External")},
             },
             QJsonObject{
                 {QStringLiteral("Format"), QStringLiteral("ass")},
                 {QStringLiteral("Method"), QStringLiteral("External")},
             },
             QJsonObject{
                 {QStringLiteral("Format"), QStringLiteral("ssa")},
                 {QStringLiteral("Method"), QStringLiteral("External")},
             },
             QJsonObject{
                 {QStringLiteral("Format"), QStringLiteral("vtt")},
                 {QStringLiteral("Method"), QStringLiteral("External")},
             },
             QJsonObject{
                 {QStringLiteral("Format"), QStringLiteral("pgssub")},
                 {QStringLiteral("Method"), QStringLiteral("Encode")},
             },
             QJsonObject{
                 {QStringLiteral("Format"), QStringLiteral("dvdsub")},
                 {QStringLiteral("Method"), QStringLiteral("Encode")},
             },
         }},
        {QStringLiteral("ResponseProfiles"), QJsonArray{}},
    };
}

} // namespace JellyfinNative
