#include "PlaybackPositionTracker.h"

#include <QDebug>
#include <QtGlobal>

#include <cmath>

namespace JellyfinNative {

namespace {

constexpr qint64 kSeekTargetFreshnessMs = 10000;
constexpr qint64 kMpvPostSeekRegressionMs = 5000;
constexpr double kPositionRegressionToleranceSeconds = 3.0;

} // namespace

void PlaybackPositionTracker::reset(double startSeconds) {
  m_positionSeconds =
      std::isfinite(startSeconds) ? qMax(0.0, startSeconds) : 0.0;
  m_durationSeconds = 0.0;
  m_requestedSeekTargetSeconds = -1.0;
  m_lastTrustedPositionSeconds = m_positionSeconds;
  m_positionClock.invalidate();
  m_seekCommandClock.invalidate();
  m_lastTrustedPositionClock.restart();
  m_positionRegressionAllowedClock.invalidate();
}

void PlaybackPositionTracker::clear() {
  m_positionSeconds = 0.0;
  m_durationSeconds = 0.0;
  m_requestedSeekTargetSeconds = -1.0;
  m_lastTrustedPositionSeconds = 0.0;
  m_positionClock.invalidate();
  m_seekCommandClock.invalidate();
  m_lastTrustedPositionClock.invalidate();
  m_positionRegressionAllowedClock.invalidate();
}

double PlaybackPositionTracker::position() const { return m_positionSeconds; }

double PlaybackPositionTracker::duration() const { return m_durationSeconds; }

void PlaybackPositionTracker::setDuration(double seconds) {
  m_durationSeconds = std::isfinite(seconds) ? qMax(0.0, seconds) : 0.0;
  update(m_positionSeconds, Source::Lifecycle);
}

double PlaybackPositionTracker::clamp(double seconds) const {
  if (!std::isfinite(seconds))
    return m_positionSeconds;
  if (m_durationSeconds > 0.0)
    return qBound(0.0, seconds, m_durationSeconds);
  return qMax(0.0, seconds);
}

bool PlaybackPositionTracker::update(double seconds, Source source) {
  double clamped = clamp(seconds);
  if (!regressionAllowed(source) && m_lastTrustedPositionClock.isValid() &&
      m_lastTrustedPositionSeconds > kPositionRegressionToleranceSeconds &&
      clamped + kPositionRegressionToleranceSeconds <
          m_lastTrustedPositionSeconds) {
    if (source == Source::Mpv) {
      qWarning() << "player: ignoring stale mpv position"
                 << "position=" << clamped << "ui=" << m_positionSeconds
                 << "trusted=" << m_lastTrustedPositionSeconds;
    }
    clamped = clamp(m_lastTrustedPositionSeconds);
  }

  m_lastTrustedPositionSeconds = clamped;
  m_lastTrustedPositionClock.restart();
  if (source == Source::Mpv && m_positionRegressionAllowedClock.isValid() &&
      m_positionRegressionAllowedClock.elapsed() < kSeekTargetFreshnessMs) {
    m_positionRegressionAllowedClock.invalidate();
  }

  if (std::abs(m_positionSeconds - clamped) < 0.05) {
    m_positionClock.restart();
    return false;
  }

  m_positionSeconds = clamped;
  m_positionClock.restart();
  return true;
}

double PlaybackPositionTracker::projected(bool paused, bool buffering) const {
  double position = m_positionSeconds;
  if (!paused && !buffering && m_positionClock.isValid()) {
    const double elapsed = m_positionClock.elapsed() / 1000.0;
    if (elapsed > 0.0)
      position += elapsed;
  }
  return clamp(position);
}

double PlaybackPositionTracker::seekAnchor(bool paused, bool buffering) {
  if (m_requestedSeekTargetSeconds >= 0.0 &&
      seekIsFresh(kSeekTargetFreshnessMs)) {
    double position = m_requestedSeekTargetSeconds;
    if (!paused && !buffering)
      position += m_seekCommandClock.elapsed() / 1000.0;
    return clamp(position);
  }

  restoreTrusted("seek-anchor");
  return projected(paused, buffering);
}

bool PlaybackPositionTracker::restoreTrusted(const char *reason) {
  if (!m_lastTrustedPositionClock.isValid())
    return false;

  const double trusted = clamp(m_lastTrustedPositionSeconds);
  if (m_positionSeconds + 0.05 >= trusted)
    return false;

  qInfo() << "player: restoring trusted playback position"
          << (reason ? reason : "unknown") << "from=" << m_positionSeconds
          << "to=" << trusted;
  return update(trusted, Source::Lifecycle);
}

void PlaybackPositionTracker::beginSeek(double targetSeconds) {
  m_requestedSeekTargetSeconds = clamp(targetSeconds);
  m_seekCommandClock.restart();
}

void PlaybackPositionTracker::settleSeek() {
  m_requestedSeekTargetSeconds = -1.0;
  m_positionClock.restart();
}

void PlaybackPositionTracker::cancelSeek() {
  m_requestedSeekTargetSeconds = -1.0;
  m_seekCommandClock.invalidate();
  m_positionClock.restart();
}

bool PlaybackPositionTracker::seekIsFresh(qint64 freshnessMs) const {
  return m_seekCommandClock.isValid() &&
         m_seekCommandClock.elapsed() < freshnessMs;
}

void PlaybackPositionTracker::allowRegression() {
  m_positionRegressionAllowedClock.restart();
}

void PlaybackPositionTracker::restartProjection() { m_positionClock.restart(); }

void PlaybackPositionTracker::invalidateProjection() {
  m_positionClock.invalidate();
}

bool PlaybackPositionTracker::projectionIsValid() const {
  return m_positionClock.isValid();
}

bool PlaybackPositionTracker::regressionAllowed(Source source) const {
  if (source == Source::Seek)
    return true;

  if (source == Source::Mpv && seekIsFresh(kMpvPostSeekRegressionMs)) {
    return true;
  }

  return m_positionRegressionAllowedClock.isValid() &&
         m_positionRegressionAllowedClock.elapsed() < kSeekTargetFreshnessMs;
}

} // namespace JellyfinNative
