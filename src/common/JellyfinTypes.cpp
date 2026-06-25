#include "JellyfinTypes.h"

#include <QJsonValue>
#include <QRegularExpression>

#include <stdexcept>

namespace JellyfinNative {

namespace {

constexpr qint64 kTicksPerSecond = 10000000;

QJsonArray stringListToJsonArray(const QStringList &items)
{
    QJsonArray array;
    for (const QString &item : items)
        array.push_back(item);
    return array;
}

QStringList stringListFromJsonArray(const QJsonArray &array)
{
    QStringList items;
    items.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QString item = value.toString();
        if (!item.isEmpty())
            items.push_back(item);
    }
    return items;
}

QJsonArray peopleToJsonArray(const std::vector<PersonItem> &people)
{
    QJsonArray array;
    for (const PersonItem &person : people)
        array.push_back(toJson(person));
    return array;
}

std::vector<PersonItem> peopleFromJsonArray(const QJsonArray &array)
{
    std::vector<PersonItem> people;
    people.reserve(array.size());
    for (const QJsonValue &value : array) {
        PersonItem person = personFromJson(value.toObject());
        if (!person.id.isEmpty() || !person.name.isEmpty())
            people.push_back(person);
    }
    return people;
}

QJsonObject toJson(const MediaStreamInfo &stream)
{
    return {
        {QStringLiteral("index"), stream.index},
        {QStringLiteral("type"), stream.type},
        {QStringLiteral("codec"), stream.codec},
        {QStringLiteral("profile"), stream.profile},
        {QStringLiteral("displayTitle"), stream.displayTitle},
        {QStringLiteral("title"), stream.title},
        {QStringLiteral("language"), stream.language},
        {QStringLiteral("pixelFormat"), stream.pixelFormat},
        {QStringLiteral("videoRange"), stream.videoRange},
        {QStringLiteral("colorPrimaries"), stream.colorPrimaries},
        {QStringLiteral("colorTransfer"), stream.colorTransfer},
        {QStringLiteral("colorSpace"), stream.colorSpace},
        {QStringLiteral("aspectRatio"), stream.aspectRatio},
        {QStringLiteral("width"), stream.width},
        {QStringLiteral("height"), stream.height},
        {QStringLiteral("frameRate"), stream.frameRate},
        {QStringLiteral("bitRate"), stream.bitRate},
        {QStringLiteral("bitDepth"), stream.bitDepth},
        {QStringLiteral("channels"), stream.channels},
        {QStringLiteral("sampleRate"), stream.sampleRate},
        {QStringLiteral("isDefault"), stream.isDefault},
        {QStringLiteral("isForced"), stream.isForced},
        {QStringLiteral("isExternal"), stream.isExternal},
        {QStringLiteral("isInterlaced"), stream.isInterlaced},
    };
}

MediaStreamInfo mediaStreamFromJson(const QJsonObject &object)
{
    return {
        object.value(QStringLiteral("index")).toInt(-1),
        object.value(QStringLiteral("type")).toString(),
        object.value(QStringLiteral("codec")).toString(),
        object.value(QStringLiteral("profile")).toString(),
        object.value(QStringLiteral("displayTitle")).toString(),
        object.value(QStringLiteral("title")).toString(),
        object.value(QStringLiteral("language")).toString(),
        object.value(QStringLiteral("pixelFormat")).toString(),
        object.value(QStringLiteral("videoRange")).toString(),
        object.value(QStringLiteral("colorPrimaries")).toString(),
        object.value(QStringLiteral("colorTransfer")).toString(),
        object.value(QStringLiteral("colorSpace")).toString(),
        object.value(QStringLiteral("aspectRatio")).toString(),
        object.value(QStringLiteral("width")).toInt(),
        object.value(QStringLiteral("height")).toInt(),
        object.value(QStringLiteral("frameRate")).toDouble(),
        object.value(QStringLiteral("bitRate")).toInt(),
        object.value(QStringLiteral("bitDepth")).toInt(),
        object.value(QStringLiteral("channels")).toInt(),
        object.value(QStringLiteral("sampleRate")).toInt(),
        object.value(QStringLiteral("isDefault")).toBool(false),
        object.value(QStringLiteral("isForced")).toBool(false),
        object.value(QStringLiteral("isExternal")).toBool(false),
        object.value(QStringLiteral("isInterlaced")).toBool(false),
    };
}

QJsonArray mediaStreamsToJsonArray(const std::vector<MediaStreamInfo> &streams)
{
    QJsonArray array;
    for (const MediaStreamInfo &stream : streams)
        array.push_back(toJson(stream));
    return array;
}

std::vector<MediaStreamInfo> mediaStreamsFromJsonArray(const QJsonArray &array)
{
    std::vector<MediaStreamInfo> streams;
    streams.reserve(array.size());
    for (const QJsonValue &value : array) {
        MediaStreamInfo stream = mediaStreamFromJson(value.toObject());
        if (!stream.type.isEmpty() || !stream.codec.isEmpty())
            streams.push_back(stream);
    }
    return streams;
}

QJsonObject toJson(const MediaSourceInfo &source)
{
    return {
        {QStringLiteral("id"), source.id},
        {QStringLiteral("name"), source.name},
        {QStringLiteral("path"), source.path},
        {QStringLiteral("container"), source.container},
        {QStringLiteral("protocol"), source.protocol},
        {QStringLiteral("videoType"), source.videoType},
        {QStringLiteral("size"), QString::number(source.size)},
        {QStringLiteral("bitRate"), source.bitRate},
        {QStringLiteral("runtimeTicks"), QString::number(source.runtimeTicks)},
        {QStringLiteral("streams"), mediaStreamsToJsonArray(source.streams)},
    };
}

MediaSourceInfo mediaSourceFromJson(const QJsonObject &object)
{
    return {
        object.value(QStringLiteral("id")).toString(),
        object.value(QStringLiteral("name")).toString(),
        object.value(QStringLiteral("path")).toString(),
        object.value(QStringLiteral("container")).toString(),
        object.value(QStringLiteral("protocol")).toString(),
        object.value(QStringLiteral("videoType")).toString(),
        object.value(QStringLiteral("size")).toVariant().toLongLong(),
        object.value(QStringLiteral("bitRate")).toInt(),
        object.value(QStringLiteral("runtimeTicks")).toVariant().toLongLong(),
        mediaStreamsFromJsonArray(object.value(QStringLiteral("streams")).toArray()),
    };
}

QJsonArray mediaSourcesToJsonArray(const std::vector<MediaSourceInfo> &sources)
{
    QJsonArray array;
    for (const MediaSourceInfo &source : sources)
        array.push_back(toJson(source));
    return array;
}

std::vector<MediaSourceInfo> mediaSourcesFromJsonArray(const QJsonArray &array)
{
    std::vector<MediaSourceInfo> sources;
    sources.reserve(array.size());
    for (const QJsonValue &value : array) {
        MediaSourceInfo source = mediaSourceFromJson(value.toObject());
        if (!source.id.isEmpty() || !source.container.isEmpty() || !source.streams.empty())
            sources.push_back(source);
    }
    return sources;
}

}

QJsonObject toJson(const DiscoveredServer &server)
{
    return {
        {QStringLiteral("id"), server.id},
        {QStringLiteral("name"), server.name},
        {QStringLiteral("address"), server.address},
    };
}

QJsonObject toJson(const LibraryItem &library)
{
    return {
        {QStringLiteral("id"), library.id},
        {QStringLiteral("name"), library.name},
        {QStringLiteral("collectionType"), library.collectionType},
        {QStringLiteral("imageUrl"), library.imageUrl},
        {QStringLiteral("imageTag"), library.imageTag},
    };
}

QJsonObject toJson(const PersonItem &person)
{
    return {
        {QStringLiteral("id"), person.id},
        {QStringLiteral("name"), person.name},
        {QStringLiteral("type"), person.type},
        {QStringLiteral("role"), person.role},
        {QStringLiteral("imageUrl"), person.imageUrl},
        {QStringLiteral("imageTag"), person.imageTag},
    };
}

QJsonObject toJson(const MovieItem &movie)
{
    return {
        {QStringLiteral("id"), movie.id},
        {QStringLiteral("title"), movie.title},
        {QStringLiteral("overview"), movie.overview},
        {QStringLiteral("posterUrl"), movie.posterUrl},
        {QStringLiteral("posterTag"), movie.posterTag},
        {QStringLiteral("itemType"), movie.itemType},
        {QStringLiteral("seriesId"), movie.seriesId},
        {QStringLiteral("seasonId"), movie.seasonId},
        {QStringLiteral("seriesName"), movie.seriesName},
        {QStringLiteral("seriesPosterUrl"), movie.seriesPosterUrl},
        {QStringLiteral("subtitle"), movie.subtitle},
        {QStringLiteral("path"), movie.path},
        {QStringLiteral("year"), movie.year},
        {QStringLiteral("seasonNumber"), movie.seasonNumber},
        {QStringLiteral("episodeNumber"), movie.episodeNumber},
        {QStringLiteral("resumeTicks"), QString::number(movie.resumeTicks)},
        {QStringLiteral("runtimeTicks"), QString::number(movie.runtimeTicks)},
        {QStringLiteral("playable"), movie.playable},
        {QStringLiteral("favorite"), movie.favorite},
        {QStringLiteral("played"), movie.played},
        {QStringLiteral("backdropUrl"), movie.backdropUrl},
        {QStringLiteral("logoUrl"), movie.logoUrl},
        {QStringLiteral("bannerUrl"), movie.bannerUrl},
        {QStringLiteral("thumbUrl"), movie.thumbUrl},
        {QStringLiteral("landscapeCardUrl"), movie.landscapeCardUrl},
        {QStringLiteral("genres"), stringListToJsonArray(movie.genres)},
        {QStringLiteral("tags"), stringListToJsonArray(movie.tags)},
        {QStringLiteral("studios"), stringListToJsonArray(movie.studios)},
        {QStringLiteral("officialRating"), movie.officialRating},
        {QStringLiteral("communityRating"), movie.communityRating},
        {QStringLiteral("criticRating"), movie.criticRating},
        {QStringLiteral("premiereDate"), movie.premiereDate},
        {QStringLiteral("endDate"), movie.endDate},
        {QStringLiteral("people"), peopleToJsonArray(movie.people)},
        {QStringLiteral("mediaSources"), mediaSourcesToJsonArray(movie.mediaSources)},
    };
}

DiscoveredServer discoveredServerFromJson(const QJsonObject &object)
{
    return {
        object.value(QStringLiteral("id")).toString(),
        object.value(QStringLiteral("name")).toString(),
        object.value(QStringLiteral("address")).toString(),
    };
}

LibraryItem libraryFromJson(const QJsonObject &object)
{
    return {
        object.value(QStringLiteral("id")).toString(),
        object.value(QStringLiteral("name")).toString(),
        object.value(QStringLiteral("collectionType")).toString(),
        object.value(QStringLiteral("imageUrl")).toString(),
        object.value(QStringLiteral("imageTag")).toString(),
    };
}

PersonItem personFromJson(const QJsonObject &object)
{
    return {
        object.value(QStringLiteral("id")).toString(),
        object.value(QStringLiteral("name")).toString(),
        object.value(QStringLiteral("type")).toString(),
        object.value(QStringLiteral("role")).toString(),
        object.value(QStringLiteral("imageUrl")).toString(),
        object.value(QStringLiteral("imageTag")).toString(),
    };
}

MovieItem movieFromJson(const QJsonObject &object)
{
    return {
        object.value(QStringLiteral("id")).toString(),
        object.value(QStringLiteral("title")).toString(),
        object.value(QStringLiteral("overview")).toString(),
        object.value(QStringLiteral("posterUrl")).toString(),
        object.value(QStringLiteral("posterTag")).toString(),
        object.value(QStringLiteral("itemType")).toString(QStringLiteral("Movie")),
        object.value(QStringLiteral("seriesId")).toString(),
        object.value(QStringLiteral("seasonId")).toString(),
        object.value(QStringLiteral("seriesName")).toString(),
        object.value(QStringLiteral("seriesPosterUrl")).toString(),
        object.value(QStringLiteral("subtitle")).toString(),
        object.value(QStringLiteral("path")).toString(),
        object.value(QStringLiteral("year")).toInt(),
        object.value(QStringLiteral("seasonNumber")).toInt(),
        object.value(QStringLiteral("episodeNumber")).toInt(),
        object.value(QStringLiteral("resumeTicks")).toVariant().toLongLong(),
        object.value(QStringLiteral("runtimeTicks")).toVariant().toLongLong(),
        object.value(QStringLiteral("playable")).toBool(true),
        object.value(QStringLiteral("favorite")).toBool(false),
        object.value(QStringLiteral("played")).toBool(false),
        object.value(QStringLiteral("backdropUrl")).toString(),
        object.value(QStringLiteral("logoUrl")).toString(),
        object.value(QStringLiteral("bannerUrl")).toString(),
        object.value(QStringLiteral("thumbUrl")).toString(),
        object.value(QStringLiteral("landscapeCardUrl")).toString(),
        stringListFromJsonArray(object.value(QStringLiteral("genres")).toArray()),
        stringListFromJsonArray(object.value(QStringLiteral("tags")).toArray()),
        stringListFromJsonArray(object.value(QStringLiteral("studios")).toArray()),
        object.value(QStringLiteral("officialRating")).toString(),
        object.value(QStringLiteral("communityRating")).toDouble(),
        object.value(QStringLiteral("criticRating")).toDouble(),
        object.value(QStringLiteral("premiereDate")).toString(),
        object.value(QStringLiteral("endDate")).toString(),
        peopleFromJsonArray(object.value(QStringLiteral("people")).toArray()),
        mediaSourcesFromJsonArray(object.value(QStringLiteral("mediaSources")).toArray()),
    };
}

QString exceptionMessage(const std::exception_ptr &exception)
{
    if (!exception)
        return QStringLiteral("Unknown error");

    try {
        std::rethrow_exception(exception);
    } catch (const std::exception &error) {
        return QString::fromUtf8(error.what());
    } catch (...) {
        return QStringLiteral("Unknown error");
    }
}

QString normalizedAudioOutputMode(const QString &mode)
{
    return (mode == QStringLiteral("starfish") || mode == QStringLiteral("starfish-pcm"))
        ? QStringLiteral("starfish-pcm")
        : QStringLiteral("alsa");
}

QString sanitizedDiagnosticUrl(QString url, qsizetype maxLength)
{
    static const QRegularExpression secretQuery(QStringLiteral("([?&](?:api_key|access_token|token)=)[^&]+"),
                                                QRegularExpression::CaseInsensitiveOption);
    url.replace(secretQuery, QStringLiteral("\\1<redacted>"));
    return maxLength >= 0 ? url.left(maxLength) : url;
}

bool isMeaningfulResumePosition(qint64 resumeTicks, qint64 runtimeTicks)
{
    if (resumeTicks < 5 * kTicksPerSecond)
        return false;
    if (runtimeTicks <= 0)
        return true;

    const qint64 remainingTicks = runtimeTicks - resumeTicks;
    return resumeTicks < runtimeTicks &&
           resumeTicks * 100 < runtimeTicks * 95 &&
           remainingTicks > 30 * kTicksPerSecond;
}

qint64 normalizedResumeTicks(qint64 resumeTicks, qint64 runtimeTicks)
{
    return isMeaningfulResumePosition(resumeTicks, runtimeTicks) ? resumeTicks : 0;
}

} // namespace JellyfinNative
