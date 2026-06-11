#include "SyncPlayClock.h"

#include <algorithm>
#include <cmath>

namespace JellyfinNative {

namespace {

constexpr int kMaxMeasurements = 8;
constexpr std::int64_t kTicksPerMillisecond = 10'000;

} // namespace

double SyncPlayTimeMeasurement::offsetMs() const
{
    return ((serverRequestReceivedMs - localRequestSentMs) +
            (serverResponseSentMs - localResponseReceivedMs)) /
           2.0;
}

double SyncPlayTimeMeasurement::roundTripDelayMs() const
{
    return (localResponseReceivedMs - localRequestSentMs) -
           (serverResponseSentMs - serverRequestReceivedMs);
}

double SyncPlayTimeMeasurement::pingMs() const
{
    return std::max(0.0, roundTripDelayMs() / 2.0);
}

void SyncPlayClock::reset()
{
    m_measurements.clear();
    m_best = {};
    m_ready = false;
}

void SyncPlayClock::addMeasurement(
    const SyncPlayTimeMeasurement &measurement)
{
    m_measurements.push_back(measurement);
    while (m_measurements.size() > kMaxMeasurements)
        m_measurements.erase(m_measurements.begin());
    selectBestMeasurement();
}

bool SyncPlayClock::ready() const
{
    return m_ready;
}

double SyncPlayClock::offsetMs() const
{
    return m_ready ? m_best.offsetMs() : 0.0;
}

double SyncPlayClock::pingMs() const
{
    return m_ready ? m_best.pingMs() : 0.0;
}

std::int64_t SyncPlayClock::localDelayUntil(std::int64_t serverTimeMs,
                                            std::int64_t localNowMs) const
{
    const std::int64_t localTargetMs =
        serverTimeMs - static_cast<std::int64_t>(std::llround(offsetMs()));
    return std::max<std::int64_t>(0, localTargetMs - localNowMs);
}

std::int64_t SyncPlayClock::estimatePositionTicks(
    std::int64_t positionTicks, std::int64_t serverTimeMs,
    std::int64_t localNowMs) const
{
    const std::int64_t serverNowMs =
        localNowMs + static_cast<std::int64_t>(std::llround(offsetMs()));
    const std::int64_t elapsedMs =
        std::max<std::int64_t>(0, serverNowMs - serverTimeMs);
    return positionTicks + elapsedMs * kTicksPerMillisecond;
}

void SyncPlayClock::selectBestMeasurement()
{
    if (m_measurements.empty()) {
        m_ready = false;
        return;
    }

    m_best = *std::min_element(
        m_measurements.cbegin(), m_measurements.cend(),
        [](const SyncPlayTimeMeasurement &left,
           const SyncPlayTimeMeasurement &right) {
            return left.roundTripDelayMs() < right.roundTripDelayMs();
        });
    m_ready = true;
}

} // namespace JellyfinNative
