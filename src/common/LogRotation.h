#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

namespace JellyfinNative {

// mkdir -p for the directory portion of a log path. The log directory lives
// under /tmp on webOS and is wiped every boot, so every writer must be able
// to recreate it — not just whichever code path happened to run first.
inline void ensureParentDirectoryExists(const char *path)
{
    const QFileInfo info(QString::fromLocal8Bit(path));
    QDir().mkpath(info.absolutePath());
}

inline void rotateLogFile(const char *path)
{
    ensureParentDirectoryExists(path);
    const QString current = QString::fromLocal8Bit(path);
    const QString first = current + QStringLiteral(".1");
    const QString second = current + QStringLiteral(".2");
    QFile::remove(second);
    QFile::rename(first, second);
    QFile::rename(current, first);
}

} // namespace JellyfinNative
