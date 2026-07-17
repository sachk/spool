#include "api/PlaybackBandwidthPolicy.h"

#include <QDebug>

#include <cstdlib>

using JellyfinNative::PlaybackBandwidthPolicy;

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    qCritical() << message;
    std::exit(EXIT_FAILURE);
}

} // namespace

int main()
{
    require(PlaybackBandwidthPolicy::effectiveBitrate(0, false, false, false, 0)
            == PlaybackBandwidthPolicy::AutoFallbackBitrate,
        "automatic bandwidth should use the safe fallback before measurement");
    require(PlaybackBandwidthPolicy::effectiveBitrate(80'000'000, false, true, true, 40'000'000) == 80'000'000,
        "manual bandwidth should override the measured rate");
    require(PlaybackBandwidthPolicy::effectiveBitrate(80'000'000, true, true, true, 40'000'000)
            == PlaybackBandwidthPolicy::MaximumBitrate,
        "local unlimited mode should override both manual and measured rates");
    require(PlaybackBandwidthPolicy::effectiveBitrate(0, true, true, false, 40'000'000) == 40'000'000,
        "local unlimited mode should not affect remote connections");
    require(PlaybackBandwidthPolicy::conservativeEstimate(10'000'000, 1'000) == 60'000'000,
        "measurement should reserve twenty-five percent of observed throughput");
    require(PlaybackBandwidthPolicy::conservativeEstimate(0, 1'000) == 0,
        "empty transfers should not produce a measurement");
    return EXIT_SUCCESS;
}
