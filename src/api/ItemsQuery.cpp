#include "ItemsQuery.h"

#include <algorithm>

namespace JellyfinNative {

ItemsQuery& ItemsQuery::add(const QString& key, const QString& value)
{
    m_query.addQueryItem(key, value);
    return *this;
}

ItemsQuery& ItemsQuery::addIfNotEmpty(const QString& key, const QString& value)
{
    if (!value.isEmpty())
        m_query.addQueryItem(key, value);
    return *this;
}

ItemsQuery& ItemsQuery::userId(const QString& userId)
{
    return add(QStringLiteral("userId"), userId);
}

ItemsQuery& ItemsQuery::parentId(const QString& parentId)
{
    return addIfNotEmpty(QStringLiteral("parentId"), parentId);
}

ItemsQuery& ItemsQuery::recursive(bool enabled)
{
    return add(QStringLiteral("recursive"), enabled ? QStringLiteral("true") : QStringLiteral("false"));
}

ItemsQuery& ItemsQuery::includeItemTypes(const QString& types)
{
    return addIfNotEmpty(QStringLiteral("includeItemTypes"), types);
}

ItemsQuery& ItemsQuery::mediaTypes(const QString& types)
{
    return addIfNotEmpty(QStringLiteral("mediaTypes"), types);
}

ItemsQuery& ItemsQuery::fields(const QString& fields)
{
    return addIfNotEmpty(QStringLiteral("fields"), fields);
}

ItemsQuery& ItemsQuery::images(const QString& types, int imageTypeLimit)
{
    addIfNotEmpty(QStringLiteral("enableImageTypes"), types);
    add(QStringLiteral("imageTypeLimit"), QString::number(std::max(1, imageTypeLimit)));
    return *this;
}

ItemsQuery& ItemsQuery::sort(const QString& sortBy, const QString& sortOrder)
{
    addIfNotEmpty(QStringLiteral("sortBy"), sortBy);
    addIfNotEmpty(QStringLiteral("sortOrder"), sortOrder);
    return *this;
}

ItemsQuery& ItemsQuery::limit(int requested, int maximum)
{
    const int boundedMaximum = std::max(1, maximum);
    add(QStringLiteral("limit"), QString::number(std::clamp(requested, 1, boundedMaximum)));
    return *this;
}

ItemsQuery& ItemsQuery::startIndex(int startIndex)
{
    add(QStringLiteral("startIndex"), QString::number(std::max(0, startIndex)));
    return *this;
}

ItemsQuery& ItemsQuery::enableTotalRecordCount(bool enabled)
{
    return add(QStringLiteral("enableTotalRecordCount"), enabled ? QStringLiteral("true") : QStringLiteral("false"));
}

QUrlQuery ItemsQuery::toUrlQuery() const
{
    return m_query;
}

} // namespace JellyfinNative
