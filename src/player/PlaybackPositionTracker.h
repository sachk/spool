#pragma once

#include <QElapsedTimer>

namespace JellyfinNative {

class PlaybackPositionTracker final {
public:
    enum class Source {
        Projection,
        Mpv,
        Seek,
        Lifecycle,
    };

    void reset(double startSeconds);
    void clear();

    double position() const;
    double duration() const;
    void setDuration(double seconds);
    double clamp(double seconds) const;

    bool update(double seconds, Source source);
    double projected(bool paused, bool buffering) const;
    double seekAnchor(bool paused, bool buffering);
    bool restoreTrusted(const char *reason);

    void beginSeek(double targetSeconds);
    void settleSeek();
    void cancelSeek();
    bool seekIsFresh(qint64 freshnessMs) const;
    void allowRegression();
    void restartProjection();
    void invalidateProjection();
    bool projectionIsValid() const;

private:
    bool regressionAllowed(Source source) const;

    double m_positionSeconds = 0.0;
    double m_durationSeconds = 0.0;
    double m_requestedSeekTargetSeconds = -1.0;
    double m_requestedSeekStartSeconds = -1.0;
    double m_lastTrustedPositionSeconds = 0.0;
    QElapsedTimer m_positionClock;
    QElapsedTimer m_seekCommandClock;
    QElapsedTimer m_lastTrustedPositionClock;
    QElapsedTimer m_positionRegressionAllowedClock;
};

} // namespace JellyfinNative
