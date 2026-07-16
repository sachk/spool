#include "diagnostics/SystemPerformanceMonitor.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QTimer>

#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << message << '\n';
    std::exit(1);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    JellyfinNative::SystemPerformanceMonitor monitor;

    QEventLoop waitForSecondSample;
    QTimer::singleShot(1100, &waitForSecondSample, &QEventLoop::quit);
    waitForSecondSample.exec();

#ifdef Q_OS_LINUX
    require(monitor.available(), "Linux performance counters should be available");
    require(monitor.threadBreakdownAvailable(), "Linux thread counters should be available");
    if (QFileInfo::exists(QStringLiteral("/proc/self/task/%1/schedstat").arg(QCoreApplication::applicationPid())))
        require(monitor.preciseThreadCpuAvailable(), "schedstat should enable precise thread CPU counters");
    require(monitor.processCpuPercent() >= 0.0, "process CPU should be non-negative");
    require(
        monitor.systemCpuPercent() >= 0.0 && monitor.systemCpuPercent() <= 100.0, "system CPU should be a percentage");
    require(monitor.processRssBytes() > 0, "process RSS should be populated");
    require(monitor.systemTotalBytes() > 0, "system memory should be populated");
    require(monitor.systemAvailableBytes() > 0, "available memory should be populated");
    require(monitor.systemUsedBytes() > 0, "used memory should be populated");
#else
    require(!monitor.available(), "unsupported platforms should report unavailable counters");
#endif
    return 0;
}
