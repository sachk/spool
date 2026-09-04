#include "PlaybackBandwidthPolicy.h"

#include "common/NetworkAddress.h"

#include <QCryptographicHash>
#include <QHostAddress>

#include <algorithm>
#include <cmath>

namespace JellyfinNative {

PlaybackBandwidthPolicy::Source PlaybackBandwidthPolicy::effectiveBitrateSource(
    qint64 manualBitrate, bool unlimitedLocalNetwork, bool endpointKnown, bool inLocalNetwork, qint64 measuredBitrate)
{
    if (unlimitedLocalNetwork && endpointKnown && inLocalNetwork)
        return Source::UnlimitedLocal;
    if (manualBitrate > 0)
        return Source::Manual;
    if (measuredBitrate > 0)
        return Source::Measured;
    if (endpointKnown && inLocalNetwork)
        return Source::LocalEstimate;
    return Source::Estimate;
}

bool PlaybackBandwidthPolicy::isEstimate(Source source)
{
    return source == Source::LocalEstimate || source == Source::Estimate;
}

qint64 PlaybackBandwidthPolicy::effectiveBitrate(
    qint64 manualBitrate, bool unlimitedLocalNetwork, bool endpointKnown, bool inLocalNetwork, qint64 measuredBitrate)
{
    if (unlimitedLocalNetwork && endpointKnown && inLocalNetwork)
        return MaximumBitrate;
    if (manualBitrate > 0)
        return std::clamp<qint64>(manualBitrate, MinimumBitrate, MaximumBitrate);
    if (measuredBitrate > 0)
        return std::clamp<qint64>(measuredBitrate, MinimumBitrate, MaximumBitrate);
    // A local route is not evidence of a fast one. Until something is
    // measured, assume a plausible wireless link rather than gigabit ethernet.
    if (endpointKnown && inLocalNetwork)
        return LocalNetworkFallbackBitrate;
    return AutoFallbackBitrate;
}

qint64 PlaybackBandwidthPolicy::conservativeEstimate(qint64 transferredBytes, qint64 elapsedMilliseconds)
{
    if (transferredBytes <= 0 || elapsedMilliseconds <= 0)
        return 0;
    const long double measuredBitsPerSecond
        = static_cast<long double>(transferredBytes) * 8'000.0L / static_cast<long double>(elapsedMilliseconds);
    const long double conservative = measuredBitsPerSecond * 0.75L;
    return std::clamp<qint64>(static_cast<qint64>(std::llround(conservative)), MinimumBitrate, MaximumBitrate);
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

QString PlaybackBandwidthPolicy::networkSignature(
    const QString& serverAuthority, const QStringList& localAddresses, const QString& transportMedium)
{
    // Only the network part of each private address takes part: a DHCP lease
    // that hands out a different host on the same network is the same route,
    // and a public address would put the user's own address in the key.
    QStringList networks;
    for (const QString& candidate : localAddresses) {
        QHostAddress address;
        if (!address.setAddress(candidate.trimmed()) || address.isLoopback())
            continue;
        if (!isPrivateNetworkAddress(address))
            continue;
        if (address.protocol() == QAbstractSocket::IPv4Protocol) {
            const quint32 value = address.toIPv4Address() & 0xffffff00u;
            networks.push_back(QHostAddress(value).toString());
        } else if (address.protocol() == QAbstractSocket::IPv6Protocol) {
            networks.push_back(address.toString().section(QLatin1Char(':'), 0, 3));
        }
    }
    networks.removeDuplicates();
    networks.sort();

    const QString material = serverAuthority.trimmed().toLower() + QLatin1Char('|')
        + transportMedium.trimmed().toLower() + QLatin1Char('|') + networks.join(QLatin1Char(','));
    return QString::fromLatin1(
        QCryptographicHash::hash(material.toUtf8(), QCryptographicHash::Sha256).toHex().left(32));
}

QString PlaybackBandwidthPolicy::formatBitrate(qint64 bitsPerSecond)
{
    if (bitsPerSecond <= 0)
        return QStringLiteral("unknown");
    const double mbps = static_cast<double>(bitsPerSecond) / 1'000'000.0;
    if (mbps >= 10.0)
        return QString::number(std::llround(mbps)) + QStringLiteral(" Mbps");
    if (mbps >= 1.0)
        return QString::number(mbps, 'f', 1) + QStringLiteral(" Mbps");
    return QString::number(std::llround(mbps * 1000.0)) + QStringLiteral(" kbps");
}

QList<int> PlaybackBandwidthPolicy::resolutionLadder()
{
    return { 2160, 1080, 720, 480, 360 };
}

QString PlaybackBandwidthPolicy::describeResolution(int height)
{
    if (height <= 0)
        return QStringLiteral("Source");
    // 2160p is universally sold as 4K, and nothing else in the ladder is.
    if (height >= 2160)
        return QStringLiteral("4K");
    return QString::number(height) + QLatin1Char('p');
}

QList<PlaybackBandwidthPolicy::QualityOption> PlaybackBandwidthPolicy::qualityLadder(qint64 sourceBitrate)
{
    struct Rung {
        qint64 bitrate;
        int height;
    };
    // The rungs jellyfin-web offers, so a viewer who knows one client is not
    // surprised by the other. The height is carried rather than spelled, so
    // the rung the viewer picked is the rung the server is asked for.
    static constexpr Rung rungs[] = {
        { 120'000'000, 2160 },
        { 80'000'000, 2160 },
        { 60'000'000, 1080 },
        { 40'000'000, 1080 },
        { 20'000'000, 1080 },
        { 15'000'000, 1080 },
        { 10'000'000, 720 },
        { 8'000'000, 720 },
        { 6'000'000, 720 },
        { 4'000'000, 480 },
        { 3'000'000, 480 },
        { 2'000'000, 480 },
        { 1'000'000, 360 },
    };

    QList<QualityOption> options;
    options.reserve(std::size(rungs));
    for (const Rung& rung : rungs) {
        if (sourceBitrate > 0 && rung.bitrate >= sourceBitrate)
            continue;
        options.push_back(QualityOption { rung.bitrate, rung.height,
            describeResolution(rung.height) + QStringLiteral(" · ") + formatBitrate(rung.bitrate) });
    }
    return options;
}

QString PlaybackBandwidthPolicy::describeAuto(Source source, qint64 effectiveBitrate, int parallelRequests)
{
    const QString ceiling = formatBitrate(effectiveBitrate);
    const QString lanes = parallelRequests > 1 ? QStringLiteral(" · %1 connections").arg(parallelRequests) : QString();
    switch (source) {
    case Source::UnlimitedLocal:
        return QStringLiteral("No limit on this network");
    case Source::Manual:
        return QStringLiteral("Limited to %1 in Settings").arg(ceiling);
    case Source::Measured:
        return QStringLiteral("Measured %1").arg(ceiling) + lanes;
    case Source::Remembered:
        return QStringLiteral("%1 remembered for this network").arg(ceiling) + lanes;
    case Source::LocalEstimate:
        return QStringLiteral("Estimated %1 · local network, not measured yet").arg(ceiling);
    case Source::Estimate:
        break;
    }
    return QStringLiteral("Estimated %1 · not measured yet").arg(ceiling);
}

bool PlaybackBandwidthPolicy::isRememberedMeasurementUsable(qint64 recordedAtMsSinceEpoch, qint64 nowMsSinceEpoch)
{
    if (recordedAtMsSinceEpoch <= 0)
        return false;
    // A record from the future means the clock moved; treat it as unusable
    // rather than trusting it forever.
    const qint64 age = nowMsSinceEpoch - recordedAtMsSinceEpoch;
    return age >= 0 && age <= RememberedLifetimeMs;
}

} // namespace JellyfinNative
