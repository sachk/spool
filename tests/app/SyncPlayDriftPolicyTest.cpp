#include "app/SyncPlayController.h"

#include "TestMain.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace JellyfinNative;

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << message << '\n';
    std::exit(1);
}

bool near(double value, double expected, double tolerance = 0.001)
{
    return std::abs(value - expected) <= tolerance;
}

} // namespace

JELLYFIN_TEST_MAIN("sync-play-drift-policy")
{
    require(SyncPlayDriftPolicy::evaluate(59.0).method == SyncCorrection::Method::None,
        "drift below the speed threshold must not be corrected");
    require(SyncPlayDriftPolicy::evaluate(-59.0).method == SyncCorrection::Method::None,
        "a small lead must not be corrected either");

    // Web: speed = 1 + diff / speedToSyncDuration, held for one second.
    const SyncCorrection behind = SyncPlayDriftPolicy::evaluate(200.0);
    require(behind.method == SyncCorrection::Method::Speed, "a 200 ms lag should speed up");
    require(near(behind.speed, 1.2), "a 200 ms lag should ask for 1.2x");
    require(behind.durationMs == 1'000, "an uncapped correction runs for the web duration");

    // Beyond the rate cap the window stretches so the recovered distance holds.
    const SyncCorrection capped = SyncPlayDriftPolicy::evaluate(2'500.0);
    require(capped.method == SyncCorrection::Method::Speed, "drift under 3 s stays on SpeedToSync");
    require(near(capped.speed, 2.0), "the rate must be capped at 2x");
    require(capped.durationMs == 2'500, "the capped rate must recover the whole drift");

    const SyncCorrection ahead = SyncPlayDriftPolicy::evaluate(-600.0);
    require(ahead.method == SyncCorrection::Method::Speed, "a 600 ms lead should slow down");
    require(near(ahead.speed, 0.5), "the rate must be capped at 0.5x");
    require(ahead.durationMs == 1'200, "slowing to 0.5x for 1.2 s gives back 600 ms");

    // A large lead cannot be given back within the duration cap, so the
    // correction is partial and the next cycle finishes it.
    const SyncCorrection clipped = SyncPlayDriftPolicy::evaluate(-2'500.0);
    require(near(clipped.speed, 0.5), "a large lead still slows to the floor rate");
    require(clipped.durationMs == 3'000, "the correction window is capped");

    require(SyncPlayDriftPolicy::evaluate(3'000.0).method == SyncCorrection::Method::Skip,
        "drift at the speed ceiling must seek instead");
    require(SyncPlayDriftPolicy::evaluate(-3'500.0).method == SyncCorrection::Method::Skip,
        "a large lead must seek instead");

    for (const double diffMs : { 60.0, -60.0, 500.0, -500.0, 1'999.0, -1'500.0 }) {
        const SyncCorrection correction = SyncPlayDriftPolicy::evaluate(diffMs);
        require(correction.method == SyncCorrection::Method::Speed, "mid-range drift should speed correct");
        const double recovered = (correction.speed - 1.0) * correction.durationMs;
        require(near(recovered, diffMs, 1.0), "a speed correction must recover exactly the measured drift");
    }
    return 0;
}
