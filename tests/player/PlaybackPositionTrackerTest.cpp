#include "player/PlaybackPositionTracker.h"

#include <QDebug>

#include <cmath>
#include <cstdlib>

using JellyfinNative::PlaybackPositionTracker;

namespace {

void require(bool condition, const char *message) {
  if (condition)
    return;
  qCritical() << message;
  std::exit(EXIT_FAILURE);
}

bool near(double actual, double expected) {
  return std::abs(actual - expected) < 0.01;
}

} // namespace

int main() {
  PlaybackPositionTracker tracker;
  tracker.reset(12.0);
  tracker.setDuration(100.0);
  require(near(tracker.position(), 12.0), "resume position was not retained");
  require(near(tracker.clamp(120.0), 100.0), "duration clamp failed");

  tracker.update(40.0, PlaybackPositionTracker::Source::Lifecycle);
  tracker.update(5.0, PlaybackPositionTracker::Source::Mpv);
  require(near(tracker.position(), 40.0),
          "stale mpv regression was not rejected");

  tracker.beginSeek(10.0);
  tracker.update(10.0, PlaybackPositionTracker::Source::Seek);
  require(near(tracker.seekAnchor(true, false), 10.0),
          "seek target was not used as the anchor");

  tracker.reset(20.0);
  tracker.setDuration(100.0);
  tracker.beginSeek(80.0);
  tracker.update(80.0, PlaybackPositionTracker::Source::Seek);
  tracker.settleSeek();
  tracker.update(21.0, PlaybackPositionTracker::Source::Mpv);
  require(near(tracker.position(), 80.0),
          "stale forward-seek sample replaced optimistic target");
  tracker.update(78.0, PlaybackPositionTracker::Source::Mpv);
  require(near(tracker.position(), 78.0),
          "landed forward-seek sample was not accepted");

  tracker.beginSeek(20.0);
  tracker.update(20.0, PlaybackPositionTracker::Source::Seek);
  tracker.settleSeek();
  tracker.update(77.0, PlaybackPositionTracker::Source::Mpv);
  require(near(tracker.position(), 20.0),
          "stale backward-seek sample replaced optimistic target");
  tracker.update(22.0, PlaybackPositionTracker::Source::Mpv);
  require(near(tracker.position(), 22.0),
          "landed backward-seek sample was not accepted");

  tracker.allowRegression();
  tracker.update(4.0, PlaybackPositionTracker::Source::Mpv);
  require(near(tracker.position(), 4.0),
          "explicitly allowed regression was rejected");

  tracker.clear();
  require(near(tracker.position(), 0.0), "clear did not reset position");
  require(near(tracker.duration(), 0.0), "clear did not reset duration");
  return EXIT_SUCCESS;
}
