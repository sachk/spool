#pragma once

#include <QtTypes>

namespace JellyfinNative {

class PlaybackBandwidthPolicy final {
public:
    static constexpr qint64 AutoFallbackBitrate = 20'000'000;
    static constexpr qint64 MaximumBitrate = 1'000'000'000;

    static qint64 effectiveBitrate(qint64 manualBitrate, bool unlimitedLocalNetwork, bool endpointKnown,
        bool inLocalNetwork, qint64 measuredBitrate);
    static qint64 conservativeEstimate(qint64 transferredBytes, qint64 elapsedMilliseconds);
    static bool shouldBenchmarkFourRequests(
        qint64 roundTripMilliseconds, qint64 singleRequestBitrate, qint64 dualRequestBitrate);
    static int selectParallelRequests(
        qint64 singleRequestBitrate, qint64 dualRequestBitrate, qint64 fourRequestBitrate = 0);
};

} // namespace JellyfinNative
