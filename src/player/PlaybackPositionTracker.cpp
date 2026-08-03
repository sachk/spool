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

void PlaybackPositionTracker::reset()
{
    m_positionSeconds = 0.0;
    m_durationSeconds = 0.0;
    m_requestedSeekTargetSeconds = -1.0;
    m_requestedSeekStartSeconds = -1.0;
    m_lastTrustedPositionSeconds = 0.0;
    m_hasMpvPosition = false;
    m_seekCommandClock.invalidate();
    m_positionRegressionAllowedClock.invalidate();
}

void PlaybackPositionTracker::clear()
{
    m_positionSeconds = 0.0;
    m_durationSeconds = 0.0;
    m_requestedSeekTargetSeconds = -1.0;
    m_requestedSeekStartSeconds = -1.0;
    m_lastTrustedPositionSeconds = 0.0;
    m_hasMpvPosition = false;
    m_seekCommandClock.invalidate();
    m_positionRegressionAllowedClock.invalidate();
}

double PlaybackPositionTracker::position() const
{
    return m_positionSeconds;
}

double PlaybackPositionTracker::duration() const
{
    return m_durationSeconds;
}

void PlaybackPositionTracker::setDuration(double seconds)
{
    m_durationSeconds = std::isfinite(seconds) ? qMax(0.0, seconds) : 0.0;
    m_positionSeconds = clamp(m_positionSeconds);
    m_lastTrustedPositionSeconds = clamp(m_lastTrustedPositionSeconds);
}

double PlaybackPositionTracker::clamp(double seconds) const
{
    if (!std::isfinite(seconds))
        return m_positionSeconds;
    if (m_durationSeconds > 0.0)
        return qBound(0.0, seconds, m_durationSeconds);
    return qMax(0.0, seconds);
}

bool PlaybackPositionTracker::update(double seconds)
{
    double clamped = clamp(seconds);
    bool landedSeek = false;
    if (m_requestedSeekTargetSeconds >= 0.0 && seekIsFresh(kSeekTargetFreshnessMs)) {
        const double midpoint = (m_requestedSeekStartSeconds + m_requestedSeekTargetSeconds) / 2.0;
        const bool movingForward = m_requestedSeekTargetSeconds > m_requestedSeekStartSeconds;
        const bool movingBackward = m_requestedSeekTargetSeconds < m_requestedSeekStartSeconds;
        if ((movingForward && clamped < midpoint) || (movingBackward && clamped > midpoint))
            return false;
        landedSeek = true;
        m_requestedSeekTargetSeconds = -1.0;
        m_requestedSeekStartSeconds = -1.0;
        m_seekCommandClock.invalidate();
    }
    if (!landedSeek && !regressionAllowed() && m_hasMpvPosition
        && m_lastTrustedPositionSeconds > kPositionRegressionToleranceSeconds
        && clamped + kPositionRegressionToleranceSeconds < m_lastTrustedPositionSeconds) {
        qInfo() << "player: ignoring stale mpv position"
                << "position=" << clamped << "ui=" << m_positionSeconds << "trusted=" << m_lastTrustedPositionSeconds;
        clamped = clamp(m_lastTrustedPositionSeconds);
    }

    m_lastTrustedPositionSeconds = clamped;
    m_hasMpvPosition = true;
    if (m_positionRegressionAllowedClock.isValid()
        && m_positionRegressionAllowedClock.elapsed() < kSeekTargetFreshnessMs) {
        m_positionRegressionAllowedClock.invalidate();
    }

    if (std::abs(m_positionSeconds - clamped) < 0.05)
        return false;

    m_positionSeconds = clamped;
    return true;
}

double PlaybackPositionTracker::seekAnchor()
{
    if (m_requestedSeekTargetSeconds >= 0.0 && seekIsFresh(kSeekTargetFreshnessMs))
        return clamp(m_requestedSeekTargetSeconds);

    return m_positionSeconds;
}

void PlaybackPositionTracker::beginSeek(double targetSeconds)
{
    m_requestedSeekStartSeconds = m_positionSeconds;
    m_requestedSeekTargetSeconds = clamp(targetSeconds);
    m_seekCommandClock.restart();
}

void PlaybackPositionTracker::cancelSeek()
{
    m_requestedSeekTargetSeconds = -1.0;
    m_requestedSeekStartSeconds = -1.0;
    m_seekCommandClock.invalidate();
}

bool PlaybackPositionTracker::seekIsFresh(qint64 freshnessMs) const
{
    return m_seekCommandClock.isValid() && m_seekCommandClock.elapsed() < freshnessMs;
}

void PlaybackPositionTracker::allowRegression()
{
    m_positionRegressionAllowedClock.restart();
}

bool PlaybackPositionTracker::regressionAllowed() const
{
    return m_positionRegressionAllowedClock.isValid()
        && m_positionRegressionAllowedClock.elapsed() < kSeekTargetFreshnessMs;
}

} // namespace JellyfinNative
