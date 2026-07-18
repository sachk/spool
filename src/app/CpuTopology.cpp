#include "CpuTopology.h"
#include "../platform/PlatformSystemProbes.h"

#include <algorithm>
#include <thread>

namespace JellyfinNative {

namespace {

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
    const PlatformCpuProbe probe = platformCpuProbe(logical);
    const int physical = std::max(1, probe.physicalCores);
    const bool smt = logical > physical;
    int decodeThreads = environmentDecodeThreads();
    if (decodeThreads <= 0)
        decodeThreads = smt ? physical : (logical + 1) / 2;
    decodeThreads = std::clamp(decodeThreads, 1, logical * 2);
    const QString source = probe.source;
    return { logical, physical, smt, decodeThreads, source };
}

} // namespace JellyfinNative
