#include "SystemPerformanceMonitor.h"

#include <QDir>
#include <QFile>
#include <QStringList>

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

#include <algorithm>

namespace JellyfinNative {
namespace {

    QByteArray readFile(const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return file.readAll();
    }

    bool parseStat(const QByteArray& contents, QString *name, quint64 *ticks)
    {
        const qsizetype closeParen = contents.lastIndexOf(')');
        const qsizetype openParen = contents.indexOf('(');
        if (openParen < 0 || closeParen <= openParen || closeParen + 2 >= contents.size())
            return false;

        const QList<QByteArray> fields = contents.mid(closeParen + 2).simplified().split(' ');
        if (fields.size() <= 12)
            return false;

        bool userOk = false;
        bool systemOk = false;
        const quint64 userTicks = fields.at(11).toULongLong(&userOk);
        const quint64 systemTicks = fields.at(12).toULongLong(&systemOk);
        if (!userOk || !systemOk)
            return false;

        if (name)
            *name = QString::fromUtf8(contents.mid(openParen + 1, closeParen - openParen - 1));
        *ticks = userTicks + systemTicks;
        return true;
    }

    bool isMpvThread(const QString& name)
    {
        return name == QStringLiteral("core") || name == QStringLiteral("demux") || name == QStringLiteral("input")
            || name == QStringLiteral("opener") || name == QStringLiteral("vo") || name == QStringLiteral("ao")
            || name == QStringLiteral("worker") || name == QStringLiteral("curl") || name == QStringLiteral("log")
            || name.startsWith(QStringLiteral("dec/")) || name.startsWith(QStringLiteral("ao/"))
            || name.startsWith(QStringLiteral("video-")) || name.startsWith(QStringLiteral("lxvideodec"));
    }

    bool parseSchedstat(const QByteArray& contents, quint64 *runtimeNs)
    {
        const QByteArray firstField = contents.simplified().split(' ').value(0);
        bool ok = false;
        const quint64 value = firstField.toULongLong(&ok);
        if (ok)
            *runtimeNs = value;
        return ok;
    }

    double percentForTicks(quint64 ticks, double seconds, long ticksPerSecond)
    {
        if (seconds <= 0.0 || ticksPerSecond <= 0)
            return 0.0;
        return static_cast<double>(ticks) * 100.0 / (seconds * static_cast<double>(ticksPerSecond));
    }

} // namespace

SystemPerformanceMonitor::SystemPerformanceMonitor(QObject *parent)
    : QObject(parent)
{
#ifdef Q_OS_LINUX
    const long configuredTicks = sysconf(_SC_CLK_TCK);
    if (configuredTicks > 0)
        m_clockTicksPerSecond = configuredTicks;
#endif
    m_elapsed.start();
    sample();
    m_timer.setInterval(1000);
    m_timer.setTimerType(Qt::CoarseTimer);
    connect(&m_timer, &QTimer::timeout, this, &SystemPerformanceMonitor::sample);
    m_timer.start();
}

void SystemPerformanceMonitor::setAudioDecodeCpuTimeProvider(std::function<qint64()> provider)
{
    m_audioDecodeCpuTimeProvider = std::move(provider);
    m_previousAudioDecodeCpuTimeNs = m_audioDecodeCpuTimeProvider ? m_audioDecodeCpuTimeProvider() : -1;
}

void SystemPerformanceMonitor::sample()
{
#ifndef Q_OS_LINUX
    return;
#else
    const qint64 nowNs = m_elapsed.nsecsElapsed();
    const double elapsedSeconds
        = m_previousSampleNs > 0 ? static_cast<double>(nowNs - m_previousSampleNs) / 1'000'000'000.0 : 0.0;
    const qint64 audioDecodeCpuTimeNs = m_audioDecodeCpuTimeProvider ? m_audioDecodeCpuTimeProvider() : -1;

    quint64 processTicks = 0;
    const bool processOk = parseStat(readFile(QStringLiteral("/proc/self/stat")), nullptr, &processTicks);

    quint64 systemTotal = 0;
    quint64 systemIdle = 0;
    const QList<QByteArray> systemFields
        = readFile(QStringLiteral("/proc/stat")).split('\n').value(0).simplified().split(' ');
    bool systemOk = systemFields.size() >= 5 && systemFields.value(0) == "cpu";
    if (systemOk) {
        for (qsizetype index = 1; index < systemFields.size(); ++index) {
            bool ok = false;
            const quint64 value = systemFields.at(index).toULongLong(&ok);
            systemOk = systemOk && ok;
            systemTotal += value;
            if (index == 4 || index == 5)
                systemIdle += value;
        }
    }

    QHash<qint64, ThreadSample> threads;
    const QStringList tids
        = QDir(QStringLiteral("/proc/self/task")).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    quint64 mpvTicks = 0;
    quint64 videoDecodeTicks = 0;
    quint64 audioDecodeTicks = 0;
    quint64 audioOutputTicks = 0;
    quint64 mpvRuntimeNs = 0;
    quint64 videoDecodeRuntimeNs = 0;
    quint64 audioDecodeRuntimeNs = 0;
    quint64 audioOutputRuntimeNs = 0;
    bool preciseThreadCpuAvailable = false;
    for (const QString& tidText : tids) {
        bool tidOk = false;
        const qint64 tid = tidText.toLongLong(&tidOk);
        ThreadSample current;
        if (!tidOk
            || !parseStat(
                readFile(QStringLiteral("/proc/self/task/%1/stat").arg(tidText)), &current.name, &current.ticks)) {
            continue;
        }
        current.precise
            = parseSchedstat(readFile(QStringLiteral("/proc/self/task/%1/schedstat").arg(tidText)), &current.runtimeNs);
        preciseThreadCpuAvailable = preciseThreadCpuAvailable || current.precise;
        threads.insert(tid, current);
        const auto previous = m_previousThreads.constFind(tid);
        if (previous == m_previousThreads.cend() || previous->name != current.name || current.ticks < previous->ticks)
            continue;
        const bool precise = current.precise && previous->precise && current.runtimeNs >= previous->runtimeNs;
        const quint64 delta = precise ? current.runtimeNs - previous->runtimeNs : current.ticks - previous->ticks;
        quint64& mpvTotal = precise ? mpvRuntimeNs : mpvTicks;
        quint64& videoDecodeTotal = precise ? videoDecodeRuntimeNs : videoDecodeTicks;
        quint64& audioDecodeTotal = precise ? audioDecodeRuntimeNs : audioDecodeTicks;
        quint64& audioOutputTotal = precise ? audioOutputRuntimeNs : audioOutputTicks;
        if (isMpvThread(current.name))
            mpvTotal += delta;
        if (current.name == QStringLiteral("dec/video") || current.name.startsWith(QStringLiteral("lxvideodec")))
            videoDecodeTotal += delta;
        else if (current.name == QStringLiteral("dec/audio"))
            audioDecodeTotal += delta;
        else if (current.name == QStringLiteral("ao") || current.name.startsWith(QStringLiteral("ao/")))
            audioOutputTotal += delta;
    }

    const QList<QByteArray> loadFields = readFile(QStringLiteral("/proc/loadavg")).simplified().split(' ');
    if (loadFields.size() >= 3) {
        m_loadOne = loadFields.at(0).toDouble();
        m_loadFive = loadFields.at(1).toDouble();
        m_loadFifteen = loadFields.at(2).toDouble();
    }

    const QList<QByteArray> statusLines = readFile(QStringLiteral("/proc/self/status")).split('\n');
    for (const QByteArray& line : statusLines) {
        const QList<QByteArray> fields = line.simplified().split(' ');
        if (fields.size() < 2)
            continue;
        if (fields.at(0) == "VmRSS:")
            m_processRssBytes = fields.at(1).toLongLong() * 1024;
        else if (fields.at(0) == "RssAnon:")
            m_processAnonymousBytes = fields.at(1).toLongLong() * 1024;
    }

    const QList<QByteArray> memoryLines = readFile(QStringLiteral("/proc/meminfo")).split('\n');
    for (const QByteArray& line : memoryLines) {
        const QList<QByteArray> fields = line.simplified().split(' ');
        if (fields.size() < 2)
            continue;
        if (fields.at(0) == "MemTotal:")
            m_systemTotalBytes = fields.at(1).toLongLong() * 1024;
        else if (fields.at(0) == "MemAvailable:")
            m_systemAvailableBytes = fields.at(1).toLongLong() * 1024;
    }
    m_systemUsedBytes = std::max<qint64>(0, m_systemTotalBytes - m_systemAvailableBytes);

    if (elapsedSeconds > 0.0) {
        if (processOk && processTicks >= m_previousProcessTicks)
            m_processCpuPercent
                = percentForTicks(processTicks - m_previousProcessTicks, elapsedSeconds, m_clockTicksPerSecond);
        if (systemOk && systemTotal >= m_previousSystemTotal && systemIdle >= m_previousSystemIdle) {
            const quint64 totalDelta = systemTotal - m_previousSystemTotal;
            const quint64 idleDelta = systemIdle - m_previousSystemIdle;
            if (totalDelta > 0)
                m_systemCpuPercent = 100.0 * static_cast<double>(totalDelta - std::min(totalDelta, idleDelta))
                    / static_cast<double>(totalDelta);
        }
        const double elapsedNs = elapsedSeconds * 1'000'000'000.0;
        const auto precisePercent = [elapsedNs](quint64 runtimeNs) {
            return elapsedNs > 0.0 ? static_cast<double>(runtimeNs) * 100.0 / elapsedNs : 0.0;
        };
        m_mpvCpuPercent
            = precisePercent(mpvRuntimeNs) + percentForTicks(mpvTicks, elapsedSeconds, m_clockTicksPerSecond);
        m_videoDecodeCpuPercent = precisePercent(videoDecodeRuntimeNs)
            + percentForTicks(videoDecodeTicks, elapsedSeconds, m_clockTicksPerSecond);
        if (audioDecodeCpuTimeNs >= 0 && m_previousAudioDecodeCpuTimeNs >= 0
            && audioDecodeCpuTimeNs >= m_previousAudioDecodeCpuTimeNs) {
            m_audioDecodeCpuPercent
                = precisePercent(static_cast<quint64>(audioDecodeCpuTimeNs - m_previousAudioDecodeCpuTimeNs));
        } else {
            m_audioDecodeCpuPercent = precisePercent(audioDecodeRuntimeNs)
                + percentForTicks(audioDecodeTicks, elapsedSeconds, m_clockTicksPerSecond);
        }
        m_audioOutputCpuPercent = precisePercent(audioOutputRuntimeNs)
            + percentForTicks(audioOutputTicks, elapsedSeconds, m_clockTicksPerSecond);
    }

    m_available = processOk && systemOk && m_systemTotalBytes > 0;
    m_threadBreakdownAvailable = !threads.isEmpty();
    m_preciseThreadCpuAvailable = preciseThreadCpuAvailable;
    m_previousProcessTicks = processTicks;
    m_previousSystemTotal = systemTotal;
    m_previousSystemIdle = systemIdle;
    m_previousThreads = std::move(threads);
    m_previousSampleNs = nowNs;
    m_previousAudioDecodeCpuTimeNs = audioDecodeCpuTimeNs;
    emit metricsChanged();
#endif
}

} // namespace JellyfinNative
