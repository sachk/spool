#include "api/ItemsQuery.h"

#include <QUrlQuery>

#include <cstdlib>
#include <iostream>

using namespace JellyfinNative;

namespace {

void fail(const char *message, const QUrlQuery& query)
{
    std::cerr << message << ": " << query.toString().toStdString() << '\n';
    std::exit(1);
}

void require(bool condition, const char *message, const QUrlQuery& query)
{
    if (condition)
        return;
    fail(message, query);
}

void requireValue(const QUrlQuery& query, const QString& key, const QString& expected, const char *message)
{
    require(query.hasQueryItem(key) && query.queryItemValue(key) == expected, message, query);
}

void requireMissing(const QUrlQuery& query, const QString& key, const char *message)
{
    require(!query.hasQueryItem(key), message, query);
}

} // namespace

int main()
{
    const QUrlQuery populated = ItemsQuery()
                                    .userId(QStringLiteral("user-1"))
                                    .parentId(QStringLiteral("parent-1"))
                                    .recursive(false)
                                    .includeItemTypes(QStringLiteral("Movie,Series"))
                                    .mediaTypes(QStringLiteral("Video"))
                                    .fields(QStringLiteral("PrimaryImageAspectRatio,Genres"))
                                    .toUrlQuery();
    requireValue(populated, QStringLiteral("userId"), QStringLiteral("user-1"), "user id should be encoded");
    requireValue(
        populated, QStringLiteral("parentId"), QStringLiteral("parent-1"), "non-empty parent id should be encoded");
    requireValue(
        populated, QStringLiteral("recursive"), QStringLiteral("false"), "recursive flag should preserve false");
    requireValue(populated, QStringLiteral("includeItemTypes"), QStringLiteral("Movie,Series"),
        "item type filter should be encoded");
    requireValue(
        populated, QStringLiteral("mediaTypes"), QStringLiteral("Video"), "media type filter should be encoded");
    requireValue(populated, QStringLiteral("fields"), QStringLiteral("PrimaryImageAspectRatio,Genres"),
        "field list should be encoded");

    const QUrlQuery emptyOptional = ItemsQuery()
                                        .parentId(QString())
                                        .includeItemTypes(QString())
                                        .mediaTypes(QString())
                                        .fields(QString())
                                        .sort(QString(), QString())
                                        .images(QString(), 4)
                                        .toUrlQuery();
    requireMissing(emptyOptional, QStringLiteral("parentId"), "empty parent id should be omitted");
    requireMissing(emptyOptional, QStringLiteral("includeItemTypes"), "empty item type filter should be omitted");
    requireMissing(emptyOptional, QStringLiteral("mediaTypes"), "empty media type filter should be omitted");
    requireMissing(emptyOptional, QStringLiteral("fields"), "empty field list should be omitted");
    requireMissing(emptyOptional, QStringLiteral("sortBy"), "empty sort field should be omitted");
    requireMissing(emptyOptional, QStringLiteral("sortOrder"), "empty sort order should be omitted");
    requireMissing(emptyOptional, QStringLiteral("enableImageTypes"), "empty image type list should be omitted");

    const QUrlQuery presentation = ItemsQuery()
                                       .fields(QStringLiteral("PrimaryImageAspectRatio,ChildCount"))
                                       .images(QStringLiteral("Primary,Backdrop"), 0)
                                       .sort(QStringLiteral("SortName"), QStringLiteral("Descending"))
                                       .toUrlQuery();
    requireValue(presentation, QStringLiteral("fields"), QStringLiteral("PrimaryImageAspectRatio,ChildCount"),
        "presentation fields should be encoded");
    requireValue(presentation, QStringLiteral("enableImageTypes"), QStringLiteral("Primary,Backdrop"),
        "requested image types should be encoded");
    requireValue(presentation, QStringLiteral("imageTypeLimit"), QStringLiteral("1"),
        "image type limit should be clamped to at least one");
    requireValue(presentation, QStringLiteral("sortBy"), QStringLiteral("SortName"), "sort field should be encoded");
    requireValue(
        presentation, QStringLiteral("sortOrder"), QStringLiteral("Descending"), "sort order should be encoded");

    const QUrlQuery underLimit = ItemsQuery().limit(0, 60).toUrlQuery();
    requireValue(underLimit, QStringLiteral("limit"), QStringLiteral("1"), "limit should be clamped to at least one");

    const QUrlQuery overLimit = ItemsQuery().limit(250, 60).toUrlQuery();
    requireValue(
        overLimit, QStringLiteral("limit"), QStringLiteral("60"), "limit should be clamped to the supplied maximum");

    const QUrlQuery withinLimit = ItemsQuery().limit(42, 60).toUrlQuery();
    requireValue(withinLimit, QStringLiteral("limit"), QStringLiteral("42"), "limit within bounds should be preserved");

    return 0;
}
