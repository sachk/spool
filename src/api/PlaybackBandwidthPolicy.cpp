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

} // namespace JellyfinNative
