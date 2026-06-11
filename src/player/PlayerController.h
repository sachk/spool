#pragma once

#include "../common/JellyfinTypes.h"
#include "MpvLifecycle.h"
#include "PlaybackPositionTracker.h"
#include "PlaybackReporter.h"
#include "PlaybackTimeline.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariant>

#include <atomic>

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
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY chaptersChanged)
    Q_PROPERTY(bool hasChapters READ hasChapters NOTIFY chaptersChanged)
    Q_PROPERTY(int currentChapter READ currentChapter NOTIFY stateChanged)
    Q_PROPERTY(bool nightModeEnabled READ nightModeEnabled WRITE setNightModeEnabled NOTIFY nightModeEnabledChanged)
    Q_PROPERTY(bool toneMappingVisualizationEnabled READ toneMappingVisualizationEnabled
               WRITE setToneMappingVisualizationEnabled
               NOTIFY toneMappingVisualizationEnabledChanged)
    Q_PROPERTY(int audioDelayMs READ audioDelayMs WRITE setAudioDelayMs NOTIFY audioDelayMsChanged)
    Q_PROPERTY(QString audioOutputMode READ audioOutputMode WRITE setAudioOutputMode NOTIFY audioOutputModeChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
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
    QVariantList chapters() const;
    bool hasChapters() const;
    int currentChapter() const;
    bool nightModeEnabled() const;
    bool toneMappingVisualizationEnabled() const;
    int audioDelayMs() const;
    QString audioOutputMode() const;
    int volume() const;
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
    void prepareForBackground();
    void pauseForBackground();
    void resyncForForeground();
    Q_INVOKABLE void toggleDebugOsd();
    Q_INVOKABLE void toggleSubtitles();
    Q_INVOKABLE void cycleSubtitles();
    Q_INVOKABLE void enableSubtitles();
    Q_INVOKABLE void selectSubtitle(int index);
    Q_INVOKABLE void cycleAudio();
    Q_INVOKABLE void selectAudio(int index);
    Q_INVOKABLE void nextChapter();
    Q_INVOKABLE void previousChapter();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void stopWithReason(const QString &reason);
    Q_INVOKABLE void setNightModeEnabled(bool enabled);
    Q_INVOKABLE void setToneMappingVisualizationEnabled(bool enabled);
    Q_INVOKABLE void setAudioDelayMs(int delayMs);
    Q_INVOKABLE void setAudioOutputMode(const QString &mode);
    Q_INVOKABLE void setVolume(int volume);
    Q_INVOKABLE void adjustVolume(int delta);
    void setSubtitlePreferences(const JellyfinNative::SubtitlePreferences &preferences);

signals:
    void visibleChanged();
    void stateChanged();
    void chaptersChanged();
    void playbackStopped(const QString &itemId, qint64 positionTicks);
    void nightModeEnabledChanged();
    void toneMappingVisualizationEnabledChanged();
    void audioDelayMsChanged();
    void audioOutputModeChanged();
    void volumeChanged();

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
        ToneMappingVisualization,
        AudioDelay,
    };

    bool ensureMpv();
    void scheduleMpvTeardown();
    void handleMpvEvent(mpv_event *event);
    void startProgressReporting();
    void stopProgressReporting(bool failed = false);
    bool mpvCommand(const char *command);
    bool beginSeekCommand(double targetSeconds, const QByteArray &flags,
                          bool markSeeking = true);
    bool beginRelativeSeekCommand(double deltaSeconds);
    double seekAnchorPosition();
    double projectedPositionSeconds() const;
    void requestMpvPositionRefresh(const char *reason);
    void restoreTrustedPosition(const char *reason);
    QByteArray buildSeekCommand(double targetSeconds, const QByteArray &flags) const;
    void updatePlaybackStatusText();
    void setPositionSeconds(double seconds, PlaybackPositionTracker::Source source);
    double clampedPosition(double seconds) const;
    void resetPlaybackUiState();
    bool applyMpvRuntimeOption(MpvRuntimeOption option, MpvOptionApplyMode mode, mpv_handle *handle);
    bool applyMpvSubtitleOptions(MpvOptionApplyMode mode, mpv_handle *handle);
    bool applyMpvRuntimeOptions(MpvOptionApplyMode mode, mpv_handle *handle);
    void handleVideoRenderError(const QString &message);

    NativeAppWindow *m_window = nullptr;
    JellyfinApiFacade *m_api = nullptr;
    PlaybackSession m_session;
    PlaybackReporter m_reporter;
    MpvLifecycle m_mpvLifecycle;
    QTimer m_progressTimer;
    QTimer m_backGuardTimer;
    QTimer m_uiPositionTimer;
    QTimer m_seekWatchdogTimer;
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
    QVariantList m_chapters;  // [{ title: QString, start: double seconds }]
    int m_currentChapter = -1;
    bool m_backAllowed = true;
    QString m_title;
    QString m_statusText = QStringLiteral("Ready");
    QString m_errorText;
    std::atomic_bool m_nightModeEnabled = false;
    std::atomic_bool m_toneMappingVisualizationEnabled = false;
    std::atomic<int> m_audioDelayMs = 0;
    std::atomic<int> m_volume = 100;
    QString m_audioOutputMode = QStringLiteral("alsa");
    SubtitlePreferences m_subtitlePreferences;
    PlaybackPositionTracker m_positionTracker;
    PlaybackTimeline m_timeline;
};

} // namespace JellyfinNative
