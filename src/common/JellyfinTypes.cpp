#include "JellyfinTypes.h"

#include <QJsonValue>

#include <stdexcept>

namespace JellyfinNative {

namespace {

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
        {QStringLiteral("seriesName"), movie.seriesName},
        {QStringLiteral("subtitle"), movie.subtitle},
        {QStringLiteral("path"), movie.path},
        {QStringLiteral("year"), movie.year},
        {QStringLiteral("seasonNumber"), movie.seasonNumber},
        {QStringLiteral("episodeNumber"), movie.episodeNumber},
        {QStringLiteral("resumeTicks"), QString::number(movie.resumeTicks)},
        {QStringLiteral("runtimeTicks"), QString::number(movie.runtimeTicks)},
        {QStringLiteral("playable"), movie.playable},
        {QStringLiteral("backdropUrl"), movie.backdropUrl},
        {QStringLiteral("logoUrl"), movie.logoUrl},
        {QStringLiteral("bannerUrl"), movie.bannerUrl},
        {QStringLiteral("thumbUrl"), movie.thumbUrl},
        {QStringLiteral("genres"), stringListToJsonArray(movie.genres)},
        {QStringLiteral("tags"), stringListToJsonArray(movie.tags)},
        {QStringLiteral("studios"), stringListToJsonArray(movie.studios)},
        {QStringLiteral("officialRating"), movie.officialRating},
        {QStringLiteral("communityRating"), movie.communityRating},
        {QStringLiteral("criticRating"), movie.criticRating},
        {QStringLiteral("premiereDate"), movie.premiereDate},
        {QStringLiteral("endDate"), movie.endDate},
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
        object.value(QStringLiteral("seriesName")).toString(),
        object.value(QStringLiteral("subtitle")).toString(),
        object.value(QStringLiteral("path")).toString(),
        object.value(QStringLiteral("year")).toInt(),
        object.value(QStringLiteral("seasonNumber")).toInt(),
        object.value(QStringLiteral("episodeNumber")).toInt(),
        object.value(QStringLiteral("resumeTicks")).toVariant().toLongLong(),
        object.value(QStringLiteral("runtimeTicks")).toVariant().toLongLong(),
        object.value(QStringLiteral("playable")).toBool(true),
        object.value(QStringLiteral("backdropUrl")).toString(),
        object.value(QStringLiteral("logoUrl")).toString(),
        object.value(QStringLiteral("bannerUrl")).toString(),
        object.value(QStringLiteral("thumbUrl")).toString(),
        stringListFromJsonArray(object.value(QStringLiteral("genres")).toArray()),
        stringListFromJsonArray(object.value(QStringLiteral("tags")).toArray()),
        stringListFromJsonArray(object.value(QStringLiteral("studios")).toArray()),
        object.value(QStringLiteral("officialRating")).toString(),
        object.value(QStringLiteral("communityRating")).toDouble(),
        object.value(QStringLiteral("criticRating")).toDouble(),
        object.value(QStringLiteral("premiereDate")).toString(),
        object.value(QStringLiteral("endDate")).toString(),
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

} // namespace JellyfinNative
