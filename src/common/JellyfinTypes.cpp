#include "JellyfinTypes.h"

#include <QJsonValue>

#include <stdexcept>

namespace JellyfinNative {

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
        {QStringLiteral("year"), movie.year},
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
        object.value(QStringLiteral("year")).toInt(),
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
