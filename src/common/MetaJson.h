#pragma once

#include "JellyfinTypes.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaProperty>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace JellyfinNative {

enum class MetaJsonKeyPolicy {
    CamelCase,
    PascalCase,
};

namespace MetaJsonDetail {

inline QString jsonKey(const char *name, MetaJsonKeyPolicy policy)
{
    QString key = QString::fromLatin1(name);
    if (policy == MetaJsonKeyPolicy::PascalCase && !key.isEmpty())
        key[0] = key[0].toUpper();
    return key;
}

inline QJsonValue objectValueForProperty(const QJsonObject &object, const char *name,
                                         MetaJsonKeyPolicy policy)
{
    const QString key = jsonKey(name, policy);
    QJsonValue value = object.value(key);
    if (!value.isUndefined())
        return value;
    if (policy == MetaJsonKeyPolicy::PascalCase && key == QStringLiteral("RuntimeTicks"))
        return object.value(QStringLiteral("RunTimeTicks"));
    return {};
}

inline QJsonArray stringListToJson(const QStringList &items)
{
    QJsonArray array;
    for (const QString &item : items)
        array.push_back(item);
    return array;
}

inline QStringList stringListFromJson(const QJsonValue &value)
{
    QStringList items;
    const QJsonArray array = value.toArray();
    items.reserve(array.size());
    for (const QJsonValue &entry : array) {
        const QString item = entry.toString();
        if (!item.isEmpty())
            items.push_back(item);
    }
    return items;
}

template<typename T>
QJsonObject metaToJson(const T &value, MetaJsonKeyPolicy policy);

template<typename T>
T metaFromJson(const QJsonObject &object, MetaJsonKeyPolicy policy);

template<typename T>
QJsonArray listToJson(const QList<T> &items, MetaJsonKeyPolicy policy)
{
    QJsonArray array;
    for (const T &item : items)
        array.push_back(MetaJsonDetail::metaToJson(item, policy));
    return array;
}

template<typename T>
QList<T> listFromJson(const QJsonValue &value, MetaJsonKeyPolicy policy)
{
    QList<T> items;
    const QJsonArray array = value.toArray();
    items.reserve(array.size());
    for (const QJsonValue &entry : array)
        items.push_back(MetaJsonDetail::metaFromJson<T>(entry.toObject(), policy));
    return items;
}

inline QJsonValue variantToJson(const QVariant &value, QMetaType type,
                                MetaJsonKeyPolicy policy)
{
    switch (type.id()) {
    case QMetaType::QString:
        return value.toString();
    case QMetaType::Bool:
        return value.toBool();
    case QMetaType::Int:
        return value.toInt();
    case QMetaType::LongLong:
        return value.toLongLong();
    case QMetaType::Double:
        return value.toDouble();
    case QMetaType::QStringList:
        return stringListToJson(value.toStringList());
    default:
        break;
    }

    if (type.id() == qMetaTypeId<QList<PersonItem>>())
        return listToJson(qvariant_cast<QList<PersonItem>>(value), policy);
    if (type.id() == qMetaTypeId<QList<MediaStreamInfo>>())
        return listToJson(qvariant_cast<QList<MediaStreamInfo>>(value), policy);
    if (type.id() == qMetaTypeId<QList<MediaSourceInfo>>())
        return listToJson(qvariant_cast<QList<MediaSourceInfo>>(value), policy);

    qFatal("Unsupported MetaJson property type: %s", type.name());
    return {};
}

inline QVariant variantFromJson(const QJsonValue &value, QMetaType type,
                                MetaJsonKeyPolicy policy)
{
    switch (type.id()) {
    case QMetaType::QString:
        return value.toString();
    case QMetaType::Bool:
        return value.toBool(false);
    case QMetaType::Int:
        return value.toInt();
    case QMetaType::LongLong:
        return value.toVariant().toLongLong();
    case QMetaType::Double:
        return value.toDouble();
    case QMetaType::QStringList:
        return stringListFromJson(value);
    default:
        break;
    }

    if (type.id() == qMetaTypeId<QList<PersonItem>>())
        return QVariant::fromValue(listFromJson<PersonItem>(value, policy));
    if (type.id() == qMetaTypeId<QList<MediaStreamInfo>>())
        return QVariant::fromValue(listFromJson<MediaStreamInfo>(value, policy));
    if (type.id() == qMetaTypeId<QList<MediaSourceInfo>>())
        return QVariant::fromValue(listFromJson<MediaSourceInfo>(value, policy));

    qFatal("Unsupported MetaJson property type: %s", type.name());
    return {};
}

template<typename T>
QJsonObject metaToJson(const T &value, MetaJsonKeyPolicy policy)
{
    QJsonObject object;
    const QMetaObject &meta = T::staticMetaObject;
    for (int i = meta.propertyOffset(); i < meta.propertyCount(); ++i) {
        const QMetaProperty property = meta.property(i);
        object.insert(jsonKey(property.name(), policy),
                      variantToJson(property.readOnGadget(&value), property.metaType(), policy));
    }
    return object;
}

template<typename T>
T metaFromJson(const QJsonObject &object, MetaJsonKeyPolicy policy)
{
    T value;
    const QMetaObject &meta = T::staticMetaObject;
    for (int i = meta.propertyOffset(); i < meta.propertyCount(); ++i) {
        const QMetaProperty property = meta.property(i);
        const QJsonValue jsonValue = objectValueForProperty(object, property.name(), policy);
        if (jsonValue.isUndefined())
            continue;
        const QVariant propertyValue = variantFromJson(jsonValue, property.metaType(), policy);
        if (!property.writeOnGadget(&value, propertyValue))
            qFatal("Failed to write MetaJson property: %s", property.name());
    }
    return value;
}

} // namespace MetaJsonDetail

template<typename T>
QJsonObject metaToJson(const T &value,
                       MetaJsonKeyPolicy policy = MetaJsonKeyPolicy::CamelCase)
{
    return MetaJsonDetail::metaToJson(value, policy);
}

template<typename T>
T metaFromJson(const QJsonObject &object,
               MetaJsonKeyPolicy policy = MetaJsonKeyPolicy::CamelCase)
{
    return MetaJsonDetail::metaFromJson<T>(object, policy);
}

template<typename T>
QJsonArray metaListToJson(const QList<T> &items,
                          MetaJsonKeyPolicy policy = MetaJsonKeyPolicy::CamelCase)
{
    return MetaJsonDetail::listToJson(items, policy);
}

template<typename T>
QList<T> metaListFromJson(const QJsonArray &array,
                          MetaJsonKeyPolicy policy = MetaJsonKeyPolicy::CamelCase)
{
    return MetaJsonDetail::listFromJson<T>(array, policy);
}

} // namespace JellyfinNative
