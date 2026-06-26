#include "CpuTopology.h"

#include <QFile>
#include <QList>
#include <QSet>
#include <QStringList>
#include <QThread>

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace JellyfinNative {

namespace {

QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll()).trimmed();
}

QSet<int> parseCpuList(const QString &text)
{
    QSet<int> cpus;
    const QStringList ranges = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &range : ranges) {
        const QString trimmed = range.trimmed();
        const QStringList bounds = trimmed.split(QLatin1Char('-'));
        bool ok = false;
        const int first = bounds.value(0).toInt(&ok);
        if (!ok)
            continue;
        int last = first;
        if (bounds.size() > 1) {
            last = bounds.value(1).toInt(&ok);
            if (!ok)
                continue;
        }
        for (int cpu = first; cpu <= last; ++cpu)
            cpus.insert(cpu);
    }
    return cpus;
}

QSet<int> systemCpus(QString *source)
{
    const std::pair<QString, QString> candidates[] = {
        {QStringLiteral("/sys/devices/system/cpu/possible"), QStringLiteral("possible")},
        {QStringLiteral("/sys/devices/system/cpu/present"), QStringLiteral("present")},
        {QStringLiteral("/sys/devices/system/cpu/online"), QStringLiteral("online")},
    };
    for (const auto &[path, name] : candidates) {
        QSet<int> cpus = parseCpuList(readTextFile(path));
        if (!cpus.isEmpty()) {
            if (source)
                *source = name;
            return cpus;
        }
    }

    const int count = std::max(1, QThread::idealThreadCount());
    if (source)
        *source = QStringLiteral("idealThreadCount");
    QSet<int> cpus;
    for (int cpu = 0; cpu < count; ++cpu)
        cpus.insert(cpu);
    return cpus;
}

QString normalizedSiblingSet(const QString &text, const QSet<int> &cpus)
{
    QList<int> siblings;
    const QSet<int> parsed = parseCpuList(text);
    for (int cpu : parsed) {
        if (cpus.contains(cpu))
            siblings.push_back(cpu);
    }
    std::sort(siblings.begin(), siblings.end());

    QStringList parts;
    parts.reserve(siblings.size());
    for (int cpu : siblings)
        parts.push_back(QString::number(cpu));
    return parts.join(QLatin1Char(','));
}

int physicalCoresFromSiblings(const QSet<int> &cpus)
{
    QSet<QString> siblingGroups;
    int covered = 0;
    for (int cpu : cpus) {
        const QString text =
            readTextFile(QStringLiteral("/sys/devices/system/cpu/cpu%1/topology/thread_siblings_list").arg(cpu));
        const QString siblings = normalizedSiblingSet(text, cpus);
        if (!siblings.isEmpty())
            siblingGroups.insert(siblings);
        if (!text.isEmpty())
            ++covered;
    }
    return covered == cpus.size() && !siblingGroups.isEmpty() ? siblingGroups.size() : 0;
}

int physicalCoresFromCoreIds(const QSet<int> &cpus)
{
    QSet<QString> cores;
    int covered = 0;
    for (int cpu : cpus) {
        const QString packageId =
            readTextFile(QStringLiteral("/sys/devices/system/cpu/cpu%1/topology/physical_package_id").arg(cpu));
        const QString coreId =
            readTextFile(QStringLiteral("/sys/devices/system/cpu/cpu%1/topology/core_id").arg(cpu));
        if (!packageId.isEmpty() && !coreId.isEmpty()) {
            cores.insert(packageId + QLatin1Char(':') + coreId);
            ++covered;
        }
    }
    return covered == cpus.size() && !cores.isEmpty() ? cores.size() : 0;
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
    QString cpuSetSource;
    const QSet<int> cpus = systemCpus(&cpuSetSource);
    const int logical = std::max(1, static_cast<int>(cpus.size()));
    int physical = physicalCoresFromSiblings(cpus);
    QString source = cpuSetSource + QStringLiteral("+thread_siblings");
    if (physical <= 0) {
        physical = physicalCoresFromCoreIds(cpus);
        source = cpuSetSource + QStringLiteral("+core_id");
    }
    if (physical <= 0) {
        physical = logical;
        source = cpuSetSource;
    }

    const bool smt = physical > 0 && physical < logical;
    int decodeThreads = environmentDecodeThreads();
    if (decodeThreads <= 0)
        decodeThreads = smt ? physical : (logical + 1) / 2;
    decodeThreads = std::clamp(decodeThreads, 1, std::max(1, logical * 2));

    return {logical, std::max(1, physical), smt, decodeThreads, source};
}

} // namespace JellyfinNative
