#include "PlayerController.h"

#include "../api/JellyfinApiFacade.h"
#include "../common/JellyfinTypes.h"
#include "../common/LogRotation.h"
#include "../common/TlsTrust.h"
#include "../diagnostics/Diagnostics.h"
#include "../platform/MpvConfigPolicy.h"
#include "../platform/NativeAppWindow.h"
#include "../platform/PlatformPlaybackSurface.h"
#include "../platform/PlatformSystemProbes.h"
#include "MpvOptionProfile.h"
#include "PlaybackFailurePolicy.h"
#include "PlaybackTrackParser.h"

extern "C" {
#include <mpv/client.h>
}

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QMetaObject>
#include <QPointer>
#include <QStandardPaths>
#include <QUrl>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>

namespace JellyfinNative {

namespace {

    constexpr auto kMpvLogFileName = "com.sachk.spool-mpv.log";

    constexpr uint64_t kTimePosRefreshReply = 0x6a666e7074730001ULL;
    constexpr auto kNightModeFilter
        = "lavfi=[pan=stereo|FL<0.5*FL+1.0*FC+0.25*BL|FR<0.5*FR+1.0*FC+0.25*BR,"
          "dialoguenhance=original=0.25:enhance=2.0,"
          "pan=stereo|FL=FL+0.6*FC|FR=FR+0.6*FC,"
          "compand=attacks=0.02:decays=0.5:points=-80/-80|-45/-35|-30/-25|-20/-18|0/-10:gain=2,"
          "highpass=f=50:p=2:t=q:w=0.7071,"
          "equalizer=f=60:t=q:w=1.4:g=-4,"
          "equalizer=f=98:t=q:w=6.0:g=-9,"
          "equalizer=f=131:t=q:w=2.5:g=-11,"
          "equalizer=f=850:t=q:w=3.0:g=-1,"
          "equalizer=f=2000:t=q:w=2.0:g=5,"
          "equalizer=f=3200:t=q:w=2.5:g=4.5,"
          "equalizer=f=4200:t=q:w=2.0:g=3.5,"
          "treble=f=7500:t=q:w=0.6667:g=3,"
          "speechnorm=e=12.5:r=0.0001:l=1,"
          "alimiter=limit=0.95:attack=3:release=50]";

    const char *endFileReasonName(int reason)
    {
        switch (reason) {
        case MPV_END_FILE_REASON_EOF:
            return "eof";
        case MPV_END_FILE_REASON_STOP:
            return "stop";
        case MPV_END_FILE_REASON_QUIT:
            return "quit";
        case MPV_END_FILE_REASON_ERROR:
            return "error";
        case MPV_END_FILE_REASON_REDIRECT:
            return "redirect";
        default:
            return "unknown";
        }
    }

    QByteArray mpvLogPath()
    {
        const QByteArray logDir = qgetenv("JELLYFIN_NATIVE_LOG_DIR");
        if (logDir.isEmpty()) {
            const QString fallback = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
            return QFile::encodeName(QDir(fallback).filePath(QString::fromLatin1(kMpvLogFileName)));
        }

        QByteArray path = logDir;
        if (!path.endsWith('/'))
            path += '/';
        path += QByteArray(kMpvLogFileName);
        return path;
    }

    QByteArray mpvShaderCachePath()
    {
        const QString cacheDirectory = QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
                                           .filePath(QStringLiteral("mpv-shaders"));
        return QFile::encodeName(cacheDirectory);
    }

    QByteArray bundledSubtitleFontsPath()
    {
        const QString fontsPath
            = QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)).filePath(QStringLiteral("fonts"));
        if (!QDir().mkpath(fontsPath))
            return {};

        const QStringList fontFiles {
            QStringLiteral("AtkinsonHyperlegible-Bold.otf"),
            QStringLiteral("AtkinsonHyperlegible-Regular.otf"),
            QStringLiteral("IBMPlexSans-Variable.ttf"),
        };
        for (const QString& fileName : fontFiles) {
            const QString target = QDir(fontsPath).filePath(fileName);
            if (QFile::exists(target))
                continue;
            const QString source = QStringLiteral(":/qt/qml/JellyfinWebOS/qml/fonts/") + fileName;
            if (!QFile::copy(source, target)) {
                qWarning() << "player: failed to extract bundled subtitle font" << fileName;
                return {};
            }
        }
        return QFile::encodeName(fontsPath);
    }

    bool setOption(mpv_handle *handle, const char *name, const char *value)
    {
        const int error = mpv_set_option_string(handle, name, value);
        if (error >= 0 || error == MPV_ERROR_OPTION_NOT_FOUND)
            return true;
        qWarning() << "player: failed to set mpv option" << name << "=" << value << mpv_error_string(error);
        return false;
    }

    bool applyOptions(mpv_handle *handle, const std::vector<MpvOption>& options)
    {
        bool ok = true;
        for (const MpvOption& option : options)
            ok &= setOption(handle, option.name.constData(), option.value.constData());
        return ok;
    }
    bool applyProperties(mpv_handle *handle, const std::vector<MpvOption>& options)
    {
        bool ok = true;
        for (const MpvOption& option : options) {
            const int error = mpv_set_property_string(handle, option.name.constData(), option.value.constData());
            if (error >= 0 || error == MPV_ERROR_OPTION_NOT_FOUND)
                continue;
            qWarning() << "player: failed to set mpv property" << option.name << "=" << option.value
                       << mpv_error_string(error);
            ok = false;
        }
        return ok;
    }

    bool setMpvProperty(mpv_handle *handle, const char *name, const char *value)
    {
        const int error = mpv_set_property_string(handle, name, value);
        return error >= 0 || error == MPV_ERROR_OPTION_NOT_FOUND;
    }

    bool setRequiredMpvProperty(mpv_handle *handle, const char *name, const char *value)
    {
        return mpv_set_property_string(handle, name, value) >= 0;
    }

    bool setMpvDoubleProperty(mpv_handle *handle, const char *name, double value, double *appliedValue = nullptr)
    {
        const int error = mpv_set_property(handle, name, MPV_FORMAT_DOUBLE, &value);
        if (error < 0 && error != MPV_ERROR_OPTION_NOT_FOUND)
            return false;

        if (appliedValue)
            *appliedValue = value;

        double readback = 0.0;
        const int readError = mpv_get_property(handle, name, MPV_FORMAT_DOUBLE, &readback);
        if (readError >= 0 && appliedValue)
            *appliedValue = readback;

        return true;
    }

    QByteArray mpvBool(bool value)
    {
        return value ? QByteArrayLiteral("yes") : QByteArrayLiteral("no");
    }

    qint64 secondsToTicks(double seconds)
    {
        return static_cast<qint64>(seconds * 10000000.0);
    }

    void logMemoryStats()
    {
        const QString diagnostics = platformProcessMemoryDiagnostics();
        if (!diagnostics.isEmpty())
            qInfo().noquote() << "player: memstats" << diagnostics;
    }

} // namespace

PlayerController::PlayerController(
    NativeAppWindow *window, JellyfinApiFacade *api, TlsTrustController *tlsTrust, QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_api(api)
    , m_reporter(api, this)
    , m_tlsTrust(tlsTrust)
{
    if (m_api) {
        connect(m_api, &JellyfinApiFacade::sessionTokenChanged, this, [this]() {
            if (auto *handle = m_mpvLifecycle.handle())
                setMpvProperty(handle, "http-header-fields", "");
        });
    }
    m_idleMpvPreparationEnabled = platformIdleMpvPreparationEnabled();
    m_progressTimer.setInterval(5000);
    m_uiPositionTimer.setInterval(250);
    m_backGuardTimer.setSingleShot(true);
    m_backGuardTimer.setInterval(1500);
    m_seekWatchdogTimer.setSingleShot(true);
    m_seekWatchdogTimer.setInterval(2500);
    m_backgroundTeardownTimer.setSingleShot(true);
    m_backgroundTeardownTimer.setInterval(750);
    connect(&m_backgroundTeardownTimer, &QTimer::timeout, this, [this]() {
        if (!platformUsesBackgroundPlaybackPolicy() || !m_sessionActive)
            return;
        qInfo() << "player: stopping after sustained background/hidden app state";
        stopWithReason(QStringLiteral("background"));
        teardownMpv();
    });
    connect(&m_backGuardTimer, &QTimer::timeout, this, [this]() {
        if (!m_sessionActive || m_backAllowed)
            return;
        m_backAllowed = true;
        qInfo() << "player: startup back guard released";
        emit playbackStateChanged();
    });
    connect(&m_uiPositionTimer, &QTimer::timeout, this, [this]() {
        if (m_sessionActive)
            requestMpvPositionRefresh("ui timer");
    });
    connect(&m_seekWatchdogTimer, &QTimer::timeout, this, [this]() {
        if (!m_sessionActive || !m_seeking)
            return;

        qInfo() << "player: clearing stale seek state";
        m_seeking = false;
        m_positionTracker.cancelSeek();
        notifyPlaybackStateChanged();
    });
    connect(&m_progressTimer, &QTimer::timeout, this, [this]() {
        if (!m_sessionActive)
            return;

        logMemoryStats();

        m_reporter.reportProgress(secondsToTicks(m_positionTracker.position()), m_paused, effectivePlaybackSpeed());
    });
    connect(&m_reporter, &PlaybackReporter::reportFailed, this, [](const QString& operation, const QString& message) {
        Diagnostics::logEvent(QStringLiteral("player"), QStringLiteral("report_failed"),
            { { QStringLiteral("operation"), operation }, { QStringLiteral("message"), message } });
    });
    if (m_api) {
        connect(m_api, &JellyfinApiFacade::playbackNetworkProfileChanged, this,
            [this]() { discardPreparedMpvForOptionChange("network profile change"); });
    }
    scheduleIdleMpvPreparation();
}

PlayerController::~PlayerController()
{
    teardownMpv();
}

void PlayerController::prepareForShutdown()
{
    m_idleMpvPreparationEnabled = false;
    m_idleMpvPreparationScheduled = false;
    if (!m_mpvLifecycle.handle())
        return;

    // Silence first: reporting shutdown may cancel network work, but it must
    // never keep audible playback alive while the application is closing.
    mpvCommand(
        { QByteArrayLiteral("no-osd"), QByteArrayLiteral("set"), QByteArrayLiteral("mute"), QByteArrayLiteral("yes") });
    if (m_sessionActive)
        stopWithReason(QStringLiteral("app-shutdown"));
    else
        mpvCommand({ QByteArrayLiteral("stop") });
}

void PlayerController::teardownMpv(bool async)
{
    ++m_mpvTeardownGeneration;
    Diagnostics::Phase phase(QStringLiteral("shutdown"), QStringLiteral("player_teardown_mpv"));
    m_idleMpvPreparationEnabled = false;
    m_idleMpvPreparationScheduled = false;
    destroyIdleMpv("teardown");
    // Platform render resources must detach before the mpv core is destroyed.
    if (!releasePlatformMpvSurface(m_embeddedVideoOutput)) {
        qCritical() << "player: timed out releasing the mpv render context; preserving the mpv core";
        return;
    }
    // The deferred post-stop teardown must not stall the GUI thread; shutdown
    // and the stale-core path before a new play() stay synchronous so the new
    // pipeline never races the old core for media resources.
    if (async)
        m_mpvLifecycle.destroyAsync();
    else
        m_mpvLifecycle.destroy();
    m_embeddedVideoOutput = false;
}

void PlayerController::scheduleIdleMpvPreparation()
{
    if (!m_idleMpvPreparationEnabled || m_idleMpvPreparationScheduled || m_idleMpvHandle || m_mpvLifecycle.handle())
        return;

    m_idleMpvPreparationScheduled = true;
    QPointer<PlayerController> controller(this);
    runAfterPlatformMpvLoaded([controller]() {
        if (auto *app = QCoreApplication::instance()) {
            // Delay past the launch window: idle preparation saves little at
            // play-start and must not compete with first-page construction.
            QTimer::singleShot(3000, app, [controller]() {
                if (controller)
                    controller->prepareIdleMpv();
            });
        }
    });
}

void PlayerController::prepareIdleMpv()
{
    m_idleMpvPreparationScheduled = false;
    if (!m_idleMpvPreparationEnabled || m_idleMpvHandle || m_mpvLifecycle.handle() || m_sessionActive)
        return;

    QElapsedTimer startupTimer;
    startupTimer.start();
    const QByteArray logPath = mpvLogPath();
    rotateLogFile(logPath.constData());
    mpv_handle *handle = mpv_create();
    if (!handle) {
        qWarning() << "player: idle mpv_create failed";
        return;
    }

    if (!configureAndInitializeMpv(handle, false)) {
        mpv_terminate_destroy(handle);
        qWarning() << "player: idle mpv initialization failed";
        return;
    }

    m_idleMpvHandle = handle;
    qInfo() << "player: idle-prepared mpv initialized in" << startupTimer.elapsed() << "ms";
}

void PlayerController::destroyIdleMpv(const char *reason)
{
    if (!m_idleMpvHandle)
        return;

    mpv_handle *handle = m_idleMpvHandle;
    m_idleMpvHandle = nullptr;
    qInfo() << "player: destroying idle-prepared mpv" << (reason ? reason : "unspecified");
    mpv_terminate_destroy(handle);
}

mpv_handle *PlayerController::takeIdleMpvHandle()
{
    mpv_handle *handle = m_idleMpvHandle;
    m_idleMpvHandle = nullptr;
    if (handle)
        qInfo() << "player: adopting idle-prepared mpv handle";
    return handle;
}

bool PlayerController::configureAndInitializeMpv(mpv_handle *handle, bool embeddedVideo)
{
    if (!handle)
        return false;

    const auto platform = platformMpvOptionProfile();
    const int parallelRequests = m_api ? m_api->playbackParallelRequests() : 1;
    const MpvOptionProfile::NetworkProfile network = MpvOptionProfile::networkProfile(platform, parallelRequests);
    qInfo().nospace() << "player: curl profile source=MpvOptionProfile platform="
                      << platformPlaybackBackendName(embeddedVideo) << " requestsPerStream=" << network.parallelRequests
                      << " rangeBytes=" << network.rangeBytes << " ringBytes=" << network.ringBytes;
    if (!applyOptions(handle, MpvOptionProfile::preInitializeOptions(m_mpvConfigPolicy)))
        return false;
    if (mpv_initialize(handle) < 0)
        return false;

    auto applicationOptions = MpvOptionProfile::applicationOptions(platform, m_audioOutputMode, mpvLogPath(),
        m_demuxerMaxBytes, m_demuxerMaxBackBytes, parallelRequests, embeddedVideo, mpvShaderCachePath());
    const QByteArray subtitleFontsPath = bundledSubtitleFontsPath();
    if (!subtitleFontsPath.isEmpty())
        applicationOptions.push_back({ "sub-fonts-dir", subtitleFontsPath });

    return applyProperties(handle, applicationOptions) && applyMpvRuntimeOptions(MpvOptionApplyMode::Runtime, handle);
}

void PlayerController::observeMpvProperties(mpv_handle *handle)
{
    mpv_observe_property(handle, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(handle, 0, "paused-for-cache", MPV_FORMAT_FLAG);
    mpv_observe_property(handle, 0, "cache-buffering-state", MPV_FORMAT_INT64);
    mpv_observe_property(handle, 0, "seeking", MPV_FORMAT_FLAG);
    mpv_observe_property(handle, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle, 0, "volume", MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle, 0, "track-list", MPV_FORMAT_NODE);
    mpv_observe_property(handle, 0, "chapter-list", MPV_FORMAT_NODE);
    mpv_observe_property(handle, 0, "chapter", MPV_FORMAT_INT64);
    mpv_observe_property(handle, 0, "video-params/transfer", MPV_FORMAT_STRING);
    mpv_observe_property(handle, 0, "hwdec-current", MPV_FORMAT_STRING);
    mpv_observe_property(handle, 0, "decoder-frame-drop-count", MPV_FORMAT_INT64);
    mpv_observe_property(handle, 0, "frame-drop-count", MPV_FORMAT_INT64);
}

void PlayerController::scheduleMpvTeardown()
{
    auto *scheduledHandle = m_mpvLifecycle.handle();
    if (!scheduledHandle)
        return;

    const quint64 scheduledGeneration = ++m_mpvTeardownGeneration;
    QTimer::singleShot(1000, this, [this, scheduledHandle, scheduledGeneration]() {
        if (m_mpvTeardownGeneration != scheduledGeneration || m_mpvLifecycle.handle() != scheduledHandle)
            return;
        qInfo() << "player: deferred mpv teardown";
        teardownMpv(true);
    });
}

bool PlayerController::visible() const
{
    return m_visible;
}

bool PlayerController::sessionActive() const
{
    return m_sessionActive;
}

bool PlayerController::fileLoaded() const
{
    return m_fileLoaded;
}
bool PlayerController::hdrPlayback() const
{
    return m_hdrPlayback;
}

QString PlayerController::mediaKind() const
{
    return m_mediaKind;
}

QString PlayerController::mediaKindForSession(const PlaybackSession& session)
{
    bool hasAudio = false;
    for (const MediaStreamInfo& stream : session.mediaStreams) {
        if (stream.type.compare(QStringLiteral("Video"), Qt::CaseInsensitive) == 0)
            return QStringLiteral("video");
        if (stream.type.compare(QStringLiteral("Audio"), Qt::CaseInsensitive) == 0)
            hasAudio = true;
    }
    if (hasAudio || session.itemType.compare(QStringLiteral("Audio"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("audio");
    return QStringLiteral("video");
}

bool PlayerController::paused() const
{
    return m_paused;
}

QString PlayerController::title() const
{
    return m_title;
}

QString PlayerController::statusText() const
{
    return m_statusText;
}

QString PlayerController::errorText() const
{
    return m_errorText;
}

bool PlayerController::buffering() const
{
    return m_buffering;
}

int PlayerController::bufferingPercent() const
{
    return m_bufferingPercent;
}

bool PlayerController::seeking() const
{
    return m_seeking;
}

bool PlayerController::debugOsdVisible() const
{
    return m_debugOsdVisible;
}

bool PlayerController::embeddedVideoOutput() const
{
    return m_embeddedVideoOutput;
}

qint64 PlayerController::decoderDroppedFrames() const
{
    return m_decoderDroppedFrames;
}

qint64 PlayerController::outputDroppedFrames() const
{
    return m_outputDroppedFrames;
}

bool PlayerController::subtitlesEnabled() const
{
    return m_tracks.subtitlesEnabled();
}

QStringList PlayerController::subtitleTracks() const
{
    return m_tracks.subtitleTracks();
}

int PlayerController::selectedSubtitleIndex() const
{
    return m_tracks.selectedSubtitleIndex();
}

QStringList PlayerController::audioTracks() const
{
    return m_tracks.audioTracks();
}

int PlayerController::selectedAudioIndex() const
{
    return m_tracks.selectedAudioIndex();
}

bool PlayerController::backAllowed() const
{
    return m_backAllowed;
}

double PlayerController::positionSeconds() const
{
    return m_positionTracker.position();
}

double PlayerController::durationSeconds() const
{
    return m_positionTracker.duration();
}

QVariantList PlayerController::chapters() const
{
    return m_tracks.chapters();
}

bool PlayerController::hasChapters() const
{
    return m_tracks.hasChapters();
}

int PlayerController::currentChapter() const
{
    return m_tracks.currentChapter();
}

bool PlayerController::nightModeEnabled() const
{
    return m_nightModeEnabled.load();
}

bool PlayerController::toneMappingVisualizationEnabled() const
{
    return m_toneMappingVisualizationEnabled.load();
}

int PlayerController::audioDelayMs() const
{
    return m_audioDelayMs.load();
}

int PlayerController::fileAudioDelayMs() const
{
    return m_fileAudioDelayMs.load();
}

int PlayerController::effectiveAudioDelayMs() const
{
    return qBound(-4000, m_audioDelayMs.load() + m_fileAudioDelayMs.load(), 4000);
}

int PlayerController::subtitleDelayMs() const
{
    return m_subtitleDelayMs.load();
}

QString PlayerController::audioOutputMode() const
{
    return m_audioOutputMode;
}

int PlayerController::volume() const
{
    return m_volume.load();
}

double PlayerController::playbackSpeed() const
{
    return m_playbackSpeed;
}

double PlayerController::effectivePlaybackSpeed() const
{
    return m_syncPlaybackSpeedActive ? m_syncPlaybackSpeed : m_playbackSpeed;
}

bool PlayerController::applyMpvRuntimeOption(MpvRuntimeOption option, MpvOptionApplyMode mode, mpv_handle *handle)
{
    if (!handle)
        return false;

    const char *name = nullptr;
    QByteArray value;
    double doubleValue = 0.0;
    switch (option) {
    case MpvRuntimeOption::NightMode:
        name = "af";
        value = m_nightModeEnabled.load() ? QByteArray(kNightModeFilter) : QByteArray();
        break;
    case MpvRuntimeOption::ToneMappingVisualization:
        name = "tone-mapping-visualize";
        value = mpvBool(m_toneMappingVisualizationEnabled.load());
        break;
    case MpvRuntimeOption::AudioDelay:
        name = "audio-delay";
        doubleValue = static_cast<double>(effectiveAudioDelayMs()) / 1000.0;
        value = QByteArray::number(doubleValue, 'f', 3);
        break;
    case MpvRuntimeOption::SubtitleDelay:
        name = "sub-delay";
        doubleValue = static_cast<double>(m_subtitleDelayMs.load()) / 1000.0;
        value = QByteArray::number(doubleValue, 'f', 3);
        break;
    case MpvRuntimeOption::PlaybackSpeed:
        name = "speed";
        doubleValue = effectivePlaybackSpeed();
        value = QByteArray::number(doubleValue, 'f', 3);
        break;
    }

    double appliedDoubleValue = doubleValue;
    const bool numericOption = option == MpvRuntimeOption::AudioDelay || option == MpvRuntimeOption::SubtitleDelay
        || option == MpvRuntimeOption::PlaybackSpeed;
    const bool ok = mode == MpvOptionApplyMode::Initial ? setOption(handle, name, value.constData())
        : numericOption ? setMpvDoubleProperty(handle, name, doubleValue, &appliedDoubleValue)
                        : setMpvProperty(handle, name, value.constData());
    if (!ok) {
        qWarning() << "player: failed to apply mpv runtime option" << name
                   << "mode=" << (mode == MpvOptionApplyMode::Initial ? "initial" : "runtime");
    } else if (numericOption) {
        qInfo() << "player: applied numeric playback option" << name
                << "mode=" << (mode == MpvOptionApplyMode::Initial ? "initial" : "runtime")
                << "requested=" << doubleValue << "applied=" << appliedDoubleValue;
    }
    return ok;
}

bool PlayerController::applyMpvRuntimeOptions(MpvOptionApplyMode mode, mpv_handle *handle)
{
    return applyMpvRuntimeOption(MpvRuntimeOption::NightMode, mode, handle)
        && applyMpvRuntimeOption(MpvRuntimeOption::ToneMappingVisualization, mode, handle)
        && applyMpvRuntimeOption(MpvRuntimeOption::AudioDelay, mode, handle)
        && applyMpvRuntimeOption(MpvRuntimeOption::SubtitleDelay, mode, handle)
        && applyMpvRuntimeOption(MpvRuntimeOption::PlaybackSpeed, mode, handle)
        && applyMpvSubtitleOptions(mode, handle);
}

void PlayerController::discardPreparedMpvForOptionChange(const char *reason)
{
    if (m_mpvLifecycle.handle())
        return;

    destroyIdleMpv(reason);
    scheduleIdleMpvPreparation();
}

bool PlayerController::applyMpvSubtitleOptions(MpvOptionApplyMode mode, mpv_handle *handle, bool preserveTrackSelection,
    const SubtitlePreferences *previousPreferences)
{
    if (!handle)
        return false;

    auto applyString = [mode, handle](const char *name, const QByteArray& value) {
        return mode == MpvOptionApplyMode::Initial ? setOption(handle, name, value.constData())
                                                   : setMpvProperty(handle, name, value.constData());
    };

    bool ok = true;
    const auto options
        = MpvOptionProfile::subtitleOptions(m_subtitlePreferences, m_tracks.subtitlesEnabled(), m_hdrPlayback);
    const auto previousOptions = previousPreferences
        ? MpvOptionProfile::subtitleOptions(*previousPreferences, m_tracks.subtitlesEnabled(), m_hdrPlayback)
        : std::vector<MpvOption> {};
    for (const MpvOption& option : options) {
        const auto previous = std::find_if(previousOptions.begin(), previousOptions.end(),
            [&option](const MpvOption& candidate) { return candidate.name == option.name; });
        if (previous != previousOptions.end() && previous->value == option.value)
            continue;
        const bool selectsTrack = option.name == QByteArrayLiteral("sid") || option.name == QByteArrayLiteral("slang")
            || option.name == QByteArrayLiteral("alang") || option.name == QByteArrayLiteral("sub-auto")
            || option.name == QByteArrayLiteral("sub-visibility")
            || option.name == QByteArrayLiteral("sub-forced-events-only")
            || option.name == QByteArrayLiteral("subs-with-matching-audio")
            || option.name == QByteArrayLiteral("subs-fallback")
            || option.name == QByteArrayLiteral("subs-fallback-forced");
        if (!preserveTrackSelection || !selectsTrack)
            ok &= applyString(option.name.constData(), option.value);
    }

    if (!ok) {
        qWarning() << "player: failed to apply subtitle preferences"
                   << "mode=" << (mode == MpvOptionApplyMode::Initial ? "initial" : "runtime");
    } else {
        qInfo() << "player: subtitle appearance applied"
                << "mode=" << (mode == MpvOptionApplyMode::Initial ? "initial" : "runtime")
                << "preserveTrack=" << preserveTrackSelection << "hdr=" << m_hdrPlayback
                << "dimInHdr=" << m_subtitlePreferences.dimInHdr
                << "brightnessPercent=" << m_subtitlePreferences.hdrBrightnessPercent;
    }
    return ok;
}

bool PlayerController::ensureMpv(bool needsVideoSurface, bool embeddedVideo)
{
    if (m_mpvLifecycle.handle())
        return true;

    QElapsedTimer startupTimer;
    startupTimer.start();

    if (embeddedVideo)
        destroyIdleMpv("embedded software video output");
    mpv_handle *handle = takeIdleMpvHandle();
    const bool idlePrepared = handle != nullptr;
    if (!handle) {
        const QByteArray logPath = mpvLogPath();
        rotateLogFile(logPath.constData());
        handle = mpv_create();
        if (!handle) {
            m_errorText = QStringLiteral("mpv_create failed.");
            emit playbackStateChanged();
            return false;
        }

        if (!configureAndInitializeMpv(handle, embeddedVideo)) {
            mpv_terminate_destroy(handle);
            m_errorText = QStringLiteral("Failed to initialize libmpv.");
            emit playbackStateChanged();
            return false;
        }
    }

    QString surfaceError;
    if (!configurePlatformMpvSurface(handle, *m_window, needsVideoSurface, embeddedVideo, surfaceError)) {
        mpv_terminate_destroy(handle);
        m_errorText = surfaceError;
        emit playbackStateChanged();
        return false;
    }

    observeMpvProperties(handle);

    qInfo() << "player: mpv initialized in" << startupTimer.elapsed() << "ms"
            << "idlePrepared=" << idlePrepared;

    QString attachmentError;
    if (!attachPlatformMpvSurface(
            handle, needsVideoSurface, embeddedVideo, *this,
            [this](const QString& message) { handleVideoRenderError(message); }, attachmentError)) {
        mpv_terminate_destroy(handle);
        m_errorText = attachmentError;
        m_statusText = QStringLiteral("Playback unavailable");
        emit playbackStateChanged();
        return false;
    }

    m_embeddedVideoOutput = needsVideoSurface && embeddedVideo;
    if (!m_mpvLifecycle.adopt(handle, [this](mpv_event *event) { handleMpvEvent(event); })) {
        if (needsVideoSurface)
            releasePlatformMpvSurface(m_embeddedVideoOutput);
        m_embeddedVideoOutput = false;
        mpv_terminate_destroy(handle);
        m_errorText = QStringLiteral("Failed to start the libmpv event loop.");
        emit playbackStateChanged();
        return false;
    }
    return true;
}

void PlayerController::handleVideoRenderError(const QString& message)
{
    m_errorText = message;
    m_statusText = QStringLiteral("Playback unavailable");
    emit playbackStateChanged();
}

void PlayerController::play(const PlaybackSession& session, bool startPaused)
{
    const QString nextMediaKind = mediaKindForSession(session);
    const bool needsVideoSurface = nextMediaKind == QStringLiteral("video");
    const bool embeddedVideo = needsVideoSurface && platformUsesEmbeddedVideo(session);
    Diagnostics::Task task(QStringLiteral("player_play"),
        { { QStringLiteral("itemId"), session.itemId }, { QStringLiteral("title"), session.title },
            { QStringLiteral("mediaKind"), nextMediaKind } });
    qInfo() << "player: play requested" << session.title << "method=" << session.playMethod
            << "mediaKind=" << nextMediaKind << "startTimeTicks=" << session.startTimeTicks
            << "startPaused=" << startPaused;

    if (m_mpvLifecycle.handle()) {
        qInfo() << "player: tearing down stale mpv before play";
        teardownMpv();
    }

    const bool hadFileAudioDelay = m_fileAudioDelayMs.exchange(0) != 0;
    const bool hadSubtitleDelay = m_subtitleDelayMs.exchange(0) != 0;
    if (hadFileAudioDelay) {
        emit fileAudioDelayMsChanged();
        emit effectiveAudioDelayMsChanged();
    }
    if (hadSubtitleDelay)
        emit subtitleDelayMsChanged();

    const bool hdrPlayback = MpvOptionProfile::isHdrPlayback(session.mediaStreams);
    if (m_hdrPlayback != hdrPlayback) {
        m_hdrPlayback = hdrPlayback;
        emit hdrPlaybackChanged();
    }
    qInfo() << "player: HDR subtitle mode" << (m_hdrPlayback ? "enabled" : "disabled") << "source=media-metadata";
    m_window->clearOverlay();
    if (needsVideoSurface && !embeddedVideo) {
        QElapsedTimer playbackSurfaceTimer;
        playbackSurfaceTimer.start();
        if (!m_window->prepareForPlaybackSurface()) {
            m_errorText = QStringLiteral("Failed to prepare the native playback surface.");
            qWarning() << "player: prepareForPlaybackSurface failed after" << playbackSurfaceTimer.elapsed() << "ms";
            emit playbackStateChanged();
            return;
        }
        qInfo() << "player: prepareForPlaybackSurface completed in" << playbackSurfaceTimer.elapsed() << "ms";
    } else if (embeddedVideo) {
        qInfo() << "player: using embedded OpenGL software video surface";
    } else {
        qInfo() << "player: audio-only playback does not request a video surface";
    }

    if (!ensureMpv(needsVideoSurface, embeddedVideo))
        return;

    m_session = session;
    m_timeline.setSession(session);
    rebuildTrickplaySheetUrls();
    m_title = session.title;
    m_mediaKind = nextMediaKind;
    m_statusText = platformPreparingStatus(needsVideoSurface, embeddedVideo);
    m_errorText.clear();
    const double startSeconds
        = session.startTimeTicks > 0 ? static_cast<double>(session.startTimeTicks) / 10000000.0 : 0.0;
    m_positionTracker.reset();
    m_paused = startPaused;
    m_fileLoaded = false;
    m_seekDispatchReady = false;
    m_buffering = false;
    m_bufferingPercent = 0;
    m_seeking = false;
    m_pendingSeek = false;
    m_pendingSeekFlags.clear();
    m_debugOsdVisible = false;
    m_tracks.resetForPlayback();
    m_restoreStreamSelection = session.restoreStreamSelection;
    m_backAllowed = false;
    m_backGuardTimer.start();
    m_uiPositionTimer.start();
    const bool wasVisible = m_visible;
    const bool wasSessionActive = m_sessionActive;
    m_visible = needsVideoSurface;
    m_sessionActive = true;
    if (wasVisible != m_visible)
        emit visibleChanged();
    if (wasSessionActive != m_sessionActive)
        emit sessionActiveChanged();
    emit positionChanged();
    emit playbackStateChanged();
    emit tracksChanged();
    emit segmentsChanged();
    emit trickplayChanged();

    QString surfaceReadyError;
    if (!waitForPlatformMpvSurfaceReady(needsVideoSurface, embeddedVideo, surfaceReadyError)) {
        m_errorText = surfaceReadyError;
        m_statusText = QStringLiteral("Playback unavailable");
        teardownMpv();
        emit playbackStateChanged();
        return;
    }
    auto *handle = m_mpvLifecycle.handle();
    m_mpvLifecycle.beginFileLoad();

    // SyncPlay queue preparation must never emit audio or advance the
    // timeline before the server's scheduled Unpause command. Set pause on
    // the idle mpv core before loadfile so even the first decoded frame is
    // held. Ordinary playback explicitly clears any inherited pause state.
    if (!setRequiredMpvProperty(handle, "pause", startPaused ? "yes" : "no")) {
        m_mpvLifecycle.cancelFileLoad();
        m_errorText = QStringLiteral("libmpv rejected the initial playback state.");
        stopProgressReporting(true);
        return;
    }

    QString subtitlePreloadError;
    if (!applyPlatformSubtitlePreload(handle, session, m_subtitlePreferences.language, subtitlePreloadError)) {
        m_mpvLifecycle.cancelFileLoad();
        m_errorText = subtitlePreloadError;
        stopProgressReporting(true);
        return;
    }

    const QByteArray urlBytes = session.url.toUtf8();
    const QByteArray token = m_api ? m_api->session().accessToken.toUtf8() : QByteArray {};
    const QByteArray header = token.isEmpty() ? QByteArray {} : QByteArrayLiteral("X-Emby-Token: ") + token;
    if (!setRequiredMpvProperty(handle, "http-header-fields", header.constData())) {
        m_mpvLifecycle.cancelFileLoad();
        m_errorText = QStringLiteral("libmpv rejected the authenticated media request.");
        stopProgressReporting(true);
        return;
    }
    if (!setRequiredMpvProperty(handle, "tls-verify", "yes")) {
        m_mpvLifecycle.cancelFileLoad();
        m_errorText = QStringLiteral("libmpv could not enable secure certificate verification.");
        stopProgressReporting(true);
        return;
    }
    if (m_api && m_tlsTrust) {
        const QSslCertificate certificate = m_tlsTrust->trustedCertificate(QUrl(m_api->serverUrl()));
        if (!certificate.isNull()) {
            const QString trustDirectory
                = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/tls");
            const QString trustPath = trustDirectory + QLatin1Char('/')
                + QString::fromLatin1(TlsTrustController::fingerprint(certificate)) + QStringLiteral(".pem");
            const QByteArray certificatePem = certificate.toPem();
            QDir().mkpath(trustDirectory);
            QFile trustFile(trustPath);
            if ((!trustFile.exists() || trustFile.size() == 0)
                && (!trustFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
                    || trustFile.write(certificatePem) != certificatePem.size())) {
                m_mpvLifecycle.cancelFileLoad();
                m_errorText = QStringLiteral("The trusted server certificate could not be prepared for playback.");
                stopProgressReporting(true);
                return;
            }
            trustFile.close();
            const QByteArray encodedTrustPath = QFile::encodeName(trustPath);
            if (!setRequiredMpvProperty(handle, "tls-ca-file", encodedTrustPath.constData())) {
                m_mpvLifecycle.cancelFileLoad();
                m_errorText = QStringLiteral("libmpv rejected the trusted server certificate.");
                stopProgressReporting(true);
                return;
            }
        }
    }
    if (startSeconds > 0.0) {
        const QByteArray startValue = QByteArray::number(startSeconds, 'f', 3);
        if (!setOption(handle, "start", startValue.constData())) {
            m_mpvLifecycle.cancelFileLoad();
            m_errorText = QStringLiteral("libmpv rejected the resume position.");
            stopProgressReporting(true);
            return;
        }
        qInfo() << "player: instructing mpv to start at resume position seconds=" << startSeconds;
    }
    const QByteArray loadFileOptions = MpvOptionProfile::loadFileOptions(session);
    const char *loadCommand[] = { "loadfile", urlBytes.constData(), "replace", "-1",
        loadFileOptions.isEmpty() ? nullptr : loadFileOptions.constData(), nullptr };
    if (mpv_command(handle, loadCommand) < 0) {
        m_mpvLifecycle.cancelFileLoad();
        m_errorText = QStringLiteral("libmpv rejected the playback URL.");
        stopProgressReporting(true);
        return;
    }
}

void PlayerController::setMediaSegments(const QString& itemId, const std::vector<MediaSegment>& segments)
{
    if (!m_sessionActive || itemId != m_session.itemId)
        return;

    m_session.segments = segments;
    m_timeline.setSession(m_session);
    m_timeline.updatePosition(m_positionTracker.position());
    qInfo() << "player: media segments updated" << itemId << "count=" << segments.size();
    emit segmentsChanged();
}

void PlayerController::togglePause()
{
    qInfo() << "player: toggle pause requested";
    mpvCommand({ QByteArrayLiteral("no-osd"), QByteArrayLiteral("cycle"), QByteArrayLiteral("pause") });
}

void PlayerController::setPaused(bool paused)
{
    qInfo() << "player: pause requested" << paused;
    mpvCommand({ QByteArrayLiteral("no-osd"), QByteArrayLiteral("set"), QByteArrayLiteral("pause"),
        paused ? QByteArrayLiteral("yes") : QByteArrayLiteral("no") });
}

void PlayerController::prepareForBackground()
{
    if (!platformUsesBackgroundPlaybackPolicy())
        return;
    if (!m_sessionActive) {
        m_idleMpvPreparationEnabled = false;
        m_idleMpvPreparationScheduled = false;
        destroyIdleMpv("background");
        return;
    }
    qInfo() << "player: playback position snapshot background"
            << "position=" << m_positionTracker.position();
}

void PlayerController::teardownForBackground()
{
    if (!platformUsesBackgroundPlaybackPolicy())
        return;
    prepareForBackground();
    if (!m_sessionActive)
        return;
    // A transient hidden state during surface handoff must not tear down
    // ordinary playback. A real background transition outlasts this timer,
    // at which point Starfish must release the system media pipeline.
    qInfo() << "player: scheduling teardown for background/hidden app state";
    m_backgroundTeardownTimer.start();
}

void PlayerController::resyncForForeground()
{
    if (!platformUsesBackgroundPlaybackPolicy())
        return;
    if (m_backgroundTeardownTimer.isActive()) {
        m_backgroundTeardownTimer.stop();
        qInfo() << "player: cancelled transient background teardown";
    }
    if (!m_visible)
        return;

    qInfo() << "player: foreground position resync requested";
    requestMpvPositionRefresh("foreground");

    for (int delayMs : { 250, 1000, 2500 }) {
        QTimer::singleShot(delayMs, this, [this]() {
            if (!m_visible)
                return;
            requestMpvPositionRefresh("foreground-delayed");
        });
    }
}

void PlayerController::seekBack()
{
    beginRelativeSeekCommand(-10.0);
}

void PlayerController::seekForward()
{
    beginRelativeSeekCommand(10.0);
}

void PlayerController::seek(double seconds)
{
    if (!std::isfinite(seconds))
        return;

    const double clampedSeconds = m_positionTracker.clamp(seconds);
    // Use absolute+exact so a committed click lands on the requested frame.
    qInfo() << "player: absolute exact seek" << clampedSeconds;
    beginSeekCommand(clampedSeconds, QByteArray("absolute+exact"));
}

void PlayerController::previewSeekBy(double deltaSeconds)
{
    beginRelativeSeekCommand(deltaSeconds);
}

void PlayerController::toggleDebugOsd()
{
    if (!mpvCommand({ QByteArrayLiteral("script-binding"), QByteArrayLiteral("stats/display-stats-toggle") }))
        return;
    m_debugOsdVisible = !m_debugOsdVisible;
    emit playbackStateChanged();
}

void PlayerController::toggleSubtitles()
{
    if (const auto target = m_tracks.toggleSubtitleTarget())
        selectSubtitle(*target);
}

void PlayerController::cycleSubtitles()
{
    // Off (index 0) -> first track -> second -> ... -> last -> Off.
    if (const auto target = m_tracks.cycleSubtitleTarget())
        selectSubtitle(*target);
}

void PlayerController::enableSubtitles()
{
    if (const auto target = m_tracks.enableSubtitleTarget())
        selectSubtitle(*target);
}

void PlayerController::cycleAudio()
{
    if (const auto target = m_tracks.cycleAudioTarget())
        selectAudio(*target);
}

void PlayerController::selectSubtitle(int index)
{
    const std::optional<QByteArrayList> command = m_tracks.subtitleCommand(index);
    if (!command)
        return;

    mpvCommand(*command);
    m_tracks.applySubtitleSelection(index);
    if (!m_tracks.subtitlesEnabled()) {
        m_window->clearOverlay();
    }
    updateReportedStreamSelection(true);
    emit streamSelectionChanged(m_session.audioStreamIndex, m_session.subtitleStreamIndex);
    emit tracksChanged();
}

void PlayerController::selectSubtitleStreamIndex(int streamIndex)
{
    const int uiIndex = streamIndex < 0 ? 0 : uiTrackIndexForStream(QStringLiteral("Subtitle"), streamIndex, 1);
    if (uiIndex < 0) {
        qWarning() << "player: Jellyfin subtitle stream index not found" << streamIndex;
        return;
    }
    qInfo() << "player: selecting Jellyfin subtitle stream" << streamIndex << "uiIndex" << uiIndex;
    selectSubtitle(uiIndex);
}

void PlayerController::selectAudio(int index)
{
    const std::optional<QByteArrayList> command = m_tracks.audioCommand(index);
    if (!command)
        return;

    if (!mpvCommand(*command))
        return;
    m_tracks.applyAudioSelection(index);
    updateReportedStreamSelection(true);
    emit streamSelectionChanged(m_session.audioStreamIndex, m_session.subtitleStreamIndex);
    platformAudioTrackChanged(index);
    emit tracksChanged();
}

void PlayerController::selectAudioStreamIndex(int streamIndex)
{
    const int uiIndex = uiTrackIndexForStream(QStringLiteral("Audio"), streamIndex, 0);
    if (uiIndex < 0) {
        qWarning() << "player: Jellyfin audio stream index not found" << streamIndex;
        return;
    }
    qInfo() << "player: selecting Jellyfin audio stream" << streamIndex << "uiIndex" << uiIndex;
    selectAudio(uiIndex);
}

int PlayerController::uiTrackIndexForStream(const QString& type, int streamIndex, int firstUiIndex) const
{
    int uiIndex = firstUiIndex;
    for (const MediaStreamInfo& stream : m_session.mediaStreams) {
        if (stream.type.compare(type, Qt::CaseInsensitive) != 0)
            continue;
        if (stream.index == streamIndex)
            return uiIndex;
        ++uiIndex;
    }
    return -1;
}

int PlayerController::streamIndexForUiTrack(const QString& type, int uiIndex, int firstUiIndex) const
{
    if (uiIndex < firstUiIndex)
        return -1;
    int candidateUiIndex = firstUiIndex;
    for (const MediaStreamInfo& stream : m_session.mediaStreams) {
        if (stream.type.compare(type, Qt::CaseInsensitive) != 0)
            continue;
        if (candidateUiIndex == uiIndex)
            return stream.index;
        ++candidateUiIndex;
    }
    return -1;
}

void PlayerController::updateReportedStreamSelection(bool sendProgress)
{
    m_session.audioStreamIndex = streamIndexForUiTrack(QStringLiteral("Audio"), m_tracks.selectedAudioIndex(), 0);
    m_session.subtitleStreamIndex = m_tracks.subtitlesEnabled()
        ? streamIndexForUiTrack(QStringLiteral("Subtitle"), m_tracks.selectedSubtitleIndex(), 1)
        : -1;
    const bool reportChanged = m_reporter.setStreamIndexes(m_session.audioStreamIndex, m_session.subtitleStreamIndex);
    if (reportChanged && sendProgress && m_sessionActive)
        m_reporter.reportProgress(secondsToTicks(m_positionTracker.position()), m_paused, effectivePlaybackSpeed());
}

void PlayerController::nextChapter()
{
    if (!m_tracks.hasChapters())
        return;
    m_positionTracker.allowRegression();
    mpvCommand({ QByteArrayLiteral("add"), QByteArrayLiteral("chapter"), QByteArrayLiteral("1") });
}

void PlayerController::previousChapter()
{
    if (!m_tracks.hasChapters())
        return;
    m_positionTracker.allowRegression();
    mpvCommand({ QByteArrayLiteral("add"), QByteArrayLiteral("chapter"), QByteArrayLiteral("-1") });
}

void PlayerController::stop()
{
    stopWithReason(QStringLiteral("unspecified"));
}

void PlayerController::stopWithReason(const QString& reason)
{
    Diagnostics::Task task(QStringLiteral("player_stop"),
        { { QStringLiteral("reason"), reason }, { QStringLiteral("sessionActive"), m_sessionActive } });
    qInfo() << "player: stop requested" << reason << "sessionActive" << m_sessionActive;
    if (!m_sessionActive)
        return;

    // Drop the UI synchronously so navigation never waits for backend unload.
    stopProgressReporting(false);

    if (auto *handle = m_mpvLifecycle.handle())
        setMpvProperty(handle, "http-header-fields", "");
    mpvCommand({ QByteArrayLiteral("stop") });
    scheduleMpvTeardown();
}

void PlayerController::setNightModeEnabled(bool enabled)
{
    if (m_nightModeEnabled.load() == enabled)
        return;

    m_nightModeEnabled = enabled;
    if (auto *handle = m_mpvLifecycle.handle()) {
        applyMpvRuntimeOption(MpvRuntimeOption::NightMode, MpvOptionApplyMode::Runtime, handle);
    } else {
        discardPreparedMpvForOptionChange("night mode change");
    }

    emit nightModeEnabledChanged();
}

void PlayerController::setToneMappingVisualizationEnabled(bool enabled)
{
    if (m_toneMappingVisualizationEnabled.load() == enabled)
        return;

    m_toneMappingVisualizationEnabled = enabled;
    if (auto *handle = m_mpvLifecycle.handle()) {
        applyMpvRuntimeOption(MpvRuntimeOption::ToneMappingVisualization, MpvOptionApplyMode::Runtime, handle);
    } else {
        discardPreparedMpvForOptionChange("tone mapping visualization change");
    }

    emit toneMappingVisualizationEnabledChanged();
}

void PlayerController::setAudioDelayMs(int delayMs)
{
    const int clampedDelayMs = qBound(-2000, delayMs, 2000);
    if (m_audioDelayMs.load() == clampedDelayMs) {
        qInfo() << "player: audio delay unchanged" << clampedDelayMs << "ms";
        return;
    }

    m_audioDelayMs = clampedDelayMs;
    qInfo() << "player: audio delay requested" << clampedDelayMs << "ms"
            << "visible=" << m_visible;
    if (auto *handle = m_mpvLifecycle.handle()) {
        applyMpvRuntimeOption(MpvRuntimeOption::AudioDelay, MpvOptionApplyMode::Runtime, handle);
    } else {
        qInfo() << "player: audio delay stored without active mpv";
        discardPreparedMpvForOptionChange("audio delay change");
    }

    emit audioDelayMsChanged();
    emit effectiveAudioDelayMsChanged();
}

void PlayerController::setFileAudioDelayMs(int delayMs)
{
    const int clampedDelayMs = qBound(-2000, delayMs, 2000);
    if (m_fileAudioDelayMs.exchange(clampedDelayMs) == clampedDelayMs)
        return;
    if (auto *handle = m_mpvLifecycle.handle())
        applyMpvRuntimeOption(MpvRuntimeOption::AudioDelay, MpvOptionApplyMode::Runtime, handle);
    emit fileAudioDelayMsChanged();
    emit effectiveAudioDelayMsChanged();
}

void PlayerController::setSubtitleDelayMs(int delayMs)
{
    const int clampedDelayMs = qBound(-2000, delayMs, 2000);
    if (m_subtitleDelayMs.exchange(clampedDelayMs) == clampedDelayMs)
        return;
    if (auto *handle = m_mpvLifecycle.handle())
        applyMpvRuntimeOption(MpvRuntimeOption::SubtitleDelay, MpvOptionApplyMode::Runtime, handle);
    emit subtitleDelayMsChanged();
}

void PlayerController::setAudioOutputMode(const QString& mode)
{
    const QString normalized = normalizedAudioOutputMode(mode);
    if (m_audioOutputMode == normalized)
        return;

    m_audioOutputMode = normalized;
    qInfo() << "player: audio output mode changed" << normalized << "visible=" << m_visible;
    discardPreparedMpvForOptionChange("audio output mode change");
    emit audioOutputModeChanged();
}

void PlayerController::setVolume(int volume)
{
    const int clampedVolume = qBound(0, volume, 100);
    if (m_volume.load() == clampedVolume)
        return;

    m_volume = clampedVolume;
    mpvCommand({ QByteArrayLiteral("no-osd"), QByteArrayLiteral("set"), QByteArrayLiteral("volume"),
        QByteArray::number(clampedVolume) });
    emit volumeChanged();
}

void PlayerController::adjustVolume(int delta)
{
    if (delta == 0)
        return;
    setVolume(m_volume.load() + delta);
}

void PlayerController::setPlaybackSpeed(double speed)
{
    changePlaybackSpeed(speed, false);
}

void PlayerController::setSyncPlaybackSpeed(double speed)
{
    changePlaybackSpeed(speed, true);
}

void PlayerController::clearSyncPlaybackSpeed()
{
    changePlaybackSpeed(1.0, false, true);
}

void PlayerController::changePlaybackSpeed(double speed, bool syncOverride, bool clearSyncOverride)
{
    if (!std::isfinite(speed))
        return;

    const double oldUserSpeed = m_playbackSpeed;
    const double oldEffectiveSpeed = effectivePlaybackSpeed();

    if (clearSyncOverride) {
        if (!m_syncPlaybackSpeedActive)
            return;
        m_syncPlaybackSpeedActive = false;
        m_syncPlaybackSpeed = 1.0;
    } else if (syncOverride) {
        const double clampedSpeed = qBound(0.2, std::round(speed * 1000.0) / 1000.0, 2.0);
        if (m_syncPlaybackSpeedActive && qFuzzyCompare(m_syncPlaybackSpeed, clampedSpeed))
            return;
        m_syncPlaybackSpeedActive = true;
        m_syncPlaybackSpeed = clampedSpeed;
    } else {
        const double clampedSpeed = qBound(0.25, std::round(speed * 1000.0) / 1000.0, 4.0);
        if (qFuzzyCompare(m_playbackSpeed, clampedSpeed))
            return;
        m_playbackSpeed = clampedSpeed;
    }

    const double newEffectiveSpeed = effectivePlaybackSpeed();
    const bool effectiveChanged = !qFuzzyCompare(oldEffectiveSpeed, newEffectiveSpeed);
    qInfo() << "player: playback speed changed"
            << "user=" << m_playbackSpeed << "effective=" << newEffectiveSpeed
            << "syncOverride=" << m_syncPlaybackSpeedActive;

    if (effectiveChanged) {
        if (auto *handle = m_mpvLifecycle.handle()) {
            applyMpvRuntimeOption(MpvRuntimeOption::PlaybackSpeed, MpvOptionApplyMode::Runtime, handle);
        } else {
            discardPreparedMpvForOptionChange("playback speed change");
        }
        emit effectivePlaybackSpeedChanged();
    }
    if (!qFuzzyCompare(oldUserSpeed, m_playbackSpeed))
        emit playbackSpeedChanged();
}

void PlayerController::setSubtitlePreferences(const SubtitlePreferences& preferences)
{
    if (m_subtitlePreferences == preferences)
        return;

    const SubtitlePreferences previousPreferences = m_subtitlePreferences;
    const bool preserveTrackSelection = previousPreferences.language == preferences.language
        && previousPreferences.mode == preferences.mode && previousPreferences.audioMode == preferences.audioMode
        && previousPreferences.audioLanguage == preferences.audioLanguage;
    m_subtitlePreferences = preferences;
    qInfo() << "player: track preferences changed"
            << "subtitleMode=" << preferences.mode << "subtitleLanguage=" << preferences.language
            << "audioMode=" << preferences.audioMode << "audioLanguage=" << preferences.audioLanguage
            << "styling=" << preferences.styling << "geometryOverride=" << preferences.alwaysOverridePositionAndSize
            << "subPos=" << preferences.verticalPosition << "subScale=" << preferences.scalePercent;
    if (auto *handle = m_mpvLifecycle.handle()) {
        applyMpvSubtitleOptions(MpvOptionApplyMode::Runtime, handle, preserveTrackSelection,
            preserveTrackSelection ? &previousPreferences : nullptr);
    } else {
        discardPreparedMpvForOptionChange("subtitle preferences change");
    }
}

void PlayerController::setDemuxerBudget(const QByteArray& maxBytes, const QByteArray& maxBackBytes)
{
    bool changed = false;
    if (!maxBytes.isEmpty())
        m_automaticDemuxerMaxBytes = maxBytes;
    const QByteArray effectiveMaxBytes
        = m_forwardCacheSizeMiB > 0 ? QByteArray::number(m_forwardCacheSizeMiB) + 'M' : m_automaticDemuxerMaxBytes;
    if (!effectiveMaxBytes.isEmpty() && m_demuxerMaxBytes != effectiveMaxBytes) {
        m_demuxerMaxBytes = effectiveMaxBytes;
        changed = true;
    }
    if (!maxBackBytes.isEmpty() && m_demuxerMaxBackBytes != maxBackBytes) {
        m_demuxerMaxBackBytes = maxBackBytes;
        changed = true;
    }
    if (changed)
        discardPreparedMpvForOptionChange("demuxer budget change");
}

void PlayerController::setForwardCacheSizeMiB(int sizeMiB)
{
    sizeMiB = std::clamp(sizeMiB, 16, 4096);
    if (sizeMiB == m_forwardCacheSizeMiB)
        return;

    m_forwardCacheSizeMiB = sizeMiB;
    const QByteArray effectiveMaxBytes = QByteArray::number(sizeMiB) + 'M';
    if (effectiveMaxBytes == m_demuxerMaxBytes)
        return;

    m_demuxerMaxBytes = effectiveMaxBytes;
    qInfo() << "player: forward cache size" << QString::number(sizeMiB) + QStringLiteral(" MiB");
    discardPreparedMpvForOptionChange("forward cache size change");
}
void PlayerController::setMpvConfigPolicy(const MpvConfigPolicy& policy)
{
    if (!policy.valid || policy == m_mpvConfigPolicy)
        return;
    m_mpvConfigPolicy = policy;
    qInfo() << "player: mpv configuration policy changed; applies on next playback";
    discardPreparedMpvForOptionChange("mpv config policy change");
}

void PlayerController::startProgressReporting()
{
    Diagnostics::logEvent(QStringLiteral("player"), QStringLiteral("progress_reporting_start"),
        { { QStringLiteral("itemId"), m_session.itemId } });
    if (m_progressTimer.isActive())
        return;
    m_progressTimer.start();

    updateReportedStreamSelection(false);
    m_reporter.start(m_session, effectivePlaybackSpeed());
}

void PlayerController::stopProgressReporting(bool failed, bool completed)
{
    Diagnostics::Phase phase(QStringLiteral("player"), QStringLiteral("stop_progress_reporting"),
        { { QStringLiteral("failed"), failed }, { QStringLiteral("completed"), completed } });
    if (!m_sessionActive && !m_progressTimer.isActive()) {
        qInfo() << "player: stopProgressReporting skipped sessionActive=" << m_sessionActive;
        return;
    }

    const bool wasVisible = m_visible;
    const bool wasSessionActive = m_sessionActive;
    qInfo() << "player: stopProgressReporting sessionActive=" << m_sessionActive << "visible=" << m_visible
            << "failed=" << failed << "completed=" << completed;
    m_progressTimer.stop();
    m_uiPositionTimer.stop();
    m_seekWatchdogTimer.stop();

    const auto session = m_session;
    const qint64 positionTicks
        = completed && session.runtimeTicks > 0 ? session.runtimeTicks : secondsToTicks(m_positionTracker.position());
    m_reporter.stop(positionTicks, failed, effectivePlaybackSpeed());

    resetPlaybackUiState();
    m_window->clearOverlay();
    emit positionChanged();
    emit playbackStateChanged();
    emit tracksChanged();
    emit segmentsChanged();
    emit trickplayChanged();
    if (wasVisible != m_visible)
        emit visibleChanged();
    if (wasSessionActive != m_sessionActive)
        emit sessionActiveChanged();
    emit playbackStopped(session.itemId, positionTicks, completed);
}

void PlayerController::resetPlaybackUiState()
{
    m_visible = false;
    m_sessionActive = false;
    m_fileLoaded = false;
    m_seekDispatchReady = false;
    if (m_hdrPlayback) {
        m_hdrPlayback = false;
        emit hdrPlaybackChanged();
    }
    m_paused = false;
    m_buffering = false;
    m_bufferingPercent = 0;
    m_seeking = false;
    m_pendingSeek = false;
    m_pendingSeekFlags.clear();
    m_positionTracker.clear();
    m_debugOsdVisible = false;
    m_decoderDroppedFrames = 0;
    m_outputDroppedFrames = 0;
    m_timeline.clear();
    rebuildTrickplaySheetUrls();
    m_statusText = QStringLiteral("Ready");
    m_mediaKind = QStringLiteral("none");
    if (m_tracks.clearChapters()) {
        emit chaptersChanged();
    }
}

bool PlayerController::mpvCommand(QByteArrayList command)
{
    auto *handle = m_mpvLifecycle.handle();
    if (!handle) {
        qInfo() << "player: mpv command dropped (no handle):" << command;
        return false;
    }

    QList<const char *> argv;
    argv.reserve(command.size() + 1);
    for (const QByteArray& arg : std::as_const(command))
        argv.append(arg.constData());
    argv.append(nullptr);

    const int error = mpv_command_async(handle, 0, argv.data());
    if (error < 0) {
        qWarning() << "player: mpv_command_async failed" << command << "error=" << error << mpv_error_string(error);
        return false;
    }
    return true;
}

QByteArrayList PlayerController::buildSeekCommand(double targetSeconds, const QByteArray& flags) const
{
    return { QByteArrayLiteral("no-osd"), QByteArrayLiteral("seek"), QByteArray::number(targetSeconds, 'f', 3), flags };
}

bool PlayerController::beginSeekCommand(double targetSeconds, const QByteArray& flags)
{
    if (!m_sessionActive)
        return false;

    const double clampedTarget = clampedPosition(targetSeconds);
    m_seeking = true;
    m_positionTracker.beginSeek(clampedTarget);
    notifyPlaybackStateChanged();

    if (!m_seekDispatchReady) {
        m_pendingSeek = true;
        m_pendingSeekTargetSeconds = clampedTarget;
        m_pendingSeekFlags = flags;
        qInfo() << "player: deferring seek until initial playback restart target=" << clampedTarget
                << "flags=" << flags;
        return true;
    }

    m_pendingSeek = false;
    m_pendingSeekFlags.clear();
    m_seekWatchdogTimer.start();
    if (mpvCommand(buildSeekCommand(clampedTarget, flags)))
        return true;

    m_seeking = false;
    m_seekWatchdogTimer.stop();
    m_positionTracker.cancelSeek();
    notifyPlaybackStateChanged();
    return false;
}

bool PlayerController::beginRelativeSeekCommand(double deltaSeconds)
{
    if (!std::isfinite(deltaSeconds) || deltaSeconds == 0.0)
        return false;

    const double optimisticTarget = clampedPosition(seekAnchorPosition() + deltaSeconds);
    qInfo() << "player: relative keyframe seek" << deltaSeconds << "absoluteTarget=" << optimisticTarget;
    return beginSeekCommand(optimisticTarget, QByteArrayLiteral("absolute+keyframes"));
}

void PlayerController::flushPendingSeek()
{
    if (!m_pendingSeek || !m_sessionActive || !m_seekDispatchReady)
        return;

    const double target = m_pendingSeekTargetSeconds;
    const QByteArray flags = m_pendingSeekFlags;
    m_pendingSeek = false;
    m_pendingSeekFlags.clear();
    m_seekWatchdogTimer.start();
    qInfo() << "player: dispatching deferred seek target=" << target << "flags=" << flags;
    if (mpvCommand(buildSeekCommand(target, flags)))
        return;

    m_seeking = false;
    m_seekWatchdogTimer.stop();
    m_positionTracker.cancelSeek();
    notifyPlaybackStateChanged();
}

void PlayerController::handleMpvEvent(mpv_event *event)
{
    if (!event)
        return;

    switch (event->event_id) {
    case MPV_EVENT_FILE_LOADED:
        m_mpvLifecycle.completeFileLoad();
        QMetaObject::invokeMethod(this, [this]() {
            qInfo() << "player: file loaded";
            m_fileLoaded = true;
            notifyPlaybackStateChanged();
            startProgressReporting();
        });
        break;
    case MPV_EVENT_PLAYBACK_RESTART:
        QMetaObject::invokeMethod(this, [this]() {
            qInfo() << "player: playback restart";
            const bool hadPendingSeek = m_pendingSeek;
            m_seekDispatchReady = true;
            if (hadPendingSeek) {
                flushPendingSeek();
            } else if (m_seeking) {
                m_seeking = false;
                m_seekWatchdogTimer.stop();
            }
            notifyPlaybackStateChanged();
        });
        break;
    case MPV_EVENT_GET_PROPERTY_REPLY: {

        if (event->reply_userdata != kTimePosRefreshReply)
            break;

        auto *property = static_cast<mpv_event_property *>(event->data);
        if (!property || !property->data || property->format != MPV_FORMAT_DOUBLE)
            break;

        const double seconds = *static_cast<double *>(property->data);
        QMetaObject::invokeMethod(this, [this, seconds]() { setPositionSeconds(seconds); });
        break;
    }
    case MPV_EVENT_PROPERTY_CHANGE: {
        auto *property = static_cast<mpv_event_property *>(event->data);
        if (!property || !property->data)
            break;

        if (strcmp(property->name, "pause") == 0 && property->format == MPV_FORMAT_FLAG) {
            const bool paused = *static_cast<int *>(property->data);
            QMetaObject::invokeMethod(this, [this, paused]() {
                if (m_paused != paused)
                    qInfo() << "player: pause state changed" << paused;
                m_paused = paused;
                if (!m_paused)
                    requestMpvPositionRefresh("unpause");
                notifyPlaybackStateChanged();
            });
        } else if (strcmp(property->name, "paused-for-cache") == 0 && property->format == MPV_FORMAT_FLAG) {
            const bool buffering = *static_cast<int *>(property->data);
            QMetaObject::invokeMethod(this, [this, buffering]() {
                m_buffering = buffering;
                if (!buffering)
                    m_bufferingPercent = 0;
                notifyPlaybackStateChanged();
            });
        } else if (strcmp(property->name, "cache-buffering-state") == 0 && property->format == MPV_FORMAT_INT64) {
            const auto percent = static_cast<int>(*static_cast<int64_t *>(property->data));
            QMetaObject::invokeMethod(this, [this, percent]() {
                m_bufferingPercent = percent;
                notifyPlaybackStateChanged();
            });
        } else if ((strcmp(property->name, "decoder-frame-drop-count") == 0
                       || strcmp(property->name, "frame-drop-count") == 0)
            && property->format == MPV_FORMAT_INT64) {
            const bool decoder = strcmp(property->name, "decoder-frame-drop-count") == 0;
            const qint64 count = *static_cast<int64_t *>(property->data);
            QMetaObject::invokeMethod(this, [this, decoder, count]() {
                qint64& current = decoder ? m_decoderDroppedFrames : m_outputDroppedFrames;
                if (current == count)
                    return;
                current = count;
                emit performanceStatsChanged();
            });
        } else if (strcmp(property->name, "seeking") == 0 && property->format == MPV_FORMAT_FLAG) {
            const bool seeking = *static_cast<int *>(property->data);
            QMetaObject::invokeMethod(this, [this, seeking]() {
                m_seeking = seeking;
                if (m_seeking)
                    m_seekWatchdogTimer.start();
                else
                    m_seekWatchdogTimer.stop();
                notifyPlaybackStateChanged();
            });
        } else if (strcmp(property->name, "time-pos") == 0 && property->format == MPV_FORMAT_DOUBLE) {
            const double seconds = *static_cast<double *>(property->data);
            QMetaObject::invokeMethod(this, [this, seconds]() { setPositionSeconds(seconds); });
        } else if (strcmp(property->name, "duration") == 0 && property->format == MPV_FORMAT_DOUBLE) {
            const double seconds = *static_cast<double *>(property->data);
            QMetaObject::invokeMethod(this, [this, seconds]() {
                m_positionTracker.setDuration(seconds);
                if (m_timeline.updatePosition(m_positionTracker.position()))
                    emit segmentsChanged();
                emit positionChanged();
            });
        } else if (strcmp(property->name, "volume") == 0 && property->format == MPV_FORMAT_DOUBLE) {
            const auto volume = static_cast<int>(std::round(*static_cast<double *>(property->data)));
            QMetaObject::invokeMethod(this, [this, volume]() {
                const int clampedVolume = qBound(0, volume, 100);
                if (m_volume.load() == clampedVolume)
                    return;
                m_volume = clampedVolume;
                emit volumeChanged();
            });
        } else if (strcmp(property->name, "hwdec-current") == 0 && property->format == MPV_FORMAT_STRING) {
            const auto *decoder = static_cast<char **>(property->data);
            const QByteArray decoderName(decoder && *decoder ? *decoder : "");
            QMetaObject::invokeMethod(this, [decoderName]() {
                qInfo() << "player: hardware decoder"
                        << (decoderName.isEmpty() ? QByteArrayLiteral("none") : decoderName);
            });
        } else if (strcmp(property->name, "video-params/transfer") == 0 && property->format == MPV_FORMAT_STRING) {
            const auto *transferValue = static_cast<char **>(property->data);
            const QByteArray transfer(transferValue && *transferValue ? *transferValue : "");
            const QByteArray normalizedTransfer = transfer.toLower();
            const bool hdrPlayback = normalizedTransfer == QByteArrayLiteral("pq")
                || normalizedTransfer == QByteArrayLiteral("hlg") || normalizedTransfer.contains("2084")
                || normalizedTransfer.contains("b67");
            QMetaObject::invokeMethod(this, [this, hdrPlayback, transfer]() {
                if (m_hdrPlayback != hdrPlayback) {
                    m_hdrPlayback = hdrPlayback;
                    emit hdrPlaybackChanged();
                    if (auto *handle = m_mpvLifecycle.handle())
                        applyMpvSubtitleOptions(MpvOptionApplyMode::Runtime, handle, true);
                }
                qInfo() << "player: video transfer" << transfer << "HDR subtitle mode"
                        << (m_hdrPlayback ? "enabled" : "disabled");
            });
        } else if (strcmp(property->name, "track-list") == 0 && property->format == MPV_FORMAT_NODE) {
            const auto *node = static_cast<mpv_node *>(property->data);
            const ParsedPlaybackTracks tracks = PlaybackTrackParser::parseTracks(node);
            QMetaObject::invokeMethod(this, [this, tracks]() {
                m_tracks.applyParsedTracks(tracks);
                if (m_restoreStreamSelection) {
                    const int audioStreamIndex = m_session.audioStreamIndex;
                    const int subtitleStreamIndex = m_session.subtitleStreamIndex;
                    const int audioUiIndex = uiTrackIndexForStream(QStringLiteral("Audio"), audioStreamIndex, 0);
                    const int subtitleUiIndex = subtitleStreamIndex < 0
                        ? 0
                        : uiTrackIndexForStream(QStringLiteral("Subtitle"), subtitleStreamIndex, 1);
                    const bool tracksPending = (audioUiIndex >= 0 && !m_tracks.audioCommand(audioUiIndex))
                        || (subtitleUiIndex >= 0 && !m_tracks.subtitleCommand(subtitleUiIndex));
                    if (!tracksPending) {
                        m_restoreStreamSelection = false;
                        if (audioUiIndex >= 0)
                            selectAudio(audioUiIndex);
                        if (subtitleUiIndex >= 0)
                            selectSubtitle(subtitleUiIndex);
                    }
                }
                if (!m_restoreStreamSelection)
                    updateReportedStreamSelection(true);
                qInfo() << "player: subtitle tracks" << tracks.subtitleLabels << "selected"
                        << tracks.selectedSubtitleIndex << "audio tracks" << tracks.audioLabels << "selected"
                        << tracks.selectedAudioIndex;
                emit tracksChanged();
            });
        } else if (strcmp(property->name, "chapter-list") == 0 && property->format == MPV_FORMAT_NODE) {
            const auto *node = static_cast<mpv_node *>(property->data);
            const QVariantList chapters = PlaybackTrackParser::parseChapters(node);
            QMetaObject::invokeMethod(this, [this, chapters]() {
                m_tracks.setChapters(chapters);
                qInfo() << "player: chapters" << chapters.size();
                emit chaptersChanged();
            });
        } else if (strcmp(property->name, "chapter") == 0 && property->format == MPV_FORMAT_INT64) {
            const int chapter = static_cast<int>(*static_cast<int64_t *>(property->data));
            QMetaObject::invokeMethod(this, [this, chapter]() {
                if (m_tracks.setCurrentChapter(chapter))
                    emit chaptersChanged();
            });
        }
        break;
    }
    case MPV_EVENT_END_FILE: {
        auto *endFile = static_cast<mpv_event_end_file *>(event->data);
        const bool failed = endFile && endFile->error < 0;
        const int endFileReason = endFile ? endFile->reason : -1;
        const int endFileError = endFile ? endFile->error : 0;
        const bool failedBeforeLoad = failed && !m_fileLoaded;
        // A fresh mpv core is created for every play request, so an END_FILE
        // while loading belongs to this request. Clear the pending marker on
        // both success and failure; otherwise a failed manifest stays stuck in
        // the preparing state forever.
        m_mpvLifecycle.cancelFileLoad();
        const bool completed = !failed && endFileReason == MPV_END_FILE_REASON_EOF;
        QMetaObject::invokeMethod(this, [this, failed, failedBeforeLoad, completed, endFileReason, endFileError]() {
            qInfo() << "player: end file (main thread) failed=" << failed << "completed=" << completed
                    << "sessionActive=" << m_sessionActive << "reason=" << endFileReason
                    << endFileReasonName(endFileReason) << "error=" << endFileError
                    << (endFileError < 0 ? mpv_error_string(endFileError) : "");
            if (failed) {
                m_errorText
                    = QStringLiteral("Playback failed: %1").arg(QString::fromUtf8(mpv_error_string(endFileError)));
            }
            const QString failedItemId = m_session.itemId;
            const qint64 failedPositionTicks = secondsToTicks(m_positionTracker.position());
            const QString failureMessage = m_errorText;
            const int audioStreamIndex = m_session.audioStreamIndex;
            const int subtitleStreamIndex = m_session.subtitleStreamIndex;
            const bool retryableCodecFailure
                = PlaybackFailurePolicy::isRetryableCodecFailure(m_session.playMethod, failedBeforeLoad, endFileError);
            stopProgressReporting(failed, completed);
            if (failedBeforeLoad) {
                emit playbackLoadFailed(failedItemId, failedPositionTicks, failureMessage, retryableCodecFailure,
                    audioStreamIndex, subtitleStreamIndex);
            }
            scheduleMpvTeardown();
        });
        break;
    }
    case MPV_EVENT_SHUTDOWN:
        m_mpvLifecycle.requestEventLoopStop();
        QMetaObject::invokeMethod(this, [this]() {
            qInfo() << "player: mpv shutdown";
            if (m_sessionActive)
                stopProgressReporting(false);
        });
        break;
    default:
        break;
    }
}

void PlayerController::updatePlaybackStatusText()
{
    if (m_seeking) {
        m_statusText = QStringLiteral("Seeking…");
        return;
    }

    if (m_buffering) {
        if (m_bufferingPercent > 0)
            m_statusText = QStringLiteral("Buffering %1%").arg(m_bufferingPercent);
        else
            m_statusText = QStringLiteral("Buffering…");
        return;
    }

    m_statusText = m_paused ? QStringLiteral("Paused") : QStringLiteral("Playing");
}

void PlayerController::notifyPlaybackStateChanged()
{
    updatePlaybackStatusText();
    emit playbackStateChanged();
}

double PlayerController::clampedPosition(double seconds) const
{
    return m_positionTracker.clamp(seconds);
}

double PlayerController::seekAnchorPosition()
{
    return m_positionTracker.seekAnchor();
}

void PlayerController::requestMpvPositionRefresh(const char *reason)
{
    auto *handle = m_mpvLifecycle.handle();
    if (!m_sessionActive || !handle)
        return;

    const int error = mpv_get_property_async(handle, kTimePosRefreshReply, "time-pos", MPV_FORMAT_DOUBLE);
    if (error < 0)
        qWarning() << "player: async time-pos refresh failed" << (reason ? reason : "unknown")
                   << mpv_error_string(error);
}

void PlayerController::setPositionSeconds(double seconds, bool notifySegments)
{
    if (!m_positionTracker.update(seconds))
        return;

    const bool segmentChanged = m_timeline.updatePosition(m_positionTracker.position());

    emit positionChanged();
    if (notifySegments && segmentChanged)
        emit segmentsChanged();
}

QString PlayerController::activeSegmentType() const
{
    return m_timeline.activeSegmentType();
}
double PlayerController::activeSegmentEndSeconds() const
{
    return m_timeline.activeSegmentEndSeconds();
}
bool PlayerController::trickplayAvailable() const
{
    return m_timeline.trickplayAvailable();
}

QStringList PlayerController::trickplaySheetUrls() const
{
    return m_trickplaySheetUrls;
}

void PlayerController::rebuildTrickplaySheetUrls()
{
    m_trickplaySheetUrls.clear();
    if (!trickplayAvailable() || !m_api)
        return;

    const int sheetCount = m_timeline.trickplaySheetCount();
    m_trickplaySheetUrls.reserve(sheetCount);
    for (int i = 0; i < sheetCount; ++i) {
        m_trickplaySheetUrls.push_back(m_api->trickplayTileUrl(m_session.itemId, m_timeline.trickplayWidth(), i));
    }
}

void PlayerController::skipActiveSegment()
{
    if (activeSegmentType().isEmpty() || activeSegmentEndSeconds() <= 0.0)
        return;
    seek(activeSegmentEndSeconds());
}

QVariantMap PlayerController::trickplayForSeconds(double seconds) const
{
    // Returns { url, width, height, offsetX, offsetY, available } so QML can
    // paint a single tile sprite from a positioned BorderImage / clipped Image.
    QVariantMap result;
    if (!trickplayAvailable() || !m_api) {
        result.insert(QStringLiteral("available"), false);
        return result;
    }
    const PlaybackTimeline::TrickplayFrame frame = m_timeline.trickplayFrameAt(seconds);
    if (!frame.available) {
        result.insert(QStringLiteral("available"), false);
        return result;
    }
    result.insert(QStringLiteral("available"), true);
    result.insert(QStringLiteral("url"),
        m_api->trickplayTileUrl(m_session.itemId, m_timeline.trickplayWidth(), frame.sheetIndex));
    result.insert(QStringLiteral("width"), frame.width);
    result.insert(QStringLiteral("height"), frame.height);
    result.insert(QStringLiteral("offsetX"), frame.offsetX);
    result.insert(QStringLiteral("offsetY"), frame.offsetY);
    result.insert(QStringLiteral("sheetWidth"), frame.sheetWidth);
    result.insert(QStringLiteral("sheetHeight"), frame.sheetHeight);
    return result;
}

} // namespace JellyfinNative
