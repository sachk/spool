#pragma once

#include <cstdint>
#include <vector>

namespace JellyfinNative {

struct SyncPlayTimeMeasurement {
    std::int64_t localRequestSentMs = 0;
    std::int64_t serverRequestReceivedMs = 0;
    std::int64_t serverResponseSentMs = 0;
    std::int64_t localResponseReceivedMs = 0;

    double offsetMs() const;
    double roundTripDelayMs() const;
    double pingMs() const;
};

class SyncPlayClock final
{
public:
    void reset();
    void addMeasurement(const SyncPlayTimeMeasurement &measurement);

    bool ready() const;
    double offsetMs() const;
    double pingMs() const;
    std::int64_t localDelayUntil(std::int64_t serverTimeMs,
                                 std::int64_t localNowMs) const;
    std::int64_t estimatePositionTicks(std::int64_t positionTicks,
                                       std::int64_t serverTimeMs,
                                       std::int64_t localNowMs) const;

private:
    void selectBestMeasurement();

    std::vector<SyncPlayTimeMeasurement> m_measurements;
    SyncPlayTimeMeasurement m_best;
    bool m_ready = false;
};

} // namespace JellyfinNative
