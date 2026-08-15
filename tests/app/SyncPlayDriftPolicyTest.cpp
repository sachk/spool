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
    require(SyncPlayDriftPolicy::evaluate(99.0).method == SyncCorrection::Method::None,
        "drift below the speed threshold must not be corrected");
    require(SyncPlayDriftPolicy::evaluate(-99.0).method == SyncCorrection::Method::None,
        "a small lead must not be corrected either");

    const SyncCorrection behind = SyncPlayDriftPolicy::evaluate(200.0);
    require(behind.method == SyncCorrection::Method::Speed, "a 200 ms lag should speed up");
    require(near(behind.speed, 1.03), "a 200 ms lag should use the bounded mpv correction rate");
    require(behind.durationMs == 6'667, "the bounded correction should recover the measured lag");

    const SyncCorrection nearSeek = SyncPlayDriftPolicy::evaluate(399.0);
    require(nearSeek.method == SyncCorrection::Method::Speed, "drift below 400 ms should speed correct");
    require(near(nearSeek.speed, 1.03), "near-threshold drift should retain the bounded rate");
    require(nearSeek.durationMs == 10'000, "the correction window must remain bounded");

    const SyncCorrection ahead = SyncPlayDriftPolicy::evaluate(-200.0);
    require(ahead.method == SyncCorrection::Method::Speed, "a 200 ms lead should slow down");
    require(near(ahead.speed, 0.97), "a lead should use the symmetric bounded rate");
    require(ahead.durationMs == 6'667, "the bounded correction should give back the measured lead");

    require(SyncPlayDriftPolicy::evaluate(400.0).method == SyncCorrection::Method::Skip,
        "drift at the speed ceiling must seek instead");
    require(
        SyncPlayDriftPolicy::evaluate(-500.0).method == SyncCorrection::Method::Skip, "a large lead must seek instead");

    for (const double diffMs : { 100.0, -100.0, 200.0, -200.0, 300.0, -300.0 }) {
        const SyncCorrection correction = SyncPlayDriftPolicy::evaluate(diffMs);
        require(correction.method == SyncCorrection::Method::Speed, "mid-range drift should speed correct");
        const double recovered = (correction.speed - 1.0) * correction.durationMs;
        require(near(recovered, diffMs, 1.0), "an unclipped speed correction must recover the measured drift");
    }
    return 0;
}
