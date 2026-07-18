#include "platform/MpvConfigPolicy.h"

#include <QDir>
#include <QFileInfo>

namespace JellyfinNative {

MpvConfigPolicy validatedPlatformMpvConfigPolicy(const QString& mode, const QString& directory)
{
    if (mode.compare(QStringLiteral("standard"), Qt::CaseInsensitive) == 0)
        return { MpvConfigPolicy::Mode::Standard };
    if (mode.compare(QStringLiteral("custom"), Qt::CaseInsensitive) != 0)
        return {};

    const QFileInfo info { QDir::cleanPath(directory) };
    if (!info.exists() || !info.isDir()) {
        return { MpvConfigPolicy::Mode::Disabled, {}, false,
            QStringLiteral("The custom mpv configuration directory does not exist.") };
    }
    const QString canonical = info.canonicalFilePath();
    if (canonical.isEmpty()) {
        return { MpvConfigPolicy::Mode::Disabled, {}, false,
            QStringLiteral("The custom mpv configuration directory cannot be resolved.") };
    }
    return { MpvConfigPolicy::Mode::Custom, canonical };
}

} // namespace JellyfinNative
