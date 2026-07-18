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
    require(!PlaybackBandwidthPolicy::shouldBenchmarkFourRequests(4, 800'000'000, 850'000'000),
        "low latency without a material dual-request gain should stop after two samples");
    require(PlaybackBandwidthPolicy::shouldBenchmarkFourRequests(25, 40'000'000, 42'000'000),
        "high latency should exercise the four-request path");
    require(PlaybackBandwidthPolicy::shouldBenchmarkFourRequests(4, 40'000'000, 50'000'000),
        "a material dual-request gain should exercise the four-request path");
    require(PlaybackBandwidthPolicy::selectParallelRequests(900'000'000, 1'000'000'000) == 1,
        "one request should win when it is already close to the best result");
    require(PlaybackBandwidthPolicy::selectParallelRequests(400'000'000, 800'000'000, 820'000'000) == 2,
        "two requests should win when four requests add no useful throughput");
    require(PlaybackBandwidthPolicy::selectParallelRequests(200'000'000, 350'000'000, 800'000'000) == 4,
        "four requests should win when they materially improve throughput");
    require(PlaybackBandwidthPolicy::selectParallelRequests(0, 0, 0) == 1,
        "failed measurements should fall back to one request");
    return EXIT_SUCCESS;
}
