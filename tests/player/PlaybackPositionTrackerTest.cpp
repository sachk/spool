#include "player/PlaybackPositionTracker.h"

#include "TestMain.h"

#include <QDebug>

#include <cmath>
#include <cstdlib>

using JellyfinNative::PlaybackPositionTracker;

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    qCritical() << message;
    std::exit(EXIT_FAILURE);
}

bool near(double actual, double expected)
{
    return std::abs(actual - expected) < 0.01;
}

} // namespace

JELLYFIN_TEST_MAIN("playback-position-tracker")
{
    PlaybackPositionTracker tracker;
    tracker.reset();
    tracker.setDuration(100.0);
    require(near(tracker.position(), 0.0), "reset invented a position before mpv reported one");
    require(near(tracker.clamp(120.0), 100.0), "duration clamp failed");

    tracker.update(40.0);
    tracker.update(5.0);
    require(near(tracker.position(), 40.0), "stale mpv regression was not rejected");

    tracker.beginSeek(10.0);
    tracker.update(10.0);
    require(near(tracker.seekAnchor(), 10.0), "seek target was not used as the anchor");

    tracker.reset();
    tracker.setDuration(100.0);
    tracker.update(20.0);
    tracker.beginSeek(80.0);
    tracker.update(21.0);
    require(near(tracker.position(), 20.0), "stale forward-seek sample changed the last mpv position");
    tracker.update(78.0);
    require(near(tracker.position(), 78.0), "landed forward-seek sample was not accepted");

    tracker.beginSeek(20.0);
    tracker.update(77.0);
    require(near(tracker.position(), 78.0), "stale backward-seek sample changed the last mpv position");
    tracker.update(22.0);
    require(near(tracker.position(), 22.0), "landed backward-seek sample was not accepted");
    tracker.beginSeek(50.0);
    require(near(tracker.seekAnchor(), 50.0), "seek anchor extrapolated beyond the requested mpv target");
    tracker.cancelSeek();

    tracker.allowRegression();
    tracker.update(4.0);
    require(near(tracker.position(), 4.0), "explicitly allowed regression was rejected");

    tracker.clear();
    require(near(tracker.position(), 0.0), "clear did not reset position");
    require(near(tracker.duration(), 0.0), "clear did not reset duration");
    return EXIT_SUCCESS;
}
