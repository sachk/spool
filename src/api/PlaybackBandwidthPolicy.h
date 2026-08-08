#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QtTypes>

namespace JellyfinNative {

class PlaybackBandwidthPolicy final {
public:
    // Where the ceiling handed to the server came from. The player surfaces
    // this so "Auto" can say whether it is a measurement or a guess.
    // effectiveBitrateSource() cannot tell a fresh measurement from a
    // remembered one; the caller that supplied the value substitutes
    // Remembered when it came from the cache.
    enum class Source {
        UnlimitedLocal,
        Manual,
        Measured,
        Remembered,
        LocalEstimate,
        Estimate,
    };

    static constexpr qint64 AutoFallbackBitrate = 20'000'000;
    // A LAN is not automatically gigabit ethernet. This is the ceiling used on
    // a local network before a measurement lands: high enough that ordinary
    // 1080p and most 4K HEVC still direct play, low enough that a 2.4 GHz
    // link is not immediately asked for a 80 Mbps remux.
    static constexpr qint64 LocalNetworkFallbackBitrate = 40'000'000;
    static constexpr qint64 MaximumBitrate = 1'000'000'000;
    static constexpr qint64 MinimumBitrate = 1'000'000;
    // A remembered measurement describes a network, not a moment. Keep it long
    // enough to cover a normal viewing routine and short enough that a
    // rearranged home network re-measures on its own.
    static constexpr qint64 RememberedLifetimeMs = 7LL * 24 * 60 * 60 * 1000;

    static qint64 effectiveBitrate(qint64 manualBitrate, bool unlimitedLocalNetwork, bool endpointKnown,
        bool inLocalNetwork, qint64 measuredBitrate);
    static Source effectiveBitrateSource(qint64 manualBitrate, bool unlimitedLocalNetwork, bool endpointKnown,
        bool inLocalNetwork, qint64 measuredBitrate);
    static bool isEstimate(Source source);

    static qint64 conservativeEstimate(qint64 transferredBytes, qint64 elapsedMilliseconds);
    static bool shouldBenchmarkFourRequests(
        qint64 roundTripMilliseconds, qint64 singleRequestBitrate, qint64 dualRequestBitrate);
    static int selectParallelRequests(
        qint64 singleRequestBitrate, qint64 dualRequestBitrate, qint64 fourRequestBitrate = 0);

    // Identifies the route to one server well enough to reuse a measurement
    // across launches, and to notice that the route changed. Local interface
    // subnets move with the network; the transport tells Wi-Fi from cellular
    // when two networks happen to hand out the same private range.
    static QString networkSignature(
        const QString& serverAuthority, const QStringList& localAddresses, const QString& transportMedium);
    static bool isRememberedMeasurementUsable(qint64 recordedAtMsSinceEpoch, qint64 nowMsSinceEpoch);

    struct QualityOption {
        qint64 bitrate = 0;
        QString label;
    };

    static QString formatBitrate(qint64 bitsPerSecond);
    // The ladder a viewer can pick from, coarsest first. Rungs at or above the
    // source bitrate are dropped: they would transcode the stream without
    // lowering anything, which is the worst of both.
    static QList<QualityOption> qualityLadder(qint64 sourceBitrate);
    // The line under "Auto". jellyfin-web says only "Auto"; we know whether
    // the ceiling was measured, remembered for this network, or guessed.
    static QString describeAuto(Source source, qint64 effectiveBitrate, int parallelRequests);
};

} // namespace JellyfinNative
