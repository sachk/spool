#pragma once

#include <QElapsedTimer>

namespace JellyfinNative {

class PlaybackPositionTracker final {
public:
    // Seeded with where playback is about to resume from, so a restart shows
    // the position it is starting at rather than zero until mpv reports in.
    void reset(double startSeconds = 0.0);
    void clear();

    double position() const;
    double estimatedPosition(double playbackSpeed, bool advancing) const;
    double duration() const;
    void setDuration(double seconds);
    double clamp(double seconds) const;

    bool update(double seconds);
    double seekAnchor();

    void beginSeek(double targetSeconds);
    void cancelSeek();
    bool seekIsFresh(qint64 freshnessMs) const;
    void allowRegression();

private:
    bool regressionAllowed() const;

    double m_positionSeconds = 0.0;
    double m_durationSeconds = 0.0;
    double m_requestedSeekTargetSeconds = -1.0;
    double m_requestedSeekStartSeconds = -1.0;
    double m_lastTrustedPositionSeconds = 0.0;
    bool m_hasMpvPosition = false;
    // A rejected position repeats with every mpv tick until it catches up.
    // Only the first of a run is worth a line in the log.
    bool m_ignoringStalePositions = false;
    QElapsedTimer m_positionClock;
    QElapsedTimer m_seekCommandClock;
    QElapsedTimer m_positionRegressionAllowedClock;
};

} // namespace JellyfinNative
