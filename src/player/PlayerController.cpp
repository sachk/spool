#include "PlayerController.h"

#include "../api/JellyfinApiFacade.h"
#include "../app/NativeAppWindow.h"
#include "../common/JellyfinTypes.h"
#include "../diagnostics/Diagnostics.h"
#include "MpvVideoItem.h"

#include <QCoroTask>

extern "C" {
#include <mpv/client.h>
}

#include <QCoreApplication>
#include <QByteArray>
#include <QElapsedTimer>
#include <QDebug>
#include <QtGlobal>
#include <QMetaObject>
#include <QPointer>

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>

namespace JellyfinNative {

namespace {

constexpr auto kMpvLogPath = "/tmp/com.codex.jellyfinnative-mpv.log";
constexpr qint64 kSeekTargetFreshnessMs = 10000;
constexpr auto kNightModeFilter =
    "lavfi=[pan=stereo|FL<0.5*FL+1.0*FC+0.25*BL|FR<0.5*FR+1.0*FC+0.25*BR,"
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

void rotateLogFile(const char *path) {
  const QByteArray base(path);
  std::remove((base + ".2").constData());
  std::rename((base + ".1").constData(), (base + ".2").constData());
  std::rename(path, (base + ".1").constData());
}

bool setOption(mpv_handle *handle, const char *name, const char *value) {
  const int error = mpv_set_option_string(handle, name, value);
  if (error >= 0 || error == MPV_ERROR_OPTION_NOT_FOUND)
    return true;
  qWarning() << "player: failed to set mpv option" << name << "=" << value
             << mpv_error_string(error);
  return false;
}

bool setMpvProperty(mpv_handle *handle, const char *name, const char *value) {
  const int error = mpv_set_property_string(handle, name, value);
  return error >= 0 || error == MPV_ERROR_OPTION_NOT_FOUND;
}

bool setMpvDoubleProperty(mpv_handle *handle, const char *name, double value,
                          double *appliedValue = nullptr) {
  const int error = mpv_set_property(handle, name, MPV_FORMAT_DOUBLE, &value);
  if (error < 0 && error != MPV_ERROR_OPTION_NOT_FOUND)
    return false;

  if (appliedValue)
    *appliedValue = value;

  double readback = 0.0;
  const int readError =
      mpv_get_property(handle, name, MPV_FORMAT_DOUBLE, &readback);
  if (readError >= 0 && appliedValue)
    *appliedValue = readback;

  return true;
}

qint64 secondsToTicks(double seconds) {
  return static_cast<qint64>(seconds * 10000000.0);
}

const mpv_node *mapValue(const mpv_node *node, const char *key) {
  if (!node || node->format != MPV_FORMAT_NODE_MAP || !node->u.list)
    return nullptr;
  const mpv_node_list *list = node->u.list;
  for (int i = 0; i < list->num; ++i) {
    if (list->keys[i] && strcmp(list->keys[i], key) == 0)
      return &list->values[i];
  }
  return nullptr;
}

QString nodeString(const mpv_node *node) {
  if (!node)
    return {};
  if (node->format == MPV_FORMAT_STRING && node->u.string)
    return QString::fromUtf8(node->u.string);
  if (node->format == MPV_FORMAT_INT64)
    return QString::number(node->u.int64);
  if (node->format == MPV_FORMAT_FLAG)
    return node->u.flag ? QStringLiteral("yes") : QStringLiteral("no");
  return {};
}

int64_t nodeInt(const mpv_node *node, int64_t fallback = 0) {
  if (!node)
    return fallback;
  if (node->format == MPV_FORMAT_INT64)
    return node->u.int64;
  if (node->format == MPV_FORMAT_STRING && node->u.string)
    return QByteArray(node->u.string).toLongLong();
  return fallback;
}

bool nodeFlag(const mpv_node *node) {
  if (!node)
    return false;
  if (node->format == MPV_FORMAT_FLAG)
    return node->u.flag != 0;
  if (node->format == MPV_FORMAT_STRING && node->u.string)
    return strcmp(node->u.string, "yes") == 0 || strcmp(node->u.string, "true") == 0;
  return false;
}

QString prettyLanguage(QString lang) {
  lang = lang.trimmed().toLower();
  if (lang == QStringLiteral("eng") || lang == QStringLiteral("en"))
    return QStringLiteral("English");
  if (lang == QStringLiteral("jpn") || lang == QStringLiteral("ja"))
    return QStringLiteral("Japanese");
  if (lang == QStringLiteral("spa") || lang == QStringLiteral("es"))
    return QStringLiteral("Spanish");
  if (lang == QStringLiteral("fre") || lang == QStringLiteral("fra") || lang == QStringLiteral("fr"))
    return QStringLiteral("French");
  if (lang == QStringLiteral("ger") || lang == QStringLiteral("deu") || lang == QStringLiteral("de"))
    return QStringLiteral("German");
  if (lang == QStringLiteral("ita") || lang == QStringLiteral("it"))
    return QStringLiteral("Italian");
  if (lang == QStringLiteral("por") || lang == QStringLiteral("pt"))
    return QStringLiteral("Portuguese");
  if (lang == QStringLiteral("dut") || lang == QStringLiteral("nld") || lang == QStringLiteral("nl"))
    return QStringLiteral("Dutch");
  if (lang == QStringLiteral("und") || lang.isEmpty())
    return {};
  return lang.toUpper();
}

QString prettySubtitleCodec(QString codec) {
  codec = codec.trimmed().toLower();
  if (codec == QStringLiteral("subrip") || codec == QStringLiteral("srt"))
    return QStringLiteral("SRT");
  if (codec == QStringLiteral("ass") || codec.contains(QStringLiteral("ass")))
    return QStringLiteral("ASS");
  if (codec == QStringLiteral("ssa"))
    return QStringLiteral("SSA");
  if (codec.contains(QStringLiteral("pgs")) || codec.contains(QStringLiteral("hdmv")))
    return QStringLiteral("PGS");
  if (codec.contains(QStringLiteral("dvd")) || codec.contains(QStringLiteral("vobsub")))
    return QStringLiteral("DVD");
  if (codec.contains(QStringLiteral("webvtt")) || codec == QStringLiteral("vtt"))
    return QStringLiteral("VTT");
  if (codec.isEmpty())
    return {};
  return codec.toUpper();
}

QString cleanSubtitleTitle(QString title) {
  title = title.trimmed();
  while (true) {
    const int open = title.lastIndexOf(QLatin1Char('['));
    const int close = title.endsWith(QLatin1Char(']')) ? title.size() - 1 : -1;
    if (open < 0 || close < 0 || open >= close)
      break;
    title = title.left(open).trimmed();
  }
  return title;
}

} // namespace

PlayerController::PlayerController(NativeAppWindow *window,
                                   JellyfinApiFacade *api, QObject *parent)
    : QObject(parent), m_window(window), m_api(api) {
  m_progressTimer.setInterval(5000);
  m_uiPositionTimer.setInterval(250);
  m_backGuardTimer.setSingleShot(true);
  m_backGuardTimer.setInterval(1500);
  m_seekWatchdogTimer.setSingleShot(true);
  m_seekWatchdogTimer.setInterval(2500);
  connect(&m_backGuardTimer, &QTimer::timeout, this, [this]() {
    if (!m_visible || m_backAllowed)
      return;
    m_backAllowed = true;
    qInfo() << "player: startup back guard released";
    emit stateChanged();
  });
  connect(&m_uiPositionTimer, &QTimer::timeout, this, [this]() {
    if (!m_visible || m_paused || m_buffering || m_seeking ||
        !m_positionClock.isValid())
      return;

    const double elapsed = m_positionClock.elapsed() / 1000.0;
    if (elapsed <= 0.0)
      return;

    setPositionSeconds(m_positionSeconds + elapsed);
  });
  connect(&m_seekWatchdogTimer, &QTimer::timeout, this, [this]() {
    if (!m_visible || !m_seeking)
      return;

    qWarning() << "player: clearing stale seek state";
    m_seeking = false;
    m_requestedSeekTargetSeconds = -1.0;
    m_seekCommandClock.invalidate();
    m_positionClock.restart();
    updatePlaybackStatusText();
    emit stateChanged();
  });
  connect(&m_progressTimer, &QTimer::timeout, this, [this]() {
    if (!m_visible)
      return;

    const auto session = m_session;
    QCoro::runDetached(
        m_api->reportPlaybackProgress(
            session, secondsToTicks(m_positionSeconds), m_paused),
        []() {}, [](const std::exception_ptr &) {});
  });
}

PlayerController::~PlayerController() {
  teardownMpv();
}

void PlayerController::teardownMpv() {
  Diagnostics::Phase phase(QStringLiteral("shutdown"), QStringLiteral("player_teardown_mpv"));
  mpv_handle *handle = m_mpv.exchange(nullptr);
  if (!handle) {
    if (m_eventThread.joinable())
      m_eventThread.join();
    return;
  }

#ifndef JELLYFIN_NATIVE_WEBOS
  // Free the render context first; this is thread-safe and decouples us from
  // the scene-graph render thread (which may already be shutting down).
  if (auto *videoItem = MpvVideoItem::instance())
    videoItem->setMpvHandle(nullptr);
#endif

  // Force the event loop thread to exit promptly — we don't need to wait for
  // mpv to deliver MPV_EVENT_SHUTDOWN, mpv_terminate_destroy below handles
  // the full shutdown synchronously and joins all internal mpv threads.
  m_terminating = true;
  if (m_eventThread.joinable())
    m_eventThread.join();

  // mpv_terminate_destroy stops decoding/audio output and joins mpv's
  // internal threads before returning. Audio cuts off promptly (vs.
  // mpv_destroy + "quit" which can leave audio playing while pipewire drains
  // its buffers).
  qInfo() << "player: calling mpv_terminate_destroy";
  mpv_terminate_destroy(handle);
  qInfo() << "player: mpv_terminate_destroy returned";

  m_terminating = false;
  m_pendingFileLoads = 0;
}

void PlayerController::scheduleMpvTeardown() {
  auto *scheduledHandle = m_mpv.load();
  if (!scheduledHandle)
    return;

  QTimer::singleShot(1000, this, [this, scheduledHandle]() {
    if (m_mpv.load() != scheduledHandle)
      return;
    qInfo() << "player: deferred mpv teardown";
    teardownMpv();
  });
}

bool PlayerController::visible() const { return m_visible; }

bool PlayerController::paused() const { return m_paused; }

QString PlayerController::title() const { return m_title; }

QString PlayerController::statusText() const { return m_statusText; }

QString PlayerController::errorText() const { return m_errorText; }

bool PlayerController::buffering() const { return m_buffering; }

int PlayerController::bufferingPercent() const { return m_bufferingPercent; }

bool PlayerController::seeking() const { return m_seeking; }

bool PlayerController::debugOsdVisible() const { return m_debugOsdVisible; }

bool PlayerController::subtitlesEnabled() const { return m_subtitlesEnabled; }

QStringList PlayerController::subtitleTracks() const { return m_subtitleTracks; }

int PlayerController::selectedSubtitleIndex() const { return m_selectedSubtitleIndex; }

QStringList PlayerController::audioTracks() const { return m_audioTracks; }

int PlayerController::selectedAudioIndex() const { return m_selectedAudioIndex; }

bool PlayerController::backAllowed() const { return m_backAllowed; }

double PlayerController::positionSeconds() const { return m_positionSeconds; }

double PlayerController::durationSeconds() const { return m_durationSeconds; }

bool PlayerController::nightModeEnabled() const { return m_nightModeEnabled.load(); }

int PlayerController::audioDelayMs() const { return m_audioDelayMs.load(); }

QString PlayerController::audioOutputMode() const { return m_audioOutputMode; }

bool PlayerController::applyMpvRuntimeOption(MpvRuntimeOption option,
                                             MpvOptionApplyMode mode,
                                             mpv_handle *handle) {
  if (!handle)
    return false;

  const char *name = nullptr;
  QByteArray value;
  double doubleValue = 0.0;
  switch (option) {
  case MpvRuntimeOption::NightMode:
    name = "af";
    value = m_nightModeEnabled.load() ? QByteArray(kNightModeFilter)
                                      : QByteArray();
    break;
  case MpvRuntimeOption::AudioDelay:
    name = "audio-delay";
    doubleValue = static_cast<double>(m_audioDelayMs.load()) / 1000.0;
    value = QByteArray::number(doubleValue, 'f', 3);
    break;
  }

  double appliedDoubleValue = doubleValue;
  const bool ok = mode == MpvOptionApplyMode::Initial
                      ? setOption(handle, name, value.constData())
                      : option == MpvRuntimeOption::AudioDelay
                            ? setMpvDoubleProperty(handle, name, doubleValue,
                                                   &appliedDoubleValue)
                            : setMpvProperty(handle, name, value.constData());
  if (!ok) {
    qWarning() << "player: failed to apply mpv runtime option" << name
               << "mode=" << (mode == MpvOptionApplyMode::Initial ? "initial" : "runtime");
  } else if (option == MpvRuntimeOption::AudioDelay) {
    qInfo() << "player: applied audio delay"
            << "mode=" << (mode == MpvOptionApplyMode::Initial ? "initial" : "runtime")
            << "requestedMs=" << m_audioDelayMs.load()
            << "appliedSeconds=" << appliedDoubleValue;
  }
  return ok;
}

bool PlayerController::applyMpvRuntimeOptions(MpvOptionApplyMode mode,
                                              mpv_handle *handle) {
  return applyMpvRuntimeOption(MpvRuntimeOption::NightMode, mode, handle) &&
         applyMpvRuntimeOption(MpvRuntimeOption::AudioDelay, mode, handle);
}

bool PlayerController::ensureMpv() {
  if (m_mpv.load())
    return true;

#ifdef JELLYFIN_NATIVE_WEBOS
  const bool useStarfishAudio = m_audioOutputMode == QStringLiteral("starfish");
  qputenv("STARFISH_AUDIO_HINT", useStarfishAudio ? QByteArrayLiteral("1") : QByteArrayLiteral("0"));
  qInfo() << "player: configuring webOS audio output"
          << (useStarfishAudio ? "starfish" : "alsa")
          << "starfishAudioHint=" << qgetenv("STARFISH_AUDIO_HINT");
#endif

  QElapsedTimer startupTimer;
  startupTimer.start();
  rotateLogFile(kMpvLogPath);
  mpv_handle *handle = mpv_create();
  if (!handle) {
    m_errorText = QStringLiteral("mpv_create failed.");
    emit stateChanged();
    return false;
  }

  const bool configured =
      setOption(handle, "config", "no") &&
      setOption(handle, "terminal", "no") &&
#ifdef JELLYFIN_NATIVE_WEBOS
      setOption(handle, "msg-level", "all=warn,starfish=info,sub=v") &&
#else
      setOption(handle, "msg-level", "all=warn,sub=v") &&
#endif
      setOption(handle, "log-file", kMpvLogPath) &&
      setOption(handle, "ytdl", "no") &&
      setOption(handle, "demuxer-lavf-analyzeduration", "1") &&
      setOption(handle, "demuxer-lavf-probesize", "1048576") &&
      setOption(handle, "cache", "yes") &&
      setOption(handle, "cache-pause", "no") &&
      setOption(handle, "demuxer-max-bytes", "64M") &&
      setOption(handle, "demuxer-max-back-bytes", "32M") &&
      setOption(handle, "initial-audio-sync", "no") &&
      applyMpvRuntimeOptions(MpvOptionApplyMode::Initial, handle) &&
#ifdef JELLYFIN_NATIVE_WEBOS
      setOption(handle, "force-window", "no") &&
      setOption(handle, "vo", "starfish") &&
      setOption(handle, "vd", "starfish") &&
      setOption(handle, "ao", useStarfishAudio ? "starfish,null" : "alsa,null") &&
      (useStarfishAudio || setOption(handle, "audio-device", "alsa/hw:0,7")) &&
      // Starfish VO owns frame presentation timing, so mpv cannot adjust video
      // scheduling directly. With ALSA audio, resample audio to follow the
      // display/video clock instead.
      (useStarfishAudio || setOption(handle, "video-sync", "display-resample")) &&
      setOption(handle, "audio-channels", "stereo") &&
      setOption(handle, "audio-format", "s32") &&
      setOption(handle, "audio-samplerate", "192000") &&

#else
      // Render via libmpv's render API into the embedded MpvVideoItem; no
      // separate mpv toplevel window.
      setOption(handle, "force-window", "no") &&
      setOption(handle, "vo", "libmpv") &&
      setOption(handle, "hwdec", "auto-safe") &&
      setOption(handle, "ao", "pipewire,pulse,alsa") &&
#endif
      setOption(handle, "osd-bar", "no") &&
      setOption(handle, "osd-duration", "0") &&
      setOption(handle, "sid", m_subtitlesEnabled ? "auto" : "no") &&
      setOption(handle, "sub-auto", "all") &&
      setOption(handle, "sub-visibility", "yes") &&
      setOption(handle, "sub-ass", "yes") &&
      setOption(handle, "sub-ass-override", "force") &&
      setOption(handle, "sub-use-margins", "yes") &&
      setOption(handle, "sub-font-size", "55") &&
      setOption(handle, "sub-margin-y", "40") &&
      setOption(handle, "sub-color", "#FFFFFFFF") &&
      setOption(handle, "sub-border-size", "3.5") &&
      setOption(handle, "sub-border-color", "#FF000000") &&
      setOption(handle, "sub-shadow-offset", "1") &&
      setOption(handle, "sub-shadow-color", "#80000000") &&
      setOption(handle, "audio-file-auto", "no") &&
      setOption(handle, "osc", "no") &&
      setOption(handle, "load-console", "no") &&
      setOption(handle, "load-auto-profiles", "no") &&
      setOption(handle, "load-select", "no") &&
      setOption(handle, "load-positioning", "no") &&
      setOption(handle, "load-commands", "no") &&
      setOption(handle, "load-context-menu", "no") &&
      setOption(handle, "load-scripts", "no") &&
      setOption(handle, "input-default-bindings", "no") &&
      setOption(handle, "input-vo-keyboard", "no") &&
      setOption(handle, "keep-open", "no") && setOption(handle, "idle", "yes");

  if (!configured || mpv_initialize(handle) < 0) {
    mpv_terminate_destroy(handle);
    m_errorText = QStringLiteral("Failed to initialize libmpv.");
    emit stateChanged();
    return false;
  }

  mpv_observe_property(handle, 0, "pause", MPV_FORMAT_FLAG);
  mpv_observe_property(handle, 0, "paused-for-cache", MPV_FORMAT_FLAG);
  mpv_observe_property(handle, 0, "cache-buffering-state", MPV_FORMAT_INT64);
  mpv_observe_property(handle, 0, "seeking", MPV_FORMAT_FLAG);
  mpv_observe_property(handle, 0, "time-pos", MPV_FORMAT_DOUBLE);
  mpv_observe_property(handle, 0, "duration", MPV_FORMAT_DOUBLE);
  mpv_observe_property(handle, 0, "track-list", MPV_FORMAT_NODE);

  m_mpv = handle;
  qInfo() << "player: mpv initialized in" << startupTimer.elapsed() << "ms";

#ifndef JELLYFIN_NATIVE_WEBOS
  // vo=libmpv requires the embedded MpvVideoItem to host the render context.
  // Fail loudly if QML hasn't constructed one yet — silently falling back
  // would leave us with no video at all.
  auto *videoItem = MpvVideoItem::instance();
  if (!videoItem) {
    qFatal("PlayerController: MpvVideoItem instance is missing; "
           "PlayerOverlayPage must instantiate it before play() is called.");
    mpv_terminate_destroy(handle);
    m_mpv = nullptr;
    return false;
  }
  videoItem->setMpvHandle(handle);
#endif

  m_eventThread = std::thread([this]() { runEventLoop(); });
  return true;
}

void PlayerController::play(const PlaybackSession &session) {
  Diagnostics::Task task(QStringLiteral("player_play"), {{QStringLiteral("itemId"), session.itemId}, {QStringLiteral("title"), session.title}});
  qInfo() << "player: play requested" << session.title
          << "startTimeTicks=" << session.startTimeTicks;

  if (m_mpv.load()) {
    qInfo() << "player: tearing down stale mpv before play";
    teardownMpv();
  }

  m_window->clearOverlay();
  QElapsedTimer playbackSurfaceTimer;
  playbackSurfaceTimer.start();
  if (!m_window->prepareForPlaybackSurface()) {
    m_errorText =
        QStringLiteral("Failed to prepare the native playback surface.");
    qWarning() << "player: prepareForPlaybackSurface failed after"
               << playbackSurfaceTimer.elapsed() << "ms";
    emit stateChanged();
    return;
  }
  qInfo() << "player: prepareForPlaybackSurface completed in"
          << playbackSurfaceTimer.elapsed() << "ms";

  // Surface must be ready before loadfile creates the Starfish VO.
  if (!ensureMpv())
    return;

  m_session = session;
  m_title = session.title;
#ifdef JELLYFIN_NATIVE_WEBOS
  m_statusText = QStringLiteral("Preparing libmpv + Starfish...");
#else
  m_statusText = QStringLiteral("Preparing libmpv...");
#endif
  m_errorText.clear();
  const double startSeconds =
      session.startTimeTicks > 0
          ? static_cast<double>(session.startTimeTicks) / 10000000.0
          : 0.0;
  m_resumeStartSeconds = startSeconds;
  m_positionSeconds = startSeconds;
  m_durationSeconds = 0.0;
  m_positionClock.invalidate();
  m_paused = false;
  m_buffering = false;
  m_bufferingPercent = 0;
  m_seeking = false;
  m_debugOsdVisible = false;
  m_subtitleTracks = { QStringLiteral("Off") };
  m_subtitleIds = { -1 };
  m_selectedSubtitleIndex = 0;
  m_audioTracks.clear();
  m_audioIds.clear();
  m_selectedAudioIndex = -1;
  m_backAllowed = false;
  m_seekCommandClock.invalidate();
  m_requestedSeekTargetSeconds = -1.0;
  m_backGuardTimer.start();
  m_uiPositionTimer.start();
  m_visible = true;
  emit visibleChanged();
  emit stateChanged();

  auto *handle = m_mpv.load();
  m_pendingFileLoads.fetch_add(1, std::memory_order_acq_rel);
  const QByteArray urlBytes = session.url.toUtf8();
  if (startSeconds > 0.0) {
    const QByteArray startValue = QByteArray::number(startSeconds, 'f', 3);
    if (!setOption(handle, "start", startValue.constData())) {
      m_pendingFileLoads.fetch_sub(1, std::memory_order_acq_rel);
      m_errorText = QStringLiteral("libmpv rejected the resume position.");
      stopProgressReporting(true);
      return;
    }
    qInfo() << "player: instructing mpv to start at resume position seconds="
            << startSeconds;
  }
  const char *loadCommand[] = {"loadfile", urlBytes.constData(), "replace",
                               nullptr};
  if (mpv_command(handle, loadCommand) < 0) {
    m_pendingFileLoads.fetch_sub(1, std::memory_order_acq_rel);
    m_errorText = QStringLiteral("libmpv rejected the playback URL.");
    stopProgressReporting(true);
    return;
  }
  mpv_command_string(handle, "set pause no");
}

void PlayerController::togglePause() { mpvCommand("no-osd cycle pause"); }

void PlayerController::pauseForBackground() {
#ifdef JELLYFIN_NATIVE_WEBOS
  if (!m_visible || m_paused)
    return;

  qInfo() << "player: pausing for background/hidden app state";
  mpvCommand("no-osd set pause yes");
#endif
}

void PlayerController::seekBack() {
  beginRelativeSeekCommand(-10.0);
}

void PlayerController::seekForward() {
  beginRelativeSeekCommand(10.0);
}

void PlayerController::seek(double seconds) {
  if (!std::isfinite(seconds))
    return;

  const double clampedSeconds =
      m_durationSeconds > 0.0 ? qBound(0.0, seconds, m_durationSeconds)
                              : qMax(0.0, seconds);
  // Use absolute+exact so a committed click lands on the requested frame.
  qInfo() << "player: absolute exact seek" << clampedSeconds;
  beginSeekCommand(clampedSeconds, QByteArray("absolute+exact"));
}

void PlayerController::previewSeekBy(double deltaSeconds) {
  beginRelativeSeekCommand(deltaSeconds);
}

void PlayerController::toggleDebugOsd() {
  if (!mpvCommand("script-binding stats/display-stats-toggle"))
    return;
  m_debugOsdVisible = !m_debugOsdVisible;
  emit stateChanged();
}

void PlayerController::toggleSubtitles() {
  if (m_selectedSubtitleIndex > 0) {
    selectSubtitle(0);
    return;
  }
  selectSubtitle(m_subtitleTracks.size() > 1 ? 1 : 0);
}

void PlayerController::cycleSubtitles() {
  // Off (index 0) -> first track -> second -> ... -> last -> Off.
  if (m_subtitleTracks.size() <= 1)
    return;
  const int next = (m_selectedSubtitleIndex + 1) % m_subtitleTracks.size();
  selectSubtitle(next);
}

void PlayerController::enableSubtitles() {
  if (m_selectedSubtitleIndex > 0)
    return;
  if (m_subtitleTracks.size() > 1)
    selectSubtitle(1);
}

void PlayerController::cycleAudio() {
  if (m_audioTracks.size() <= 1)
    return;
  const int next = (m_selectedAudioIndex + 1) % m_audioTracks.size();
  selectAudio(next);
}

void PlayerController::selectSubtitle(int index) {
  if (index < 0 || index >= m_subtitleIds.size())
    return;

  const int trackId = m_subtitleIds[index];
  const QByteArray command = QByteArray("no-osd set sid ") +
                             (trackId < 0 ? QByteArray("no")
                                           : QByteArray::number(trackId));
  mpvCommand(command.constData());
  m_selectedSubtitleIndex = index;
  m_subtitlesEnabled = trackId >= 0;
  if (!m_subtitlesEnabled)
    m_window->clearOverlay();
  emit stateChanged();
}

void PlayerController::selectAudio(int index) {
  if (index < 0 || index >= m_audioIds.size())
    return;

  const int trackId = m_audioIds[index];
  const QByteArray command = QByteArray("no-osd set aid ") +
                             (trackId < 0 ? QByteArray("no")
                                           : QByteArray::number(trackId));
  if (!mpvCommand(command.constData()))
    return;
  m_selectedAudioIndex = index;
#ifdef JELLYFIN_NATIVE_WEBOS
  const double targetSeconds = clampedPosition(seekAnchorPosition());
  qInfo() << "player: webOS audio track resync seek" << targetSeconds;
  beginSeekCommand(targetSeconds, QByteArray("absolute+keyframes"));
#endif
  emit stateChanged();
}

void PlayerController::stop() {
  stopWithReason(QStringLiteral("unspecified"));
}

void PlayerController::stopWithReason(const QString &reason) {
  Diagnostics::Task task(QStringLiteral("player_stop"), {{QStringLiteral("reason"), reason}, {QStringLiteral("visible"), m_visible}});
  qInfo() << "player: stop requested" << reason << "visible" << m_visible;
  if (!m_visible)
    return;

  // Drop the UI synchronously so the back button always navigates away
  // immediately, regardless of how long Starfish takes to unload.
  stopProgressReporting(false);

  if (auto *handle = m_mpv.load())
    mpv_command_string(handle, "stop");
  scheduleMpvTeardown();
}

void PlayerController::setNightModeEnabled(bool enabled) {
  if (m_nightModeEnabled.load() == enabled)
    return;

  m_nightModeEnabled = enabled;
  if (auto *handle = m_mpv.load()) {
    applyMpvRuntimeOption(MpvRuntimeOption::NightMode,
                          MpvOptionApplyMode::Runtime, handle);
  }

  emit nightModeEnabledChanged();
  emit stateChanged();
}

void PlayerController::setAudioDelayMs(int delayMs) {
  const int clampedDelayMs = qBound(-2000, delayMs, 2000);
  if (m_audioDelayMs.load() == clampedDelayMs) {
    qInfo() << "player: audio delay unchanged" << clampedDelayMs << "ms";
    return;
  }

  m_audioDelayMs = clampedDelayMs;
  qInfo() << "player: audio delay requested" << clampedDelayMs << "ms"
          << "visible=" << m_visible;
  if (auto *handle = m_mpv.load()) {
    applyMpvRuntimeOption(MpvRuntimeOption::AudioDelay,
                          MpvOptionApplyMode::Runtime, handle);
  } else {
    qInfo() << "player: audio delay stored without active mpv";
  }

  emit audioDelayMsChanged();
  emit stateChanged();
}

void PlayerController::setAudioOutputMode(const QString &mode) {
  const QString normalized = mode == QStringLiteral("starfish") ? QStringLiteral("starfish") : QStringLiteral("alsa");
  if (m_audioOutputMode == normalized)
    return;

  m_audioOutputMode = normalized;
  qInfo() << "player: audio output mode changed" << normalized
          << "visible=" << m_visible;
  emit audioOutputModeChanged();
  emit stateChanged();
}

void PlayerController::startProgressReporting() {
  Diagnostics::logEvent(QStringLiteral("player"), QStringLiteral("progress_reporting_start"), {{QStringLiteral("itemId"), m_session.itemId}});
  if (m_progressTimer.isActive())
    return;
  m_progressTimer.start();

  const auto session = m_session;
  QCoro::runDetached(
      m_api->reportPlaybackStart(session), []() {},
      [](const std::exception_ptr &) {});
}

void PlayerController::stopProgressReporting(bool failed) {
  Diagnostics::Phase phase(QStringLiteral("player"), QStringLiteral("stop_progress_reporting"), {{QStringLiteral("failed"), failed}});
  if (!m_visible && !m_progressTimer.isActive()) {
    qInfo() << "player: stopProgressReporting skipped visible=" << m_visible;
    return;
  }

  qInfo() << "player: stopProgressReporting visible=" << m_visible << "failed=" << failed;
  m_progressTimer.stop();
  m_uiPositionTimer.stop();
  m_seekWatchdogTimer.stop();

  const auto session = m_session;
  const qint64 positionTicks = secondsToTicks(m_positionSeconds);
  QCoro::runDetached(
      m_api->reportPlaybackStopped(session, positionTicks, failed), []() {},
      [](const std::exception_ptr &) {});

  resetPlaybackUiState();
  m_window->clearOverlay();
  emit stateChanged();
  emit visibleChanged();
  emit playbackStopped(session.itemId, positionTicks);
}

void PlayerController::resetPlaybackUiState() {
  m_visible = false;
  m_paused = false;
  m_buffering = false;
  m_bufferingPercent = 0;
  m_seeking = false;
  m_requestedSeekTargetSeconds = -1.0;
  m_seekCommandClock.invalidate();
  m_resumeStartSeconds = 0.0;
  m_positionClock.invalidate();
  m_debugOsdVisible = false;
  m_statusText = QStringLiteral("Ready");
}

bool PlayerController::mpvCommand(const char *command) {
  auto *handle = m_mpv.load();
  if (!handle) {
    qInfo() << "player: mpv command dropped (no handle):" << command;
    return false;
  }
  // Submit asynchronously: the synchronous mpv_command_string takes mpv's
  // dispatch lock and waits for the core thread, but with vo=libmpv the core
  // can in turn need the render thread, which dispatches an update back to
  // this (GUI) thread via QMetaObject::invokeMethod(... Qt::QueuedConnection).
  // If that update lands on us while we are blocking inside mpv, the chain
  // deadlocks (observed when toggling stats overlay, which queries vo_passes).
  // Reply events are silently dropped by the event loop (no userdata).
  //
  // mpv_command_node_async refuses MPV_FORMAT_STRING nodes (INVALID_PARAMETER),
  // so split the command into a NULL-terminated argv on whitespace. None of
  // our callers embed quoted whitespace in arguments.
  QByteArray buffer(command);
  QList<char *> argv;
  argv.reserve(8);
  char *cursor = buffer.data();
  while (*cursor) {
    while (*cursor == ' ' || *cursor == '\t')
      ++cursor;
    if (!*cursor)
      break;
    argv.append(cursor);
    while (*cursor && *cursor != ' ' && *cursor != '\t')
      ++cursor;
    if (*cursor) {
      *cursor = '\0';
      ++cursor;
    }
  }
  argv.append(nullptr);
  const int error = mpv_command_async(handle, 0, const_cast<const char **>(argv.data()));
  if (error < 0) {
    qWarning() << "player: mpv_command_async failed" << command
               << "error=" << error << mpv_error_string(error);
    return false;
  }
  return true;
}

QByteArray PlayerController::buildSeekCommand(double targetSeconds,
                                              const QByteArray &flags) const {
  return QByteArray("no-osd seek ") + QByteArray::number(targetSeconds, 'f', 3) +
         QByteArray(" ") + flags;
}

bool PlayerController::beginSeekCommand(double targetSeconds,
                                        const QByteArray &flags,
                                        bool markSeeking) {
  const double clampedTarget = clampedPosition(targetSeconds);

  if (markSeeking) {
    m_seeking = true;
    m_requestedSeekTargetSeconds = clampedTarget;
    m_seekWatchdogTimer.start();
    updatePlaybackStatusText();
    emit stateChanged();
  }

  setPositionSeconds(clampedTarget);
  m_seekCommandClock.restart();

  const QByteArray command = buildSeekCommand(clampedTarget, flags);
  if (mpvCommand(command.constData()))
    return true;

  if (markSeeking) {
    m_seeking = false;
    m_requestedSeekTargetSeconds = -1.0;
    m_seekWatchdogTimer.stop();
    m_seekCommandClock.invalidate();
    updatePlaybackStatusText();
    emit stateChanged();
  }
  return false;
}

bool PlayerController::beginRelativeSeekCommand(double deltaSeconds) {
  if (!std::isfinite(deltaSeconds) || deltaSeconds == 0.0)
    return false;

  const double optimisticTarget = clampedPosition(seekAnchorPosition() + deltaSeconds);
  qInfo() << "player: relative keyframe seek" << deltaSeconds
          << "absoluteTarget=" << optimisticTarget;

  m_seeking = true;
  m_requestedSeekTargetSeconds = optimisticTarget;
  m_seekWatchdogTimer.start();
  setPositionSeconds(optimisticTarget);
  m_seekCommandClock.restart();
  updatePlaybackStatusText();
  emit stateChanged();

  const QByteArray command = QByteArray("no-osd seek ") +
                             QByteArray::number(deltaSeconds, 'f', 3) +
                             QByteArray(" relative+keyframes");
  if (mpvCommand(command.constData()))
    return true;

  m_seeking = false;
  m_requestedSeekTargetSeconds = -1.0;
  m_seekWatchdogTimer.stop();
  m_seekCommandClock.invalidate();
  updatePlaybackStatusText();
  emit stateChanged();
  return false;
}

void PlayerController::runEventLoop() {
  Diagnostics::ThreadScope threadScope(QStringLiteral("mpv-event"));
  auto *handle = m_mpv.load();
  if (!handle)
    return;

  while (!m_terminating.load()) {
    mpv_event *event = mpv_wait_event(handle, 0.1);
    if (!event)
      continue;

    switch (event->event_id) {
    case MPV_EVENT_FILE_LOADED:
      m_pendingFileLoads.fetch_sub(1, std::memory_order_acq_rel);
      QMetaObject::invokeMethod(this, [this]() {
        qInfo() << "player: file loaded";
        updatePlaybackStatusText();
        emit stateChanged();
        startProgressReporting();
      });
      break;
    case MPV_EVENT_PLAYBACK_RESTART:
      QMetaObject::invokeMethod(this, [this]() {
        qInfo() << "player: playback restart";
        if (m_seeking) {
          m_seeking = false;
          m_requestedSeekTargetSeconds = -1.0;
          m_seekWatchdogTimer.stop();
        }
        m_positionClock.restart();
        updatePlaybackStatusText();
        emit stateChanged();
      });
      break;
    case MPV_EVENT_PROPERTY_CHANGE: {
      auto *property = static_cast<mpv_event_property *>(event->data);
      if (!property || !property->data)
        break;

      if (strcmp(property->name, "pause") == 0 &&
          property->format == MPV_FORMAT_FLAG) {
        const bool paused = *static_cast<int *>(property->data);
        QMetaObject::invokeMethod(this, [this, paused]() {
          m_paused = paused;
          if (!m_paused)
            m_positionClock.restart();
          updatePlaybackStatusText();
          emit stateChanged();
        });
      } else if (strcmp(property->name, "paused-for-cache") == 0 &&
                 property->format == MPV_FORMAT_FLAG) {
        const bool buffering = *static_cast<int *>(property->data);
        QMetaObject::invokeMethod(this, [this, buffering]() {
          m_buffering = buffering;
          if (!buffering)
            m_bufferingPercent = 0;
          updatePlaybackStatusText();
          emit stateChanged();
        });
      } else if (strcmp(property->name, "cache-buffering-state") == 0 &&
                 property->format == MPV_FORMAT_INT64) {
        const auto percent = static_cast<int>(*static_cast<int64_t *>(property->data));
        QMetaObject::invokeMethod(this, [this, percent]() {
          m_bufferingPercent = percent;
          updatePlaybackStatusText();
          emit stateChanged();
        });
      } else if (strcmp(property->name, "seeking") == 0 &&
                 property->format == MPV_FORMAT_FLAG) {
        const bool seeking = *static_cast<int *>(property->data);
        QMetaObject::invokeMethod(this, [this, seeking]() {
          m_seeking = seeking;
          if (m_seeking)
            m_seekWatchdogTimer.start();
          else {
            m_seekWatchdogTimer.stop();
            m_requestedSeekTargetSeconds = -1.0;
            m_positionClock.restart();
          }
          updatePlaybackStatusText();
          emit stateChanged();
        });
      } else if (strcmp(property->name, "time-pos") == 0 &&
                 property->format == MPV_FORMAT_DOUBLE) {
        const double seconds = *static_cast<double *>(property->data);
        QMetaObject::invokeMethod(this, [this, seconds]() {
          if (m_seeking ||
              (m_seekCommandClock.isValid() && m_seekCommandClock.elapsed() < 1500))
            return;
          const double position = playbackPositionFromMpvTime(seconds);
          if (position + 2.0 < projectedPositionSeconds())
            return;
          setPositionSeconds(position);
        });
      } else if (strcmp(property->name, "duration") == 0 &&
                 property->format == MPV_FORMAT_DOUBLE) {
        const double seconds = *static_cast<double *>(property->data);
        QMetaObject::invokeMethod(this, [this, seconds]() {
          m_durationSeconds = seconds;
          setPositionSeconds(m_positionSeconds);
          emit stateChanged();
        });
      } else if (strcmp(property->name, "track-list") == 0 &&
                 property->format == MPV_FORMAT_NODE) {
        const auto *node = static_cast<mpv_node *>(property->data);
        QStringList labels{QStringLiteral("Off")};
        QList<int> ids{-1};
        int selected = 0;
        QStringList audioLabels;
        QList<int> audioIds;
        int audioSelected = -1;

        if (node && node->format == MPV_FORMAT_NODE_ARRAY && node->u.list) {
          const mpv_node_list *tracks = node->u.list;
          for (int i = 0; i < tracks->num; ++i) {
            const mpv_node *track = &tracks->values[i];
            const QString type = nodeString(mapValue(track, "type"));
            const int id = static_cast<int>(nodeInt(mapValue(track, "id"), -1));
            if (id < 0)
              continue;

            if (type == QStringLiteral("sub")) {
              const QString language = prettyLanguage(nodeString(mapValue(track, "lang")));
              const QString title = cleanSubtitleTitle(nodeString(mapValue(track, "title")));
              const QString codec = prettySubtitleCodec(nodeString(mapValue(track, "codec")));
              QString label = language.isEmpty() ? QStringLiteral("Subtitle %1").arg(id) : language;
              if (!title.isEmpty() && title.compare(language, Qt::CaseInsensitive) != 0)
                label += QStringLiteral(" (%1)").arg(title);
              if (nodeFlag(mapValue(track, "forced")))
                label += title.contains(QStringLiteral("forced"), Qt::CaseInsensitive)
                             ? QString()
                             : QStringLiteral(" (Forced)");
              if (nodeFlag(mapValue(track, "external")))
                label += QStringLiteral(" (External)");
              if (!codec.isEmpty())
                label += QStringLiteral(" - %1").arg(codec);

              labels.push_back(label);
              ids.push_back(id);
              if (nodeFlag(mapValue(track, "selected")))
                selected = ids.size() - 1;
            } else if (type == QStringLiteral("audio")) {
              const QString language = prettyLanguage(nodeString(mapValue(track, "lang")));
              const QString title = cleanSubtitleTitle(nodeString(mapValue(track, "title")));
              const QString codec = nodeString(mapValue(track, "codec")).toUpper();
              const int channels = static_cast<int>(nodeInt(mapValue(track, "audio-channels"), 0));
              QString label = language.isEmpty() ? QStringLiteral("Audio %1").arg(id) : language;
              if (!title.isEmpty() && title.compare(language, Qt::CaseInsensitive) != 0)
                label += QStringLiteral(" (%1)").arg(title);
              QStringList tail;
              if (channels > 0)
                tail.push_back(channels == 1 ? QStringLiteral("Mono")
                              : channels == 2 ? QStringLiteral("Stereo")
                                              : QStringLiteral("%1ch").arg(channels));
              if (!codec.isEmpty())
                tail.push_back(codec);
              if (!tail.isEmpty())
                label += QStringLiteral(" - %1").arg(tail.join(QStringLiteral(", ")));

              audioLabels.push_back(label);
              audioIds.push_back(id);
              if (nodeFlag(mapValue(track, "selected")))
                audioSelected = audioIds.size() - 1;
            }
          }
        }

        QMetaObject::invokeMethod(this, [this, labels, ids, selected, audioLabels, audioIds, audioSelected]() {
          m_subtitleTracks = labels;
          m_subtitleIds = ids;
          m_selectedSubtitleIndex = selected;
          m_subtitlesEnabled = selected > 0;
          m_audioTracks = audioLabels;
          m_audioIds = audioIds;
          m_selectedAudioIndex = audioSelected;
          qInfo() << "player: subtitle tracks" << labels << "selected" << selected
                  << "audio tracks" << audioLabels << "selected" << audioSelected;
          emit stateChanged();
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
      if (m_pendingFileLoads.load(std::memory_order_acquire) > 0) {
        qInfo() << "player: end file for replaced session, ignoring";
        break;
      }
      QMetaObject::invokeMethod(this, [this, failed, endFileReason, endFileError]() {
        qInfo() << "player: end file (main thread) failed=" << failed
                << "visible=" << m_visible
                << "reason=" << endFileReason
                << "error=" << endFileError
                << (endFileError < 0 ? mpv_error_string(endFileError) : "");
        if (failed)
          m_errorText = QStringLiteral("Playback ended with an mpv error.");
        stopProgressReporting(failed);
        scheduleMpvTeardown();
      });
      break;
    }
    case MPV_EVENT_SHUTDOWN:
      m_terminating = true;
      QMetaObject::invokeMethod(this, [this]() {
        qInfo() << "player: mpv shutdown";
        if (m_visible)
          stopProgressReporting(false);
      });
      break;
    default:
      break;
    }
  }
}

void PlayerController::updatePlaybackStatusText() {
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

double PlayerController::clampedPosition(double seconds) const {
  if (!std::isfinite(seconds))
    return m_positionSeconds;
  if (m_durationSeconds > 0.0)
    return qBound(0.0, seconds, m_durationSeconds);
  return qMax(0.0, seconds);
}

double PlayerController::seekAnchorPosition() {
  if (m_requestedSeekTargetSeconds >= 0.0 && m_seekCommandClock.isValid() &&
      m_seekCommandClock.elapsed() < kSeekTargetFreshnessMs) {
    double position = m_requestedSeekTargetSeconds;
    if (!m_paused && !m_buffering)
      position += m_seekCommandClock.elapsed() / 1000.0;
    return clampedPosition(position);
  }

  if (!m_seeking) {
    double mpvPosition = 0.0;
    if (currentMpvPositionSeconds(&mpvPosition)) {
      const double projectedPosition = projectedPositionSeconds();
      if (mpvPosition + 2.0 < projectedPosition)
        return projectedPosition;
      setPositionSeconds(mpvPosition);
      return mpvPosition;
    }
  }

  return projectedPositionSeconds();
}

bool PlayerController::currentMpvPositionSeconds(double *seconds) const {
  if (!seconds)
    return false;

  auto *handle = m_mpv.load();
  if (!handle)
    return false;

  double value = 0.0;
  int error = mpv_get_property(handle, "time-pos", MPV_FORMAT_DOUBLE, &value);
  if (error < 0 || !std::isfinite(value))
    error = mpv_get_property(handle, "playback-time", MPV_FORMAT_DOUBLE, &value);
  if (error < 0 || !std::isfinite(value))
    return false;

  *seconds = playbackPositionFromMpvTime(value);
  return true;
}

double PlayerController::projectedPositionSeconds() const {
  double position = m_positionSeconds;
  if (!m_paused && !m_buffering && m_positionClock.isValid()) {
    const double elapsed = m_positionClock.elapsed() / 1000.0;
    if (elapsed > 0.0)
      position += elapsed;
  }
  return clampedPosition(position);
}

void PlayerController::setPositionSeconds(double seconds) {
  const double clamped = clampedPosition(seconds);
  if (std::abs(m_positionSeconds - clamped) < 0.05) {
    m_positionClock.restart();
    return;
  }

  m_positionSeconds = clamped;
  m_positionClock.restart();
  emit stateChanged();
}

double PlayerController::playbackPositionFromMpvTime(double seconds) const {
  if (!std::isfinite(seconds))
    return m_positionSeconds;
  return seconds;
}

} // namespace JellyfinNative
