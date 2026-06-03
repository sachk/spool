#pragma once

#include "../common/JellyfinTypes.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariant>

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
    Q_PROPERTY(int audioDelayMs READ audioDelayMs WRITE setAudioDelayMs NOTIFY audioDelayMsChanged)
    Q_PROPERTY(QString audioOutputMode READ audioOutputMode WRITE setAudioOutputMode NOTIFY audioOutputModeChanged)
    Q_PROPERTY(QString activeSegmentType READ activeSegmentType NOTIFY stateChanged)
    Q_PROPERTY(double activeSegmentEndSeconds READ activeSegmentEndSeconds NOTIFY stateChanged)
    Q_PROPERTY(bool trickplayAvailable READ trickplayAvailable NOTIFY stateChanged)
    Q_PROPERTY(QStringList trickplaySheetUrls READ trickplaySheetUrls NOTIFY stateChanged)

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
    int audioDelayMs() const;
    QString audioOutputMode() const;
    QString activeSegmentType() const;
    double activeSegmentEndSeconds() const;
    bool trickplayAvailable() const;
    QStringList trickplaySheetUrls() const;
    Q_INVOKABLE void skipActiveSegment();
    Q_INVOKABLE QVariantMap trickplayForSeconds(double seconds) const;

    Q_INVOKABLE void play(const JellyfinNative::PlaybackSession &session);
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void seekBack();
    Q_INVOKABLE void seekForward();
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void previewSeekBy(double deltaSeconds);
    void pauseForBackground();
    Q_INVOKABLE void toggleDebugOsd();
    Q_INVOKABLE void toggleSubtitles();
    Q_INVOKABLE void cycleSubtitles();
    Q_INVOKABLE void enableSubtitles();
    Q_INVOKABLE void selectSubtitle(int index);
    Q_INVOKABLE void cycleAudio();
    Q_INVOKABLE void selectAudio(int index);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void stopWithReason(const QString &reason);
    Q_INVOKABLE void setNightModeEnabled(bool enabled);
    Q_INVOKABLE void setAudioDelayMs(int delayMs);
    Q_INVOKABLE void setAudioOutputMode(const QString &mode);
    void setSubtitlePreferences(const JellyfinNative::SubtitlePreferences &preferences);

signals:
    void visibleChanged();
    void stateChanged();
    void playbackStopped(const QString &itemId, qint64 positionTicks);
    void nightModeEnabledChanged();
    void audioDelayMsChanged();
    void audioOutputModeChanged();

public:
    // Called from main on aboutToQuit so we tear down before the scene graph
    // stops accepting render jobs. Safe to call repeatedly.
    void teardownMpv();

private:
    enum class MpvOptionApplyMode {
        Initial,
        Runtime,
    };

    enum class MpvRuntimeOption {
        NightMode,
        AudioDelay,
    };

    bool ensureMpv();
    void scheduleMpvTeardown();
    void runEventLoop();
    void startProgressReporting();
    void stopProgressReporting(bool failed = false);
    bool mpvCommand(const char *command);
    bool beginSeekCommand(double targetSeconds, const QByteArray &flags,
                          bool markSeeking = true);
    bool beginRelativeSeekCommand(double deltaSeconds);
    double seekAnchorPosition();
    bool currentMpvPositionSeconds(double *seconds) const;
    double projectedPositionSeconds() const;
    QByteArray buildSeekCommand(double targetSeconds, const QByteArray &flags) const;
    void updatePlaybackStatusText();
    void setPositionSeconds(double seconds);
    double playbackPositionFromMpvTime(double seconds) const;
    double clampedPosition(double seconds) const;
    void resetPlaybackUiState();
    bool applyMpvRuntimeOption(MpvRuntimeOption option, MpvOptionApplyMode mode, mpv_handle *handle);
    bool applyMpvSubtitleOptions(MpvOptionApplyMode mode, mpv_handle *handle);
    bool applyMpvRuntimeOptions(MpvOptionApplyMode mode, mpv_handle *handle);

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
    QElapsedTimer m_seekCommandClock;
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
    double m_requestedSeekTargetSeconds = -1.0;
    std::atomic_bool m_nightModeEnabled = false;
    std::atomic<int> m_audioDelayMs = 0;
    QString m_audioOutputMode = QStringLiteral("alsa");
    SubtitlePreferences m_subtitlePreferences;
    QString m_activeSegmentType;
    double m_activeSegmentEndSeconds = 0.0;
};

} // namespace JellyfinNative
