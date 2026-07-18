#pragma once

#include <QString>
#include <QStringList>

namespace JellyfinNative {

QString resolveAppRoot(const char *argv0);
QString startupCacheRoot(const QString& appRootPath);
QString persistentDataRoot();
QStringList appLogDirectories(const QString& appRootPath);
QString appLogFileName();

} // namespace JellyfinNative
