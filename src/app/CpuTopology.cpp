#include "CpuTopology.h"

#include <QByteArray>
#include <QFile>
#include <QStringList>

#include <algorithm>
#include <thread>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

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

#ifdef Q_OS_WIN
    int windowsPhysicalCoreCount()
    {
        DWORD bytes = 0;
        GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &bytes);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || bytes == 0)
            return 0;
        QByteArray storage(static_cast<qsizetype>(bytes), Qt::Uninitialized);
        auto *cursor = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(storage.data());
        if (!GetLogicalProcessorInformationEx(RelationProcessorCore, cursor, &bytes))
            return 0;
        int cores = 0;
        DWORD offset = 0;
        while (offset < bytes) {
            auto *entry = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(storage.data() + offset);
            ++cores;
            offset += entry->Size;
        }
        return cores;
    }
#endif

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
#ifdef Q_OS_WIN
    const int detectedPhysical = windowsPhysicalCoreCount();
    const int physical = detectedPhysical > 0 ? detectedPhysical : logical;
#else
    const int physical = std::max(1, (logical + siblings - 1) / siblings);
#endif
    const bool smt = logical > physical;
    int decodeThreads = environmentDecodeThreads();
    if (decodeThreads <= 0)
        decodeThreads = smt ? physical : (logical + 1) / 2;
    decodeThreads = std::clamp(decodeThreads, 1, logical * 2);
    const QString source =
#ifdef Q_OS_WIN
        detectedPhysical > 0 ? QStringLiteral("GetLogicalProcessorInformationEx")
                             : QStringLiteral("hardware_concurrency");
#else
        siblings > 1 ? QStringLiteral("hardware_concurrency+thread_siblings") : QStringLiteral("hardware_concurrency");
#endif
    return { logical, physical, smt, decodeThreads, source };
}

} // namespace JellyfinNative
