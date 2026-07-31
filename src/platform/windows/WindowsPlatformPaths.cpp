#include "platform/PlatformPaths.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <windows.h>

namespace JellyfinNative {

namespace {
    QString localDataRoot()
    {
        const QString configured = QString::fromLocal8Bit(qgetenv("LOCALAPPDATA"));
        return configured.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                                    : QDir(configured).filePath(QStringLiteral("com.sachk.spool"));
    }
} // namespace

QString resolveAppRoot(const char *)
{
    wchar_t executablePath[32768] {};
    const DWORD length = GetModuleFileNameW(nullptr, executablePath, static_cast<DWORD>(std::size(executablePath)));
    if (length == 0 || length >= std::size(executablePath))
        return {};
    return QFileInfo(QString::fromWCharArray(executablePath, static_cast<qsizetype>(length))).absolutePath();
}

QString startupCacheRoot(const QString&)
{
    const QByteArray configured = qgetenv("JELLYFIN_NATIVE_CACHE_HOME");
    if (!configured.isEmpty())
        return QString::fromLocal8Bit(configured);
    return QDir(localDataRoot()).filePath(QStringLiteral("cache"));
}

QString persistentDataRoot()
{
    return localDataRoot();
}

QStringList appLogDirectories(const QString&)
{
    return { QDir(localDataRoot()).filePath(QStringLiteral("logs")) };
}

QString appLogFileName()
{
    return QStringLiteral("jellyfin-native.log");
}

} // namespace JellyfinNative
