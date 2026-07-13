#include "MemoryBudget.h"

#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QTextStream>

#include <algorithm>

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace JellyfinNative {

namespace {

    constexpr qint64 kMiB = 1024LL * 1024LL;
    constexpr qint64 kLowMemoryThresholdBytes = 1536LL * kMiB;

    qint64 readLinuxMemTotal()
    {
        QFile file(QStringLiteral("/proc/meminfo"));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return 0;

        while (!file.atEnd()) {
            const QByteArray line = file.readLine();
            if (!line.startsWith("MemTotal:"))
                continue;
            const QList<QByteArray> parts = line.simplified().split(' ');
            if (parts.size() >= 2)
                return parts.at(1).toLongLong() * 1024LL;
        }
        return 0;
    }

    qint64 readMemTotal()
    {
#ifdef __APPLE__
        int64_t bytes = 0;
        size_t size = sizeof(bytes);
        if (sysctlbyname("hw.memsize", &bytes, &size, nullptr, 0) == 0 && bytes > 0)
            return bytes;
#endif
#ifdef __linux__
        return readLinuxMemTotal();
#elif defined(Q_OS_WIN)
        MEMORYSTATUSEX status { };
        status.dwLength = sizeof(status);
        if (GlobalMemoryStatusEx(&status))
            return static_cast<qint64>(status.ullTotalPhys);
        return 0;
#else
        return 0;
#endif
    }

    qint64 fallbackMemTotal()
    {
#ifdef JELLYFIN_NATIVE_WEBOS
        return 1024LL * kMiB;
#else
        return 4LL * 1024LL * kMiB;
#endif
    }

} // namespace

MemoryBudget MemoryBudget::detect()
{
    MemoryBudget budget;
    budget.memTotalBytes = readMemTotal();
    const qint64 effectiveMem = budget.memTotalBytes > 0 ? budget.memTotalBytes : fallbackMemTotal();
    const bool lowMemory = effectiveMem < kLowMemoryThresholdBytes;

#ifdef JELLYFIN_NATIVE_WEBOS
    constexpr qint64 baseNetworkDiskCacheBytes = 96LL * kMiB;
    constexpr qint64 baseQmlImageDiskCacheBytes = 160LL * kMiB;
#else
    constexpr qint64 baseNetworkDiskCacheBytes = 256LL * kMiB;
    constexpr qint64 baseQmlImageDiskCacheBytes = 256LL * kMiB;
#endif

    budget.networkDiskCacheBytes = lowMemory ? baseNetworkDiskCacheBytes / 2 : baseNetworkDiskCacheBytes;
    budget.qmlImageDiskCacheBytes = lowMemory ? baseQmlImageDiskCacheBytes / 2 : baseQmlImageDiskCacheBytes;
    budget.artworkByteCacheBytes = static_cast<int>(std::clamp(effectiveMem / 32, 24LL * kMiB, 96LL * kMiB));
    budget.mpvDemuxerMaxBytes = lowMemory ? QByteArrayLiteral("32M") : QByteArrayLiteral("64M");
    budget.mpvDemuxerMaxBackBytes = lowMemory ? QByteArrayLiteral("16M") : QByteArrayLiteral("32M");
    return budget;
}

} // namespace JellyfinNative
