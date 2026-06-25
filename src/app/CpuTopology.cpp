#include "CpuTopology.h"

#include <QDir>
#include <QFile>
#include <QSet>
#include <QStringList>
#include <QThread>

#include <algorithm>
#include <cstdlib>

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

QSet<int> onlineCpus()
{
    QSet<int> cpus = parseCpuList(readTextFile(QStringLiteral("/sys/devices/system/cpu/online")));
    if (!cpus.isEmpty())
        return cpus;

    const int count = std::max(1, QThread::idealThreadCount());
    for (int cpu = 0; cpu < count; ++cpu)
        cpus.insert(cpu);
    return cpus;
}

QString normalizedSiblingSet(const QString &text, const QSet<int> &online)
{
    QList<int> siblings;
    const QSet<int> parsed = parseCpuList(text);
    for (int cpu : parsed) {
        if (online.contains(cpu))
            siblings.push_back(cpu);
    }
    std::sort(siblings.begin(), siblings.end());

    QStringList parts;
    parts.reserve(siblings.size());
    for (int cpu : siblings)
        parts.push_back(QString::number(cpu));
    return parts.join(QLatin1Char(','));
}

int physicalCoresFromSiblings(const QSet<int> &online)
{
    QSet<QString> siblingGroups;
    for (int cpu : online) {
        const QString siblings = normalizedSiblingSet(
            readTextFile(QStringLiteral("/sys/devices/system/cpu/cpu%1/topology/thread_siblings_list").arg(cpu)),
            online);
        if (!siblings.isEmpty())
            siblingGroups.insert(siblings);
    }
    return siblingGroups.isEmpty() ? 0 : siblingGroups.size();
}

int physicalCoresFromCoreIds(const QSet<int> &online)
{
    QSet<QString> cores;
    for (int cpu : online) {
        const QString packageId =
            readTextFile(QStringLiteral("/sys/devices/system/cpu/cpu%1/topology/physical_package_id").arg(cpu));
        const QString coreId =
            readTextFile(QStringLiteral("/sys/devices/system/cpu/cpu%1/topology/core_id").arg(cpu));
        if (!packageId.isEmpty() && !coreId.isEmpty())
            cores.insert(packageId + QLatin1Char(':') + coreId);
    }
    return cores.isEmpty() ? 0 : cores.size();
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
    const QSet<int> online = onlineCpus();
    const int logical = std::max(1, online.size());
    int physical = physicalCoresFromSiblings(online);
    QString source = QStringLiteral("thread_siblings");
    if (physical <= 0) {
        physical = physicalCoresFromCoreIds(online);
        source = QStringLiteral("core_id");
    }
    if (physical <= 0) {
        physical = logical;
        source = QStringLiteral("idealThreadCount");
    }

    const bool smt = physical > 0 && physical < logical;
    int decodeThreads = environmentDecodeThreads();
    if (decodeThreads <= 0)
        decodeThreads = smt ? physical : (logical + 1) / 2;
    decodeThreads = std::clamp(decodeThreads, 1, std::max(1, logical * 2));

    return {logical, std::max(1, physical), smt, decodeThreads, source};
}

} // namespace JellyfinNative
