#include "api/PlaybackBandwidthPolicy.h"

#include "TestMain.h"

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

JELLYFIN_TEST_MAIN("playback-bandwidth-policy")
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

    using Source = PlaybackBandwidthPolicy::Source;
    require(PlaybackBandwidthPolicy::effectiveBitrate(0, false, true, true, 0)
            == PlaybackBandwidthPolicy::LocalNetworkFallbackBitrate,
        "an unmeasured local network must not be assumed to be gigabit ethernet");
    require(PlaybackBandwidthPolicy::LocalNetworkFallbackBitrate < PlaybackBandwidthPolicy::MaximumBitrate,
        "the unmeasured local ceiling has to leave room for a slow wireless link");
    require(PlaybackBandwidthPolicy::effectiveBitrate(0, false, true, true, 300'000'000) == 300'000'000,
        "a measurement should replace the local estimate in both directions");
    require(PlaybackBandwidthPolicy::effectiveBitrate(0, false, true, true, 6'000'000) == 6'000'000,
        "a slow measured local link must not be raised back up to the estimate");
    require(PlaybackBandwidthPolicy::effectiveBitrateSource(0, false, true, true, 0) == Source::LocalEstimate
            && PlaybackBandwidthPolicy::isEstimate(Source::LocalEstimate),
        "an unmeasured local ceiling should report itself as an estimate");
    require(PlaybackBandwidthPolicy::effectiveBitrateSource(0, false, false, false, 0) == Source::Estimate
            && PlaybackBandwidthPolicy::isEstimate(Source::Estimate),
        "playback that starts before any measurement should report an estimate");
    require(PlaybackBandwidthPolicy::effectiveBitrateSource(0, false, true, true, 300'000'000) == Source::Measured
            && !PlaybackBandwidthPolicy::isEstimate(Source::Measured),
        "a measured ceiling should not be presented as a guess");
    require(
        PlaybackBandwidthPolicy::effectiveBitrateSource(80'000'000, false, true, true, 40'000'000) == Source::Manual,
        "an explicit limit should be reported as the user's own choice");
    require(PlaybackBandwidthPolicy::effectiveBitrateSource(80'000'000, true, true, true, 0) == Source::UnlimitedLocal,
        "the local opt-out should be reported ahead of a manual limit");

    const QString home = PlaybackBandwidthPolicy::networkSignature(
        QStringLiteral("media.example:8096"), { QStringLiteral("192.168.1.44") }, QStringLiteral("wifi"));
    require(home
            == PlaybackBandwidthPolicy::networkSignature(QStringLiteral("media.example:8096"),
                { QStringLiteral("192.168.1.90"), QStringLiteral("127.0.0.1") }, QStringLiteral("wifi")),
        "a new DHCP lease on the same network should reuse the remembered measurement");
    require(home
            != PlaybackBandwidthPolicy::networkSignature(
                QStringLiteral("media.example:8096"), { QStringLiteral("10.4.0.9") }, QStringLiteral("wifi")),
        "a different local network must not inherit the previous measurement");
    require(home
            != PlaybackBandwidthPolicy::networkSignature(
                QStringLiteral("media.example:8096"), { QStringLiteral("192.168.1.44") }, QStringLiteral("cellular")),
        "the same private range over cellular is a different route");
    require(home
            != PlaybackBandwidthPolicy::networkSignature(
                QStringLiteral("other.example:8096"), { QStringLiteral("192.168.1.44") }, QStringLiteral("wifi")),
        "a measurement describes the route to one server");
    require(!PlaybackBandwidthPolicy::networkSignature(QStringLiteral("media.example:8096"), {}, QString()).isEmpty(),
        "an unknown route should still produce a usable cache key");

    constexpr qint64 now = 1'800'000'000'000;
    require(PlaybackBandwidthPolicy::isRememberedMeasurementUsable(now - 60'000, now),
        "a recent measurement should be reused instead of guessing");
    require(!PlaybackBandwidthPolicy::isRememberedMeasurementUsable(
                now - PlaybackBandwidthPolicy::RememberedLifetimeMs - 1, now),
        "a stale measurement should be re-measured rather than trusted");
    require(!PlaybackBandwidthPolicy::isRememberedMeasurementUsable(now + 60'000, now),
        "a record from the future means the clock moved and must not be trusted");
    require(!PlaybackBandwidthPolicy::isRememberedMeasurementUsable(0, now), "an absent record is not a measurement");

    require(PlaybackBandwidthPolicy::formatBitrate(40'000'000) == QStringLiteral("40 Mbps")
            && PlaybackBandwidthPolicy::formatBitrate(3'000'000) == QStringLiteral("3.0 Mbps")
            && PlaybackBandwidthPolicy::formatBitrate(720'000) == QStringLiteral("720 kbps"),
        "a ceiling should be readable at every scale the ladder covers");
    require(PlaybackBandwidthPolicy::formatBitrate(0) == QStringLiteral("unknown"),
        "an absent ceiling must not be printed as zero");

    const QList<PlaybackBandwidthPolicy::QualityOption> fullLadder = PlaybackBandwidthPolicy::qualityLadder(0);
    require(!fullLadder.isEmpty(), "an unknown source bitrate should still offer the whole ladder");
    for (int index = 1; index < fullLadder.size(); ++index) {
        require(fullLadder[index].bitrate < fullLadder[index - 1].bitrate,
            "the ladder should descend so the first rungs are the best quality");
    }
    require(fullLadder.constFirst().label.contains(QStringLiteral("4K"))
            && fullLadder.constFirst().label.contains(QStringLiteral("Mbps")),
        "a rung should name both the resolution and the ceiling");

    // The rung carries its resolution rather than only spelling it in the
    // label: the label was all there was, so the server heard a bitrate and
    // nothing about size, and 480p lowered the bitrate alone.
    for (const PlaybackBandwidthPolicy::QualityOption& rung : fullLadder) {
        require(rung.height > 0, "every rung should carry the resolution it promises");
        require(rung.label.startsWith(PlaybackBandwidthPolicy::describeResolution(rung.height)),
            "a rung's label should be the resolution it actually asks the server for");
    }
    for (int index = 1; index < fullLadder.size(); ++index) {
        require(fullLadder[index].height <= fullLadder[index - 1].height,
            "resolution should never climb as the ladder descends");
    }
    require(PlaybackBandwidthPolicy::describeResolution(2160) == QStringLiteral("4K"),
        "2160p is sold as 4K everywhere else, so name it that here");
    require(PlaybackBandwidthPolicy::describeResolution(480) == QStringLiteral("480p"),
        "every other rung is named by its height");
    require(PlaybackBandwidthPolicy::describeResolution(0) == QStringLiteral("Source"),
        "no ceiling means the source resolution, not an unnamed one");

    // The standalone ladder exists so resolution can be answered without
    // reasoning about megabits.
    const QList<int> resolutions = PlaybackBandwidthPolicy::resolutionLadder();
    require(!resolutions.isEmpty(), "a resolution ceiling should be offerable on its own");
    for (int index = 1; index < resolutions.size(); ++index)
        require(resolutions[index] < resolutions[index - 1], "the resolution ladder should descend");

    const QList<PlaybackBandwidthPolicy::QualityOption> ladder = PlaybackBandwidthPolicy::qualityLadder(12'000'000);
    require(!ladder.isEmpty(), "a modest source should still offer lower rungs");
    for (const PlaybackBandwidthPolicy::QualityOption& rung : ladder) {
        require(rung.bitrate < 12'000'000, "a rung at or above the source would transcode without lowering anything");
    }

    require(PlaybackBandwidthPolicy::describeAuto(Source::Measured, 45'000'000, 2)
            == QStringLiteral("Measured 45 Mbps · 2 connections"),
        "Auto should report the measurement and how it was reached");
    require(!PlaybackBandwidthPolicy::describeAuto(Source::Measured, 45'000'000, 1).contains(QStringLiteral("connect")),
        "a single connection is the unremarkable case and should not be narrated");
    require(
        PlaybackBandwidthPolicy::describeAuto(Source::Remembered, 45'000'000, 1).contains(QStringLiteral("remembered")),
        "Auto should admit when the ceiling came from a previous visit to this network");
    require(PlaybackBandwidthPolicy::describeAuto(Source::LocalEstimate, 40'000'000, 1)
                .contains(QStringLiteral("not measured yet"))
            && PlaybackBandwidthPolicy::describeAuto(Source::Estimate, 20'000'000, 1)
                .contains(QStringLiteral("not measured yet")),
        "an unmeasured ceiling must say so rather than looking like a measurement");
    require(PlaybackBandwidthPolicy::describeAuto(Source::UnlimitedLocal, PlaybackBandwidthPolicy::MaximumBitrate, 1)
            == QStringLiteral("No limit on this network"),
        "the local opt-out should not be printed as a 1000 Mbps measurement");
    require(PlaybackBandwidthPolicy::describeAuto(Source::Manual, 25'000'000, 1).contains(QStringLiteral("Settings")),
        "a ceiling from the Settings slider should point at where to change it");

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
