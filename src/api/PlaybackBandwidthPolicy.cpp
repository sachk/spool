#include "PlaybackBandwidthPolicy.h"

#include <algorithm>
#include <cmath>

namespace JellyfinNative {

qint64 PlaybackBandwidthPolicy::effectiveBitrate(
    qint64 manualBitrate, bool unlimitedLocalNetwork, bool endpointKnown, bool inLocalNetwork, qint64 measuredBitrate)
{
    if (unlimitedLocalNetwork && endpointKnown && inLocalNetwork)
        return MaximumBitrate;
    if (manualBitrate > 0)
        return std::clamp<qint64>(manualBitrate, 1'000'000, MaximumBitrate);
    if (measuredBitrate > 0)
        return std::clamp<qint64>(measuredBitrate, 1'000'000, MaximumBitrate);
    if (endpointKnown && inLocalNetwork)
        return MaximumBitrate;
    return AutoFallbackBitrate;
}

qint64 PlaybackBandwidthPolicy::conservativeEstimate(qint64 transferredBytes, qint64 elapsedMilliseconds)
{
    if (transferredBytes <= 0 || elapsedMilliseconds <= 0)
        return 0;
    const long double measuredBitsPerSecond
        = static_cast<long double>(transferredBytes) * 8'000.0L / static_cast<long double>(elapsedMilliseconds);
    const long double conservative = measuredBitsPerSecond * 0.75L;
    return std::clamp<qint64>(static_cast<qint64>(std::llround(conservative)), 1'000'000, MaximumBitrate);
}

bool PlaybackBandwidthPolicy::shouldBenchmarkFourRequests(
    qint64 roundTripMilliseconds, qint64 singleRequestBitrate, qint64 dualRequestBitrate)
{
    if (singleRequestBitrate <= 0 || dualRequestBitrate <= 0)
        return false;

    constexpr qint64 highLatencyMilliseconds = 20;
    constexpr long double worthwhileDualGain = 1.10L;
    return roundTripMilliseconds >= highLatencyMilliseconds
        || static_cast<long double>(dualRequestBitrate)
        >= static_cast<long double>(singleRequestBitrate) * worthwhileDualGain;
}

int PlaybackBandwidthPolicy::selectParallelRequests(
    qint64 singleRequestBitrate, qint64 dualRequestBitrate, qint64 fourRequestBitrate)
{
    const qint64 best = std::max({ singleRequestBitrate, dualRequestBitrate, fourRequestBitrate });
    if (best <= 0)
        return 1;

    constexpr long double closeEnoughToBest = 0.85L;
    const auto isCloseEnough = [best](qint64 bitrate) {
        return bitrate > 0 && static_cast<long double>(bitrate) >= static_cast<long double>(best) * closeEnoughToBest;
    };
    if (isCloseEnough(singleRequestBitrate))
        return 1;
    if (isCloseEnough(dualRequestBitrate))
        return 2;
    return fourRequestBitrate > 0 ? 4 : 2;
}

} // namespace JellyfinNative
