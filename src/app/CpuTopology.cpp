#include "CpuTopology.h"

#include <QFile>
#include <QStringList>

#include <algorithm>
#include <thread>

namespace JellyfinNative {

namespace {

    int cpuListSize(const QString& text)
    {
        int count = 0;
        for (const QString& range : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
            const QStringList bounds = range.split(QLatin1Char('-'));
            bool firstOk = false;
            bool lastOk = false;
            const int first = bounds.first().toInt(&firstOk);
            const int last = bounds.size() > 1 ? bounds.last().toInt(&lastOk) : first;
            if (firstOk && (bounds.size() == 1 || lastOk) && last >= first)
                count += last - first + 1;
        }
        return count;
    }

    int threadsPerCore()
    {
        QFile siblings(QStringLiteral("/sys/devices/system/cpu/cpu0/topology/thread_siblings_list"));
        if (!siblings.open(QIODevice::ReadOnly | QIODevice::Text))
            return 1;
        return std::max(1, cpuListSize(QString::fromUtf8(siblings.readAll()).trimmed()));
    }

    int environmentDecodeThreads()
    {
        bool ok = false;
        const int value = QString::fromLocal8Bit(qgetenv("JELLYFIN_WEBP_DECODE_THREADS")).toInt(&ok);
        return ok ? value : 0;
    }

} // namespace

CpuTopology detectCpuTopology()
{
    const int logical = std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
    const int siblings = threadsPerCore();
    const int physical = std::max(1, (logical + siblings - 1) / siblings);
    const bool smt = siblings > 1;
    int decodeThreads = environmentDecodeThreads();
    if (decodeThreads <= 0)
        decodeThreads = smt ? physical : (logical + 1) / 2;
    decodeThreads = std::clamp(decodeThreads, 1, logical * 2);
    const QString source = siblings > 1 ? QStringLiteral("hardware_concurrency+thread_siblings")
                                        : QStringLiteral("hardware_concurrency");
    return { logical, physical, smt, decodeThreads, source };
}

} // namespace JellyfinNative
