#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

namespace JellyfinNative {

QStringList stringListFromVariant(const QVariant& value);
QStringList stringListFromVariantMap(const QVariantMap& map, const QString& key);

} // namespace JellyfinNative
