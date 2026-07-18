#include "SystemPerformanceMonitor.h"

#include <utility>

namespace JellyfinNative {

SystemPerformanceMonitor::SystemPerformanceMonitor(QObject *parent)
    : QObject(parent)
{
    sample();
    m_timer.setInterval(1000);
    m_timer.setTimerType(Qt::CoarseTimer);
    connect(&m_timer, &QTimer::timeout, this, &SystemPerformanceMonitor::sample);
    m_timer.start();
}

void SystemPerformanceMonitor::setAudioDecodeCpuTimeProvider(std::function<qint64()> provider)
{
    m_audioDecodeCpuTimeProvider = std::move(provider);
}

void SystemPerformanceMonitor::sample()
{
    PlatformPerformanceSample sample;
    const qint64 audioDecodeCpuTimeNs = m_audioDecodeCpuTimeProvider ? m_audioDecodeCpuTimeProvider() : -1;
    if (!m_sampler.sample(audioDecodeCpuTimeNs, sample))
        return;

    m_available = sample.available;
    m_threadBreakdownAvailable = sample.threadBreakdownAvailable;
    m_preciseThreadCpuAvailable = sample.preciseThreadCpuAvailable;
    m_processCpuPercent = sample.processCpuPercent;
    m_systemCpuPercent = sample.systemCpuPercent;
    m_mpvCpuPercent = sample.mpvCpuPercent;
    m_videoDecodeCpuPercent = sample.videoDecodeCpuPercent;
    m_audioDecodeCpuPercent = sample.audioDecodeCpuPercent;
    m_audioOutputCpuPercent = sample.audioOutputCpuPercent;
    m_loadOne = sample.loadOne;
    m_loadFive = sample.loadFive;
    m_loadFifteen = sample.loadFifteen;
    m_processRssBytes = sample.processRssBytes;
    m_processAnonymousBytes = sample.processAnonymousBytes;
    m_systemUsedBytes = sample.systemUsedBytes;
    m_systemAvailableBytes = sample.systemAvailableBytes;
    m_systemTotalBytes = sample.systemTotalBytes;
    emit metricsChanged();
}

} // namespace JellyfinNative
