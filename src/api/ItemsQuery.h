#pragma once

#include <QString>
#include <QUrlQuery>

namespace JellyfinNative {

class ItemsQuery final {
public:
    ItemsQuery() = default;

    ItemsQuery& add(const QString& key, const QString& value);
    ItemsQuery& addIfNotEmpty(const QString& key, const QString& value);
    ItemsQuery& userId(const QString& userId);
    ItemsQuery& parentId(const QString& parentId);
    ItemsQuery& recursive(bool enabled = true);
    ItemsQuery& includeItemTypes(const QString& types);
    ItemsQuery& mediaTypes(const QString& types);
    ItemsQuery& fields(const QString& fields);
    ItemsQuery& images(
        const QString& types = QStringLiteral("Primary,Backdrop,Logo,Banner,Thumb"), int imageTypeLimit = 3);
    ItemsQuery& sort(const QString& sortBy, const QString& sortOrder = QStringLiteral("Ascending"));
    ItemsQuery& limit(int requested, int maximum);
    ItemsQuery& startIndex(int startIndex);
    ItemsQuery& enableTotalRecordCount(bool enabled);

    QUrlQuery toUrlQuery() const;

private:
    QUrlQuery m_query;
};

} // namespace JellyfinNative
