#include "PlayerController.h"

#include "../api/JellyfinApiFacade.h"
#include "../app/NativeAppWindow.h"
#include "../common/JellyfinTypes.h"
#include "../diagnostics/Diagnostics.h"
#include "MpvOptionProfile.h"
#include "MpvVideoItem.h"
#ifdef JELLYFIN_NATIVE_WEBOS
#include "MpvRuntime.h"
#endif
#include "PlaybackTrackParser.h"

extern "C" {
#include <mpv/client.h>
}

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QPointer>
#include <QtGlobal>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>

#if defined(__GLIBC__)
#include <malloc.h>
#endif

namespace JellyfinNative {

namespace {

    constexpr auto kDefaultMpvLogPath = "/tmp/com.codex.jellyfinwebosnative/com.codex.jellyfinnative-mpv.log";
    constexpr uint64_t kTimePosRefreshReply = 0x6a666e7074730001ULL;
    constexpr uint64_t kPlaybackTimeRefreshReply = 0x6a666e7074730002ULL;
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

    void rotateLogFile(const char *path)
    {
        const QByteArray base(path);
        std::remove((base + ".2").constData());
        std::rename((base + ".1").constData(), (base + ".2").constData());
        std::rename(path, (base + ".1").constData());
    }

    QByteArray mpvLogPath()
    {
        const QByteArray logDir = qgetenv("JELLYFIN_NATIVE_LOG_DIR");
        if (logDir.isEmpty())
            return QByteArrayLiteral(kDefaultMpvLogPath);

        QByteArray path = logDir;
        if (!path.endsWith('/'))
            path += '/';
        path += QByteArrayLiteral("com.codex.jellyfinnative-mpv.log");
        return path;
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

    bool setMpvProperty(mpv_handle *handle, const char *name, const char *value)
    {
        const int error = mpv_set_property_string(handle, name, value);
        return error >= 0 || error == MPV_ERROR_OPTION_NOT_FOUND;
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

#ifdef JELLYFIN_NATIVE_WEBOS
    void configureWebOsAudioEnvironment(const QString& audioOutputMode)
    {
        const bool useStarfishPcm
            = audioOutputMode == QStringLiteral("starfish") || audioOutputMode == QStringLiteral("starfish-pcm");
        const bool useStarfishAudio = useStarfishPcm;
        qputenv("STARFISH_AUDIO_HINT", useStarfishAudio ? QByteArrayLiteral("1") : QByteArrayLiteral("0"));
        qputenv("WEBOS_ALSA_NO_HW_PAUSE", useStarfishAudio ? QByteArrayLiteral("0") : QByteArrayLiteral("1"));
        // Selects the Starfish audio ES the fork builds: raw PCM vs the legacy AAC
        // encode path. Read by both ao_starfish and the starfish VO context.
        qputenv("STARFISH_AUDIO_CODEC", useStarfishPcm ? QByteArrayLiteral("pcm") : QByteArrayLiteral("aac"));
        qInfo() << "player: configuring webOS audio output" << audioOutputMode
                << "starfishAudioHint=" << qgetenv("STARFISH_AUDIO_HINT")
                << "starfishAudioCodec=" << qgetenv("STARFISH_AUDIO_CODEC")
                << "webosAlsaNoHwPause=" << qgetenv("WEBOS_ALSA_NO_HW_PAUSE");
    }
#endif

    qint64 secondsToTicks(double seconds)
    {
        return static_cast<qint64>(seconds * 10000000.0);
    }

    // Targeted playback memory accounting. heaptrack can't produce deep call stacks
    // on this target (every unwinder crashes), so instead of attributing by stack we
    // quantify the known big buffers directly: process RSS (and the anon/heap part),
    // glibc's live-vs-free heap totals when available, and mpv's own demuxer cache size.
    // The gap between malloc_inuse and demux_cache localises non-cache heap growth.
    void logMemoryStats(mpv_handle *handle)
    {
        (void)handle;
        long vmrss = 0, rssAnon = 0, vmdata = 0, vmswap = 0;
        if (FILE *f = fopen("/proc/self/status", "r")) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                long v;
                if (sscanf(line, "VmRSS: %ld kB", &v) == 1)
                    vmrss = v;
                else if (sscanf(line, "RssAnon: %ld kB", &v) == 1)
                    rssAnon = v;
                else if (sscanf(line, "VmData: %ld kB", &v) == 1)
                    vmdata = v;
                else if (sscanf(line, "VmSwap: %ld kB", &v) == 1)
                    vmswap = v;
            }
            fclose(f);
        }

        long long mallInUse = 0;
        long long mallFree = 0;
        long long mallArena = 0;
        long long mallMmap = 0;
#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33))
        struct mallinfo2 mi = mallinfo2();
        mallInUse = (long long)mi.uordblks; // live (non-mmap) heap
        mallFree = (long long)mi.fordblks; // freed, retained in arena
        mallArena = (long long)mi.arena; // total main-arena size
        mallMmap = (long long)mi.hblkhd; // large allocs via mmap
#elif defined(__GLIBC__)
        struct mallinfo mi = mallinfo();
        mallInUse = (long long)mi.uordblks;
        mallFree = (long long)mi.fordblks;
        mallArena = (long long)mi.arena;
        mallMmap = (long long)mi.hblkhd;
#endif

        const long long MB = 1024 * 1024;
        qInfo().nospace() << "player: memstats rss=" << vmrss / 1024 << "M anon=" << rssAnon / 1024
                          << "M vmdata=" << vmdata / 1024 << "M swap=" << vmswap / 1024
                          << "M | malloc_inuse=" << mallInUse / MB << "M arena_free=" << mallFree / MB
                          << "M arena=" << mallArena / MB << "M mmap=" << mallMmap / MB;
    }

} // namespace

PlayerController::PlayerController(NativeAppWindow *window, JellyfinApiFacade *api, QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_api(api)
    , m_reporter(api, this)
{
    m_progressTimer.setInterval(5000);
    m_uiPositionTimer.setInterval(250);
    m_backGuardTimer.setSingleShot(true);
    m_backGuardTimer.setInterval(1500);
    m_seekWatchdogTimer.setSingleShot(true);
    m_seekWatchdogTimer.setInterval(2500);
    connect(&m_backGuardTimer, &QTimer::timeout, this, [this]() {
        if (!m_sessionActive || m_backAllowed)
            return;
        m_backAllowed = true;
        qInfo() << "player: startup back guard released";
        emit playbackStateChanged();
    });
    connect(&m_uiPositionTimer, &QTimer::timeout, this, [this]() {
        if (!m_sessionActive || m_paused || m_buffering || m_seeking)
            return;

        if (!m_positionTracker.projectionIsValid())
            return;

        setPositionSeconds(projectedPositionSeconds(), PlaybackPositionTracker::Source::Projection, false);
    });
    connect(&m_seekWatchdogTimer, &QTimer::timeout, this, [this]() {
        if (!m_sessionActive || !m_seeking)
            return;

        qWarning() << "player: clearing stale seek state";
        m_seeking = false;
        m_positionTracker.cancelSeek();
        notifyPlaybackStateChanged();
    });
    connect(&m_progressTimer, &QTimer::timeout, this, [this]() {
        if (!m_sessionActive)
            return;

        logMemoryStats(m_mpvLifecycle.handle());

        m_reporter.reportProgress(secondsToTicks(m_positionTracker.position()), m_paused);
    });
    connect(&m_reporter, &PlaybackReporter::reportFailed, this, [](const QString& operation, const QString& message) {
        Diagnostics::logEvent(QStringLiteral("player"), QStringLiteral("report_failed"),
            { { QStringLiteral("operation"), operation }, { QStringLiteral("message"), message } });
    });
    scheduleIdleMpvPreparation();
}

PlayerController::~PlayerController()
{
    teardownMpv();
}

void PlayerController::teardownMpv()
{
    Diagnostics::Phase phase(QStringLiteral("shutdown"), QStringLiteral("player_teardown_mpv"));
    m_idleMpvPreparationEnabled = false;
    m_idleMpvPreparationScheduled = false;
    destroyIdleMpv("teardown");
    m_mpvLifecycle.destroy([](mpv_handle *) {
#ifndef JELLYFIN_NATIVE_WEBOS
        // Free the render context first; this is thread-safe and decouples us from
        // the scene-graph render thread (which may already be shutting down).
        if (auto *videoItem = MpvVideoItem::instance())
            videoItem->setMpvHandle(nullptr);
#endif
    });
}

void PlayerController::scheduleIdleMpvPreparation()
{
#ifdef JELLYFIN_NATIVE_WEBOS
    if (!m_idleMpvPreparationEnabled || m_idleMpvPreparationScheduled || m_idleMpvHandle || m_mpvLifecycle.handle())
        return;

    m_idleMpvPreparationScheduled = true;
    QPointer<PlayerController> controller(this);
    MpvRuntime::runAfterLoaded([controller]() {
        if (auto *app = QCoreApplication::instance()) {
            QMetaObject::invokeMethod(
                app,
                [controller]() {
                    if (controller)
                        controller->prepareIdleMpv();
                },
                Qt::QueuedConnection);
        }
    });
#endif
}

void PlayerController::prepareIdleMpv()
{
    m_idleMpvPreparationScheduled = false;
#ifdef JELLYFIN_NATIVE_WEBOS
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

    if (!configureAndInitializeMpv(handle)) {
        mpv_terminate_destroy(handle);
        qWarning() << "player: idle mpv initialization failed";
        return;
    }

    m_idleMpvHandle = handle;
    qInfo() << "player: idle-prepared mpv initialized in" << startupTimer.elapsed() << "ms";
#endif
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

bool PlayerController::configureAndInitializeMpv(mpv_handle *handle)
{
    if (!handle)
        return false;

#ifdef JELLYFIN_NATIVE_WEBOS
    configureWebOsAudioEnvironment(m_audioOutputMode);
    constexpr auto platform = MpvOptionProfile::Platform::WebOS;
#else
    constexpr auto platform = MpvOptionProfile::Platform::Desktop;
#endif
    const auto startupOptions = MpvOptionProfile::startupOptions(
        platform, m_audioOutputMode, mpvLogPath(), m_demuxerMaxBytes, m_demuxerMaxBackBytes);
    const bool configured
        = applyOptions(handle, startupOptions) && applyMpvRuntimeOptions(MpvOptionApplyMode::Initial, handle);

    return configured && mpv_initialize(handle) >= 0;
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
}

void PlayerController::scheduleMpvTeardown()
{
    auto *scheduledHandle = m_mpvLifecycle.handle();
    if (!scheduledHandle)
        return;

    QTimer::singleShot(1000, this, [this, scheduledHandle]() {
        if (m_mpvLifecycle.handle() != scheduledHandle)
            return;
        qInfo() << "player: deferred mpv teardown";
        teardownMpv();
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

QString PlayerController::audioOutputMode() const
{
    return m_audioOutputMode;
}

int PlayerController::volume() const
{
    return m_volume.load();
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
#ifdef JELLYFIN_NATIVE_WEBOS
        if (m_audioOutputMode == QStringLiteral("alsa") && !value.isEmpty()) {
            qWarning() << "player: night mode disabled for webOS ALSA split-clock";
            value.clear();
        }
#endif
        break;
    case MpvRuntimeOption::ToneMappingVisualization:
        name = "tone-mapping-visualize";
        value = mpvBool(m_toneMappingVisualizationEnabled.load());
        break;
    case MpvRuntimeOption::AudioDelay:
        name = "audio-delay";
        doubleValue = static_cast<double>(m_audioDelayMs.load()) / 1000.0;
        value = QByteArray::number(doubleValue, 'f', 3);
        break;
    }

    double appliedDoubleValue = doubleValue;
    const bool ok = mode == MpvOptionApplyMode::Initial ? setOption(handle, name, value.constData())
        : option == MpvRuntimeOption::AudioDelay ? setMpvDoubleProperty(handle, name, doubleValue, &appliedDoubleValue)
                                                 : setMpvProperty(handle, name, value.constData());
    if (!ok) {
        qWarning() << "player: failed to apply mpv runtime option" << name
                   << "mode=" << (mode == MpvOptionApplyMode::Initial ? "initial" : "runtime");
    } else if (option == MpvRuntimeOption::AudioDelay) {
        qInfo() << "player: applied audio delay"
                << "mode=" << (mode == MpvOptionApplyMode::Initial ? "initial" : "runtime")
                << "requestedMs=" << m_audioDelayMs.load() << "appliedSeconds=" << appliedDoubleValue;
    }
    return ok;
}

bool PlayerController::applyMpvRuntimeOptions(MpvOptionApplyMode mode, mpv_handle *handle)
{
    return applyMpvRuntimeOption(MpvRuntimeOption::NightMode, mode, handle)
        && applyMpvRuntimeOption(MpvRuntimeOption::ToneMappingVisualization, mode, handle)
        && applyMpvRuntimeOption(MpvRuntimeOption::AudioDelay, mode, handle) && applyMpvSubtitleOptions(mode, handle);
}

void PlayerController::discardPreparedMpvForOptionChange(const char *reason)
{
    if (m_mpvLifecycle.handle())
        return;

    destroyIdleMpv(reason);
    scheduleIdleMpvPreparation();
}

bool PlayerController::applyMpvSubtitleOptions(MpvOptionApplyMode mode, mpv_handle *handle)
{
    if (!handle)
        return false;

    auto applyString = [mode, handle](const char *name, const QByteArray& value) {
        return mode == MpvOptionApplyMode::Initial ? setOption(handle, name, value.constData())
                                                   : setMpvProperty(handle, name, value.constData());
    };

    bool ok = true;
    const auto options = MpvOptionProfile::subtitleOptions(m_subtitlePreferences, m_tracks.subtitlesEnabled());
    for (const MpvOption& option : options)
        ok &= applyString(option.name.constData(), option.value);

    if (!ok) {
        qWarning() << "player: failed to apply subtitle preferences"
                   << "mode=" << (mode == MpvOptionApplyMode::Initial ? "initial" : "runtime");
    }
    return ok;
}

bool PlayerController::ensureMpv(bool needsVideoSurface)
{
    if (m_mpvLifecycle.handle())
        return true;

    QElapsedTimer startupTimer;
    startupTimer.start();

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

        if (!configureAndInitializeMpv(handle)) {
            mpv_terminate_destroy(handle);
            m_errorText = QStringLiteral("Failed to initialize libmpv.");
            emit playbackStateChanged();
            return false;
        }
    }

    observeMpvProperties(handle);

    qInfo() << "player: mpv initialized in" << startupTimer.elapsed() << "ms"
            << "idlePrepared=" << idlePrepared;

#ifndef JELLYFIN_NATIVE_WEBOS
    if (needsVideoSurface) {
        // vo=libmpv requires the embedded MpvVideoItem to host the render context.
        // Fail loudly for video playback if QML hasn't constructed one yet.
        auto *videoItem = MpvVideoItem::instance();
        if (!videoItem) {
            qCritical() << "PlayerController: MpvVideoItem instance is missing";
            mpv_terminate_destroy(handle);
            m_errorText = QStringLiteral("The video surface is unavailable. Return to the library and try again.");
            m_statusText = QStringLiteral("Playback unavailable");
            emit playbackStateChanged();
            return false;
        }
        connect(videoItem, &MpvVideoItem::renderError, this, &PlayerController::handleVideoRenderError,
            Qt::UniqueConnection);
        videoItem->setMpvHandle(handle);
    }
#else
    (void)needsVideoSurface;
#endif

    if (!m_mpvLifecycle.adopt(handle, [this](mpv_event *event) { handleMpvEvent(event); })) {
#ifndef JELLYFIN_NATIVE_WEBOS
        if (needsVideoSurface) {
            if (auto *videoItem = MpvVideoItem::instance())
                videoItem->setMpvHandle(nullptr);
        }
#endif
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

void PlayerController::play(const PlaybackSession& session)
{
    const QString nextMediaKind = mediaKindForSession(session);
    const bool needsVideoSurface = nextMediaKind == QStringLiteral("video");
    Diagnostics::Task task(QStringLiteral("player_play"),
        { { QStringLiteral("itemId"), session.itemId }, { QStringLiteral("title"), session.title },
            { QStringLiteral("mediaKind"), nextMediaKind } });
    qInfo() << "player: play requested" << session.title << "method=" << session.playMethod
            << "mediaKind=" << nextMediaKind << "startTimeTicks=" << session.startTimeTicks;

    if (m_mpvLifecycle.handle()) {
        qInfo() << "player: tearing down stale mpv before play";
        teardownMpv();
    }

    m_window->clearOverlay();
    if (needsVideoSurface) {
        QElapsedTimer playbackSurfaceTimer;
        playbackSurfaceTimer.start();
        if (!m_window->prepareForPlaybackSurface()) {
            m_errorText = QStringLiteral("Failed to prepare the native playback surface.");
            qWarning() << "player: prepareForPlaybackSurface failed after" << playbackSurfaceTimer.elapsed() << "ms";
            emit playbackStateChanged();
            return;
        }
        qInfo() << "player: prepareForPlaybackSurface completed in" << playbackSurfaceTimer.elapsed() << "ms";
    } else {
        qInfo() << "player: audio-only playback does not request a video surface";
    }

    if (!ensureMpv(needsVideoSurface))
        return;

    m_session = session;
    m_timeline.setSession(session);
    rebuildTrickplaySheetUrls();
    m_title = session.title;
    m_mediaKind = nextMediaKind;
#ifdef JELLYFIN_NATIVE_WEBOS
    m_statusText
        = needsVideoSurface ? QStringLiteral("Preparing libmpv + Starfish...") : QStringLiteral("Preparing audio...");
#else
    m_statusText = needsVideoSurface ? QStringLiteral("Preparing libmpv...") : QStringLiteral("Preparing audio...");
#endif
    m_errorText.clear();
    const double startSeconds
        = session.startTimeTicks > 0 ? static_cast<double>(session.startTimeTicks) / 10000000.0 : 0.0;
    m_positionTracker.reset(startSeconds);
    m_paused = false;
    m_buffering = false;
    m_bufferingPercent = 0;
    m_seeking = false;
    m_debugOsdVisible = false;
    m_tracks.resetForPlayback();
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

    auto *handle = m_mpvLifecycle.handle();
    m_mpvLifecycle.beginFileLoad();
    const QByteArray urlBytes = session.url.toUtf8();
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
    const char *loadCommand[] = { "loadfile", urlBytes.constData(), "replace", nullptr };
    if (mpv_command(handle, loadCommand) < 0) {
        m_mpvLifecycle.cancelFileLoad();
        m_errorText = QStringLiteral("libmpv rejected the playback URL.");
        stopProgressReporting(true);
        return;
    }
    mpv_command_string(handle, "set pause no");
}

void PlayerController::togglePause()
{
    qInfo() << "player: toggle pause requested";
    mpvCommand({ QByteArrayLiteral("no-osd"), QByteArrayLiteral("cycle"), QByteArrayLiteral("pause") });
}

void PlayerController::prepareForBackground()
{
#ifdef JELLYFIN_NATIVE_WEBOS
    if (!m_sessionActive) {
        m_idleMpvPreparationEnabled = false;
        m_idleMpvPreparationScheduled = false;
        destroyIdleMpv("background");
        return;
    }
    const double position = projectedPositionSeconds();
    qInfo() << "player: playback position snapshot background"
            << "position=" << position;
    setPositionSeconds(position, PlaybackPositionTracker::Source::Lifecycle);
#endif
}

void PlayerController::pauseForBackground()
{
#ifdef JELLYFIN_NATIVE_WEBOS
    prepareForBackground();
    if (!m_sessionActive || m_paused)
        return;

    qInfo() << "player: pausing for background/hidden app state";
    mpvCommand({ QByteArrayLiteral("no-osd"), QByteArrayLiteral("set"), QByteArrayLiteral("pause"),
        QByteArrayLiteral("yes") });
#endif
}

void PlayerController::resyncForForeground()
{
#ifdef JELLYFIN_NATIVE_WEBOS
    if (!m_visible)
        return;

    qInfo() << "player: foreground position resync requested";
    restoreTrustedPosition("foreground");
    requestMpvPositionRefresh("foreground");

    for (int delayMs : { 250, 1000, 2500 }) {
        QTimer::singleShot(delayMs, this, [this]() {
            if (!m_visible)
                return;
            restoreTrustedPosition("foreground-delayed");
            requestMpvPositionRefresh("foreground-delayed");
        });
    }
#endif
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
    emit tracksChanged();
}

void PlayerController::selectAudio(int index)
{
    const std::optional<QByteArrayList> command = m_tracks.audioCommand(index);
    if (!command)
        return;

    if (!mpvCommand(*command))
        return;
    m_tracks.applyAudioSelection(index);
#ifdef JELLYFIN_NATIVE_WEBOS
    qInfo() << "player: webOS audio track changed" << index;
#endif
    emit tracksChanged();
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

    // Drop the UI synchronously so the back button always navigates away
    // immediately, regardless of how long Starfish takes to unload.
    stopProgressReporting(false);

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

void PlayerController::setSubtitlePreferences(const SubtitlePreferences& preferences)
{
    m_subtitlePreferences = preferences;
    qInfo() << "player: subtitle preferences changed"
            << "mode=" << preferences.mode << "language=" << preferences.language << "styling=" << preferences.styling;
    if (auto *handle = m_mpvLifecycle.handle()) {
        applyMpvSubtitleOptions(MpvOptionApplyMode::Runtime, handle);
    } else {
        discardPreparedMpvForOptionChange("subtitle preferences change");
    }
}

void PlayerController::setDemuxerBudget(const QByteArray& maxBytes, const QByteArray& maxBackBytes)
{
    bool changed = false;
    if (!maxBytes.isEmpty() && m_demuxerMaxBytes != maxBytes) {
        m_demuxerMaxBytes = maxBytes;
        changed = true;
    }
    if (!maxBackBytes.isEmpty() && m_demuxerMaxBackBytes != maxBackBytes) {
        m_demuxerMaxBackBytes = maxBackBytes;
        changed = true;
    }
    if (changed)
        discardPreparedMpvForOptionChange("demuxer budget change");
}

void PlayerController::startProgressReporting()
{
    Diagnostics::logEvent(QStringLiteral("player"), QStringLiteral("progress_reporting_start"),
        { { QStringLiteral("itemId"), m_session.itemId } });
    if (m_progressTimer.isActive())
        return;
    m_progressTimer.start();

    m_reporter.start(m_session);
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
    m_reporter.stop(positionTicks, failed);

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
    m_paused = false;
    m_buffering = false;
    m_bufferingPercent = 0;
    m_seeking = false;
    m_positionTracker.clear();
    m_debugOsdVisible = false;
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

bool PlayerController::beginSeekCommand(double targetSeconds, const QByteArray& flags, bool markSeeking)
{
    const double clampedTarget = clampedPosition(targetSeconds);

    if (markSeeking) {
        m_seeking = true;
        m_seekWatchdogTimer.start();
        notifyPlaybackStateChanged();
    }

    m_positionTracker.beginSeek(clampedTarget);
    setPositionSeconds(clampedTarget, PlaybackPositionTracker::Source::Seek);

    const QByteArrayList command = buildSeekCommand(clampedTarget, flags);
    if (mpvCommand(command))
        return true;

    if (markSeeking) {
        m_seeking = false;
        m_seekWatchdogTimer.stop();
        m_positionTracker.cancelSeek();
        notifyPlaybackStateChanged();
    }
    return false;
}

bool PlayerController::beginRelativeSeekCommand(double deltaSeconds)
{
    if (!std::isfinite(deltaSeconds) || deltaSeconds == 0.0)
        return false;

    const double optimisticTarget = clampedPosition(seekAnchorPosition() + deltaSeconds);
    qInfo() << "player: relative keyframe seek" << deltaSeconds << "absoluteTarget=" << optimisticTarget;

    m_seeking = true;
    m_seekWatchdogTimer.start();
    m_positionTracker.beginSeek(optimisticTarget);
    setPositionSeconds(optimisticTarget, PlaybackPositionTracker::Source::Seek);
    notifyPlaybackStateChanged();

    if (mpvCommand(buildSeekCommand(optimisticTarget, QByteArrayLiteral("absolute+keyframes"))))
        return true;

    m_seeking = false;
    m_seekWatchdogTimer.stop();
    m_positionTracker.cancelSeek();
    notifyPlaybackStateChanged();
    return false;
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
            notifyPlaybackStateChanged();
            startProgressReporting();
        });
        break;
    case MPV_EVENT_PLAYBACK_RESTART:
        QMetaObject::invokeMethod(this, [this]() {
            qInfo() << "player: playback restart";
            if (m_seeking) {
                m_seeking = false;
                m_positionTracker.settleSeek();
                m_seekWatchdogTimer.stop();
            }
            m_positionTracker.restartProjection();
            notifyPlaybackStateChanged();
        });
        break;
    case MPV_EVENT_GET_PROPERTY_REPLY: {
        if (event->reply_userdata != kTimePosRefreshReply && event->reply_userdata != kPlaybackTimeRefreshReply)
            break;

        auto *property = static_cast<mpv_event_property *>(event->data);
        if (!property || !property->data || property->format != MPV_FORMAT_DOUBLE)
            break;

        const double seconds = *static_cast<double *>(property->data);
        QMetaObject::invokeMethod(
            this, [this, seconds]() { setPositionSeconds(seconds, PlaybackPositionTracker::Source::Mpv); });
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
                const double positionBeforeStateChange = projectedPositionSeconds();
                m_paused = paused;
                if (m_paused) {
                    setPositionSeconds(positionBeforeStateChange, PlaybackPositionTracker::Source::Projection);
                    m_positionTracker.invalidateProjection();
                } else {
                    restoreTrustedPosition("unpause");
                    requestMpvPositionRefresh("unpause");
                    m_positionTracker.restartProjection();
                }
                notifyPlaybackStateChanged();
            });
        } else if (strcmp(property->name, "paused-for-cache") == 0 && property->format == MPV_FORMAT_FLAG) {
            const bool buffering = *static_cast<int *>(property->data);
            QMetaObject::invokeMethod(this, [this, buffering]() {
                const double positionBeforeStateChange = projectedPositionSeconds();
                m_buffering = buffering;
                if (buffering) {
                    setPositionSeconds(positionBeforeStateChange, PlaybackPositionTracker::Source::Projection);
                    m_positionTracker.invalidateProjection();
                } else {
                    m_bufferingPercent = 0;
                    if (!m_paused) {
                        restoreTrustedPosition("buffering-complete");
                        m_positionTracker.restartProjection();
                    }
                }
                notifyPlaybackStateChanged();
            });
        } else if (strcmp(property->name, "cache-buffering-state") == 0 && property->format == MPV_FORMAT_INT64) {
            const auto percent = static_cast<int>(*static_cast<int64_t *>(property->data));
            QMetaObject::invokeMethod(this, [this, percent]() {
                m_bufferingPercent = percent;
                notifyPlaybackStateChanged();
            });
        } else if (strcmp(property->name, "seeking") == 0 && property->format == MPV_FORMAT_FLAG) {
            const bool seeking = *static_cast<int *>(property->data);
            QMetaObject::invokeMethod(this, [this, seeking]() {
                m_seeking = seeking;
                if (m_seeking)
                    m_seekWatchdogTimer.start();
                else {
                    m_seekWatchdogTimer.stop();
                    m_positionTracker.settleSeek();
                }
                notifyPlaybackStateChanged();
            });
        } else if (strcmp(property->name, "time-pos") == 0 && property->format == MPV_FORMAT_DOUBLE) {
            const double seconds = *static_cast<double *>(property->data);
            QMetaObject::invokeMethod(
                this, [this, seconds]() { setPositionSeconds(seconds, PlaybackPositionTracker::Source::Mpv); });
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
        } else if (strcmp(property->name, "track-list") == 0 && property->format == MPV_FORMAT_NODE) {
            const auto *node = static_cast<mpv_node *>(property->data);
            const ParsedPlaybackTracks tracks = PlaybackTrackParser::parseTracks(node);
            QMetaObject::invokeMethod(this, [this, tracks]() {
                m_tracks.applyParsedTracks(tracks);
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
        // If a new loadfile is already in flight, this END_FILE belongs to
        // the file being replaced — don't tear the UI down.
        if (m_mpvLifecycle.hasPendingFileLoads()) {
            qInfo() << "player: end file for replaced session, ignoring";
            break;
        }
        const bool completed = !failed && endFileReason == MPV_END_FILE_REASON_EOF;
        QMetaObject::invokeMethod(this, [this, failed, completed, endFileReason, endFileError]() {
            qInfo() << "player: end file (main thread) failed=" << failed << "completed=" << completed
                    << "sessionActive=" << m_sessionActive << "reason=" << endFileReason
                    << endFileReasonName(endFileReason) << "error=" << endFileError
                    << (endFileError < 0 ? mpv_error_string(endFileError) : "");
            if (failed)
                m_errorText = QStringLiteral("Playback ended with an mpv error.");
            stopProgressReporting(failed, completed);
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
    return m_positionTracker.seekAnchor(m_paused, m_buffering);
}

void PlayerController::requestMpvPositionRefresh(const char *reason)
{
    auto *handle = m_mpvLifecycle.handle();
    if (!m_sessionActive || !handle)
        return;

    int error = mpv_get_property_async(handle, kTimePosRefreshReply, "time-pos", MPV_FORMAT_DOUBLE);
    if (error < 0)
        qWarning() << "player: async time-pos refresh failed" << (reason ? reason : "unknown")
                   << mpv_error_string(error);

    error = mpv_get_property_async(handle, kPlaybackTimeRefreshReply, "playback-time", MPV_FORMAT_DOUBLE);
    if (error < 0)
        qWarning() << "player: async playback-time refresh failed" << (reason ? reason : "unknown")
                   << mpv_error_string(error);
}

void PlayerController::restoreTrustedPosition(const char *reason)
{
    if (m_positionTracker.restoreTrusted(reason)) {
        if (m_timeline.updatePosition(m_positionTracker.position()))
            emit segmentsChanged();
        emit positionChanged();
    }
}

double PlayerController::projectedPositionSeconds() const
{
    return m_positionTracker.projected(m_paused, m_buffering);
}

void PlayerController::setPositionSeconds(double seconds, PlaybackPositionTracker::Source source, bool notifySegments)
{
    if (!m_positionTracker.update(seconds, source))
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
