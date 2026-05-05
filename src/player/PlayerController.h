#pragma once

#include "../common/JellyfinTypes.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QTimer>

#include <atomic>
#include <thread>

struct mpv_handle;

namespace JellyfinNative {

class JellyfinApiFacade;
class NativeAppWindow;

class PlayerController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY stateChanged)
    Q_PROPERTY(QString title READ title NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
    Q_PROPERTY(bool buffering READ buffering NOTIFY stateChanged)
    Q_PROPERTY(int bufferingPercent READ bufferingPercent NOTIFY stateChanged)
    Q_PROPERTY(bool seeking READ seeking NOTIFY stateChanged)
    Q_PROPERTY(bool debugOsdVisible READ debugOsdVisible NOTIFY stateChanged)
    Q_PROPERTY(bool subtitlesEnabled READ subtitlesEnabled NOTIFY stateChanged)
    Q_PROPERTY(QStringList subtitleTracks READ subtitleTracks NOTIFY stateChanged)
    Q_PROPERTY(int selectedSubtitleIndex READ selectedSubtitleIndex NOTIFY stateChanged)
    Q_PROPERTY(QStringList audioTracks READ audioTracks NOTIFY stateChanged)
    Q_PROPERTY(int selectedAudioIndex READ selectedAudioIndex NOTIFY stateChanged)
    Q_PROPERTY(bool backAllowed READ backAllowed NOTIFY stateChanged)
    Q_PROPERTY(double positionSeconds READ positionSeconds NOTIFY stateChanged)
    Q_PROPERTY(double durationSeconds READ durationSeconds NOTIFY stateChanged)
    Q_PROPERTY(bool nightModeEnabled READ nightModeEnabled WRITE setNightModeEnabled NOTIFY nightModeEnabledChanged)

public:
    PlayerController(NativeAppWindow *window, JellyfinApiFacade *api, QObject *parent = nullptr);
    ~PlayerController() override;

    bool visible() const;
    bool paused() const;
    QString title() const;
    QString statusText() const;
    QString errorText() const;
    bool buffering() const;
    int bufferingPercent() const;
    bool seeking() const;
    bool debugOsdVisible() const;
    bool subtitlesEnabled() const;
    QStringList subtitleTracks() const;
    int selectedSubtitleIndex() const;
    QStringList audioTracks() const;
    int selectedAudioIndex() const;
    bool backAllowed() const;
    double positionSeconds() const;
    double durationSeconds() const;
    bool nightModeEnabled() const;

    Q_INVOKABLE void play(const JellyfinNative::PlaybackSession &session);
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void seekBack();
    Q_INVOKABLE void seekForward();
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void toggleDebugOsd();
    Q_INVOKABLE void toggleSubtitles();
    Q_INVOKABLE void selectSubtitle(int index);
    Q_INVOKABLE void selectAudio(int index);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void stopWithReason(const QString &reason);
    Q_INVOKABLE void setNightModeEnabled(bool enabled);

signals:
    void visibleChanged();
    void stateChanged();
    void playbackStopped();
    void nightModeEnabledChanged();

private:
    bool ensureMpv();
    void teardownMpv();
    void scheduleMpvTeardown();
    void runEventLoop();
    void startProgressReporting();
    void stopProgressReporting(bool failed = false);
    bool mpvCommand(const char *command);
    bool beginSeekCommand(const QByteArray &command, double targetSeconds);
    bool beginRelativeSeekCommand(double deltaSeconds);
    void dispatchPendingSeek();
    double seekBasePosition() const;
    void updatePlaybackStatusText();
    void setPositionSeconds(double seconds);
    double playbackPositionFromMpvTime(double seconds) const;
    double clampedPosition(double seconds) const;
    void resetPlaybackUiState();

    NativeAppWindow *m_window = nullptr;
    JellyfinApiFacade *m_api = nullptr;
    PlaybackSession m_session;
    std::thread m_eventThread;
    std::atomic_bool m_terminating { false };
    // Tracks loadfile calls whose FILE_LOADED has not yet arrived. When > 0,
    // an END_FILE event belongs to a file being replaced and must not tear
    // down the UI.
    std::atomic<int> m_pendingFileLoads { 0 };
    std::atomic<mpv_handle *> m_mpv { nullptr };
    QTimer m_progressTimer;
    QTimer m_backGuardTimer;
    QTimer m_uiPositionTimer;
    QTimer m_seekWatchdogTimer;
    QElapsedTimer m_positionClock;
    bool m_visible = false;
    bool m_paused = false;
    bool m_buffering = false;
    int m_bufferingPercent = 0;
    bool m_seeking = false;
    bool m_debugOsdVisible = false;
    bool m_subtitlesEnabled = true;
    QStringList m_subtitleTracks { QStringLiteral("Off") };
    QList<int> m_subtitleIds { -1 };
    int m_selectedSubtitleIndex = 0;
    QStringList m_audioTracks;
    QList<int> m_audioIds;
    int m_selectedAudioIndex = -1;
    bool m_backAllowed = true;
    QString m_title;
    QString m_statusText = QStringLiteral("Ready");
    QString m_errorText;
    double m_positionSeconds = 0.0;
    double m_durationSeconds = 0.0;
    double m_resumeStartSeconds = 0.0;
    QByteArray m_pendingSeekCommand;
    double m_pendingSeekTargetSeconds = 0.0;
    double m_requestedSeekTargetSeconds = -1.0;
    std::atomic_bool m_nightModeEnabled = false;
};

} // namespace JellyfinNative
