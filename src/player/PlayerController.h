#pragma once

#include "../common/JellyfinTypes.h"
#include "../platform/MpvConfigPolicy.h"
#include "MpvLifecycle.h"
#include "PlaybackPositionTracker.h"
#include "PlaybackReporter.h"
#include "PlaybackTimeline.h"
#include "PlaybackTrackState.h"

#include <QByteArray>
#include <QByteArrayList>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariant>

#include <atomic>
#include <vector>

struct mpv_handle;

namespace JellyfinNative {

class JellyfinApiFacade;
class NativeAppWindow;

class PlayerController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
    Q_PROPERTY(bool sessionActive READ sessionActive NOTIFY sessionActiveChanged)
    Q_PROPERTY(bool fileLoaded READ fileLoaded NOTIFY playbackStateChanged)
    Q_PROPERTY(QString mediaKind READ mediaKind NOTIFY playbackStateChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY playbackStateChanged)
    Q_PROPERTY(QString title READ title NOTIFY playbackStateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY playbackStateChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY playbackStateChanged)
    Q_PROPERTY(bool buffering READ buffering NOTIFY playbackStateChanged)
    Q_PROPERTY(int bufferingPercent READ bufferingPercent NOTIFY playbackStateChanged)
    Q_PROPERTY(bool seeking READ seeking NOTIFY playbackStateChanged)
    Q_PROPERTY(bool debugOsdVisible READ debugOsdVisible NOTIFY playbackStateChanged)
    Q_PROPERTY(bool subtitlesEnabled READ subtitlesEnabled NOTIFY tracksChanged)
    Q_PROPERTY(QStringList subtitleTracks READ subtitleTracks NOTIFY tracksChanged)
    Q_PROPERTY(int selectedSubtitleIndex READ selectedSubtitleIndex NOTIFY tracksChanged)
    Q_PROPERTY(QStringList audioTracks READ audioTracks NOTIFY tracksChanged)
    Q_PROPERTY(int selectedAudioIndex READ selectedAudioIndex NOTIFY tracksChanged)
    Q_PROPERTY(bool backAllowed READ backAllowed NOTIFY playbackStateChanged)
    Q_PROPERTY(double positionSeconds READ positionSeconds NOTIFY positionChanged)
    Q_PROPERTY(double durationSeconds READ durationSeconds NOTIFY positionChanged)
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY chaptersChanged)
    Q_PROPERTY(bool hasChapters READ hasChapters NOTIFY chaptersChanged)
    Q_PROPERTY(int currentChapter READ currentChapter NOTIFY chaptersChanged)
    Q_PROPERTY(bool nightModeEnabled READ nightModeEnabled WRITE setNightModeEnabled NOTIFY nightModeEnabledChanged)
    Q_PROPERTY(bool toneMappingVisualizationEnabled READ toneMappingVisualizationEnabled WRITE
            setToneMappingVisualizationEnabled NOTIFY toneMappingVisualizationEnabledChanged)
    Q_PROPERTY(int audioDelayMs READ audioDelayMs WRITE setAudioDelayMs NOTIFY audioDelayMsChanged)
    Q_PROPERTY(int fileAudioDelayMs READ fileAudioDelayMs WRITE setFileAudioDelayMs NOTIFY fileAudioDelayMsChanged)
    Q_PROPERTY(int effectiveAudioDelayMs READ effectiveAudioDelayMs NOTIFY effectiveAudioDelayMsChanged)
    Q_PROPERTY(int subtitleDelayMs READ subtitleDelayMs WRITE setSubtitleDelayMs NOTIFY subtitleDelayMsChanged)
    Q_PROPERTY(QString audioOutputMode READ audioOutputMode WRITE setAudioOutputMode NOTIFY audioOutputModeChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(double playbackSpeed READ playbackSpeed WRITE setPlaybackSpeed NOTIFY playbackSpeedChanged)
    Q_PROPERTY(double effectivePlaybackSpeed READ effectivePlaybackSpeed NOTIFY effectivePlaybackSpeedChanged)
    Q_PROPERTY(QString activeSegmentType READ activeSegmentType NOTIFY segmentsChanged)
    Q_PROPERTY(double activeSegmentEndSeconds READ activeSegmentEndSeconds NOTIFY segmentsChanged)
    Q_PROPERTY(bool trickplayAvailable READ trickplayAvailable NOTIFY trickplayChanged)
    Q_PROPERTY(QStringList trickplaySheetUrls READ trickplaySheetUrls NOTIFY trickplayChanged)

public:
    PlayerController(NativeAppWindow *window, JellyfinApiFacade *api, QObject *parent = nullptr);
    ~PlayerController() override;

    bool visible() const;
    bool sessionActive() const;
    bool fileLoaded() const;
    QString mediaKind() const;
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
    int fileAudioDelayMs() const;
    int effectiveAudioDelayMs() const;
    int subtitleDelayMs() const;
    QString audioOutputMode() const;
    int volume() const;
    double playbackSpeed() const;
    double effectivePlaybackSpeed() const;
    QString activeSegmentType() const;
    double activeSegmentEndSeconds() const;
    bool trickplayAvailable() const;
    QStringList trickplaySheetUrls() const;
    Q_INVOKABLE void skipActiveSegment();
    Q_INVOKABLE QVariantMap trickplayForSeconds(double seconds) const;

    Q_INVOKABLE void play(const JellyfinNative::PlaybackSession& session, bool startPaused = false);
    void setMediaSegments(const QString& itemId, const std::vector<MediaSegment>& segments);
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void seekBack();
    Q_INVOKABLE void seekForward();
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void previewSeekBy(double deltaSeconds);
    void prepareForBackground();
    void pauseForBackground();
    void resyncForForeground();
    void setKeepPlayingInBackground(bool keepPlaying);
    Q_INVOKABLE void toggleDebugOsd();
    Q_INVOKABLE void toggleSubtitles();
    Q_INVOKABLE void cycleSubtitles();
    Q_INVOKABLE void enableSubtitles();
    Q_INVOKABLE void selectSubtitle(int index);
    Q_INVOKABLE void selectSubtitleStreamIndex(int streamIndex);
    Q_INVOKABLE void cycleAudio();
    Q_INVOKABLE void selectAudio(int index);
    Q_INVOKABLE void selectAudioStreamIndex(int streamIndex);
    Q_INVOKABLE void nextChapter();
    Q_INVOKABLE void previousChapter();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void stopWithReason(const QString& reason);
    Q_INVOKABLE void setNightModeEnabled(bool enabled);
    Q_INVOKABLE void setToneMappingVisualizationEnabled(bool enabled);
    Q_INVOKABLE void setAudioDelayMs(int delayMs);
    Q_INVOKABLE void setFileAudioDelayMs(int delayMs);
    Q_INVOKABLE void setSubtitleDelayMs(int delayMs);
    Q_INVOKABLE void setAudioOutputMode(const QString& mode);
    Q_INVOKABLE void setVolume(int volume);
    Q_INVOKABLE void adjustVolume(int delta);
    Q_INVOKABLE void setPlaybackSpeed(double speed);
    void setSyncPlaybackSpeed(double speed);
    void clearSyncPlaybackSpeed();
    void setSubtitlePreferences(const JellyfinNative::SubtitlePreferences& preferences);
    void setDemuxerBudget(const QByteArray& maxBytes, const QByteArray& maxBackBytes);
    void setForwardCacheSizeMiB(int sizeMiB);
    void setMpvConfigPolicy(const MpvConfigPolicy& policy);

signals:
    void visibleChanged();
    void sessionActiveChanged();
    void positionChanged();
    void playbackStateChanged();
    void tracksChanged();
    void segmentsChanged();
    void trickplayChanged();
    void chaptersChanged();
    void playbackStopped(const QString& itemId, qint64 positionTicks, bool completed);
    void playbackLoadFailed(const QString& itemId, qint64 positionTicks, const QString& message);
    void nightModeEnabledChanged();
    void toneMappingVisualizationEnabledChanged();
    void audioDelayMsChanged();
    void fileAudioDelayMsChanged();
    void effectiveAudioDelayMsChanged();
    void subtitleDelayMsChanged();
    void audioOutputModeChanged();
    void volumeChanged();
    void playbackSpeedChanged();
    void effectivePlaybackSpeedChanged();

public:
    // Silence active playback before application services and the render
    // surface begin shutting down. Safe to call repeatedly.
    void prepareForShutdown();

    // Called from main on aboutToQuit so we tear down before the scene graph
    // stops accepting render jobs. Safe to call repeatedly. Pass async only
    // from the deferred post-stop path where blocking the GUI thread matters
    // more than deterministic completion.
    void teardownMpv(bool async = false);

private:
    int uiTrackIndexForStream(const QString& type, int streamIndex, int firstUiIndex) const;
    int streamIndexForUiTrack(const QString& type, int uiIndex, int firstUiIndex) const;
    void updateReportedStreamSelection(bool sendProgress);
    enum class MpvOptionApplyMode {
        Initial,
        Runtime,
    };

    enum class MpvRuntimeOption {
        NightMode,
        ToneMappingVisualization,
        AudioDelay,
        SubtitleDelay,
        PlaybackSpeed,
    };

    bool ensureMpv(bool needsVideoSurface);
    void scheduleIdleMpvPreparation();
    void prepareIdleMpv();
    void destroyIdleMpv(const char *reason);
    mpv_handle *takeIdleMpvHandle();
    bool configureAndInitializeMpv(mpv_handle *handle);
    void observeMpvProperties(mpv_handle *handle);
    void scheduleMpvTeardown();
    static QString mediaKindForSession(const PlaybackSession& session);
    void handleMpvEvent(mpv_event *event);
    void startProgressReporting();
    void stopProgressReporting(bool failed = false, bool completed = false);
    bool mpvCommand(QByteArrayList command);
    bool beginSeekCommand(double targetSeconds, const QByteArray& flags, bool markSeeking = true);
    QByteArrayList buildSeekCommand(double targetSeconds, const QByteArray& flags) const;
    bool beginRelativeSeekCommand(double deltaSeconds);
    void updatePlaybackStatusText();
    void notifyPlaybackStateChanged();
    void setPositionSeconds(double seconds, PlaybackPositionTracker::Source source, bool notifySegments = true);
    void requestMpvPositionRefresh(const char *reason);
    void restoreTrustedPosition(const char *reason);
    double clampedPosition(double seconds) const;
    double seekAnchorPosition();
    double projectedPositionSeconds() const;
    void resetPlaybackUiState();
    void rebuildTrickplaySheetUrls();
    bool applyMpvRuntimeOption(MpvRuntimeOption option, MpvOptionApplyMode mode, mpv_handle *handle);
    bool applyMpvSubtitleOptions(MpvOptionApplyMode mode, mpv_handle *handle, bool preserveTrackSelection = false);
    bool applyMpvRuntimeOptions(MpvOptionApplyMode mode, mpv_handle *handle);
    void discardPreparedMpvForOptionChange(const char *reason);
    void handleVideoRenderError(const QString& message);
    void changePlaybackSpeed(double speed, bool syncOverride, bool clearSyncOverride = false);

    NativeAppWindow *m_window = nullptr;
    JellyfinApiFacade *m_api = nullptr;
    PlaybackSession m_session;
    PlaybackReporter m_reporter;
    MpvLifecycle m_mpvLifecycle;
    mpv_handle *m_idleMpvHandle = nullptr;
    bool m_idleMpvPreparationScheduled = false;
    bool m_idleMpvPreparationEnabled = true;
    QTimer m_progressTimer;
    QTimer m_backGuardTimer;
    QTimer m_uiPositionTimer;
    QTimer m_seekWatchdogTimer;
    QTimer m_backgroundPauseTimer;
    bool m_visible = false;
    bool m_sessionActive = false;
    bool m_fileLoaded = false;
    bool m_keepPlayingInBackground = false;
    bool m_paused = false;
    bool m_buffering = false;
    int m_bufferingPercent = 0;
    bool m_seeking = false;
    bool m_debugOsdVisible = false;
    PlaybackTrackState m_tracks;
    bool m_backAllowed = true;
    QString m_title;
    QString m_statusText = QStringLiteral("Ready");
    QString m_errorText;
    QString m_mediaKind = QStringLiteral("none");
    std::atomic_bool m_nightModeEnabled = false;
    std::atomic_bool m_toneMappingVisualizationEnabled = false;
    std::atomic<int> m_audioDelayMs = 0;
    std::atomic<int> m_fileAudioDelayMs = 0;
    std::atomic<int> m_subtitleDelayMs = 0;
    std::atomic<int> m_volume = 100;
    double m_playbackSpeed = 1.0;
    double m_syncPlaybackSpeed = 1.0;
    bool m_syncPlaybackSpeedActive = false;
    QString m_audioOutputMode = QStringLiteral("auto");
    QByteArray m_automaticDemuxerMaxBytes = QByteArrayLiteral("64M");
    QByteArray m_demuxerMaxBytes = QByteArrayLiteral("64M");
    QByteArray m_demuxerMaxBackBytes = QByteArrayLiteral("32M");
    MpvConfigPolicy m_mpvConfigPolicy;
    int m_forwardCacheSizeMiB = 0;
    SubtitlePreferences m_subtitlePreferences;
    bool m_hdrPlayback = false;
    PlaybackPositionTracker m_positionTracker;
    PlaybackTimeline m_timeline;
    QStringList m_trickplaySheetUrls;
};

} // namespace JellyfinNative
