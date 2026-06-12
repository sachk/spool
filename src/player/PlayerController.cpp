#include "PlayerController.h"

#include "../api/JellyfinApiFacade.h"
#include "../app/NativeAppWindow.h"
#include "../common/JellyfinTypes.h"
#include "../diagnostics/Diagnostics.h"
#include "MpvVideoItem.h"
#include "MpvOptionProfile.h"
#include "PlaybackTrackParser.h"

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
#include <malloc.h>

namespace JellyfinNative {

namespace {

constexpr auto kMpvLogPath = "/tmp/com.codex.jellyfinnative-mpv.log";
constexpr uint64_t kTimePosRefreshReply = 0x6a666e7074730001ULL;
constexpr uint64_t kPlaybackTimeRefreshReply = 0x6a666e7074730002ULL;
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

const char *endFileReasonName(int reason) {
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

bool applyOptions(mpv_handle *handle, const std::vector<MpvOption> &options) {
  bool ok = true;
  for (const MpvOption &option : options)
    ok &= setOption(handle, option.name.constData(), option.value.constData());
  return ok;
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

QByteArray mpvBool(bool value) {
  return value ? QByteArrayLiteral("yes") : QByteArrayLiteral("no");
}

QByteArray mpvArgbColor(const QString &rgb, QByteArray fallback) {
  QString color = rgb.trimmed();
  if (color.startsWith(QLatin1Char('#')))
    color.remove(0, 1);
  if (color.size() != 6)
    return fallback;

  for (const QChar ch : color) {
    if (!ch.isDigit() &&
        (ch.toLower() < QLatin1Char('a') || ch.toLower() > QLatin1Char('f')))
      return fallback;
  }

  return QByteArrayLiteral("#FF") + color.toUpper().toLatin1();
}

QByteArray subtitleFontSize(const QString &value) {
  if (value == QStringLiteral("smaller"))
    return QByteArrayLiteral("44");
  if (value == QStringLiteral("small"))
    return QByteArrayLiteral("50");
  if (value == QStringLiteral("large"))
    return QByteArrayLiteral("66");
  if (value == QStringLiteral("larger"))
    return QByteArrayLiteral("76");
  if (value == QStringLiteral("extralarge"))
    return QByteArrayLiteral("84");
  return QByteArrayLiteral("55");
}

QByteArray subtitleFontFamily(const QString &value) {
  if (value == QStringLiteral("typewriter"))
    return QByteArrayLiteral("Courier New");
  if (value == QStringLiteral("print"))
    return QByteArrayLiteral("Georgia");
  if (value == QStringLiteral("console"))
    return QByteArrayLiteral("Consolas");
  if (value == QStringLiteral("cursive"))
    return QByteArrayLiteral("Lucida Handwriting");
  if (value == QStringLiteral("casual"))
    return QByteArrayLiteral("Segoe Print");
  if (value == QStringLiteral("smallcaps"))
    return QByteArrayLiteral("Copperplate Gothic");
  return QByteArrayLiteral("sans-serif");
}

struct SubtitleShadowOptions {
  QByteArray borderSize = QByteArrayLiteral("3.5");
  QByteArray shadowOffset = QByteArrayLiteral("1");
  QByteArray shadowColor = QByteArrayLiteral("#80000000");
};

SubtitleShadowOptions subtitleShadowOptions(const QString &value) {
  SubtitleShadowOptions options;
  if (value == QStringLiteral("none")) {
    options.shadowOffset = QByteArrayLiteral("0");
    options.shadowColor = QByteArrayLiteral("#00000000");
  } else if (value == QStringLiteral("raised")) {
    options.shadowOffset = QByteArrayLiteral("1");
    options.shadowColor = QByteArrayLiteral("#A0000000");
  } else if (value == QStringLiteral("depressed")) {
    options.shadowOffset = QByteArrayLiteral("-1");
    options.shadowColor = QByteArrayLiteral("#A0000000");
  } else if (value == QStringLiteral("uniform")) {
    options.borderSize = QByteArrayLiteral("4.5");
    options.shadowOffset = QByteArrayLiteral("0");
    options.shadowColor = QByteArrayLiteral("#00000000");
  }
  return options;
}

qint64 secondsToTicks(double seconds) {
  return static_cast<qint64>(seconds * 10000000.0);
}

// Targeted playback memory accounting. heaptrack can't produce deep call stacks
// on this target (every unwinder crashes), so instead of attributing by stack we
// quantify the known big buffers directly: process RSS (and the anon/heap part),
// glibc's live-vs-free heap totals (mallinfo), and mpv's own demuxer cache size.
// The gap between malloc_inuse and demux_cache localises non-cache heap growth.
void logMemoryStats(mpv_handle *handle) {
  (void)handle;
  long vmrss = 0, rssAnon = 0, vmdata = 0, vmswap = 0;
  if (FILE *f = fopen("/proc/self/status", "r")) {
    char line[256];
    while (fgets(line, sizeof(line), f)) {
      long v;
      if (sscanf(line, "VmRSS: %ld kB", &v) == 1) vmrss = v;
      else if (sscanf(line, "RssAnon: %ld kB", &v) == 1) rssAnon = v;
      else if (sscanf(line, "VmData: %ld kB", &v) == 1) vmdata = v;
      else if (sscanf(line, "VmSwap: %ld kB", &v) == 1) vmswap = v;
    }
    fclose(f);
  }

#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33))
  struct mallinfo2 mi = mallinfo2();
#else
  struct mallinfo mi = mallinfo();
#endif
  const long long mallInUse = (long long)mi.uordblks;   // live (non-mmap) heap
  const long long mallFree = (long long)mi.fordblks;    // freed, retained in arena
  const long long mallArena = (long long)mi.arena;      // total main-arena size
  const long long mallMmap = (long long)mi.hblkhd;      // large allocs via mmap

  const long long MB = 1024 * 1024;
  qInfo().nospace()
      << "player: memstats rss=" << vmrss / 1024 << "M anon=" << rssAnon / 1024
      << "M vmdata=" << vmdata / 1024 << "M swap=" << vmswap / 1024
      << "M | malloc_inuse=" << mallInUse / MB << "M arena_free=" << mallFree / MB
      << "M arena=" << mallArena / MB << "M mmap=" << mallMmap / MB;
}

} // namespace

PlayerController::PlayerController(NativeAppWindow *window,
                                   JellyfinApiFacade *api, QObject *parent)
    : QObject(parent), m_window(window), m_api(api), m_reporter(api, this) {
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
    if (!m_visible || m_paused || m_buffering || m_seeking)
      return;

    if (!m_positionTracker.projectionIsValid())
      return;

    setPositionSeconds(projectedPositionSeconds(),
                       PlaybackPositionTracker::Source::Projection);
  });
  connect(&m_seekWatchdogTimer, &QTimer::timeout, this, [this]() {
    if (!m_visible || !m_seeking)
      return;

    qWarning() << "player: clearing stale seek state";
    m_seeking = false;
    m_positionTracker.cancelSeek();
    updatePlaybackStatusText();
    emit stateChanged();
  });
  connect(&m_progressTimer, &QTimer::timeout, this, [this]() {
    if (!m_visible)
      return;

    logMemoryStats(m_mpvLifecycle.handle());

    m_reporter.reportProgress(secondsToTicks(m_positionTracker.position()),
                              m_paused);
  });
  connect(&m_reporter, &PlaybackReporter::reportFailed, this,
          [](const QString &operation, const QString &message) {
            Diagnostics::logEvent(
                QStringLiteral("player"), QStringLiteral("report_failed"),
                {{QStringLiteral("operation"), operation},
                 {QStringLiteral("message"), message}});
          });
}

PlayerController::~PlayerController() {
  teardownMpv();
}

void PlayerController::teardownMpv() {
  Diagnostics::Phase phase(QStringLiteral("shutdown"), QStringLiteral("player_teardown_mpv"));
  m_mpvLifecycle.destroy([](mpv_handle *) {
#ifndef JELLYFIN_NATIVE_WEBOS
    // Free the render context first; this is thread-safe and decouples us from
    // the scene-graph render thread (which may already be shutting down).
    if (auto *videoItem = MpvVideoItem::instance())
      videoItem->setMpvHandle(nullptr);
#endif
  });
}

void PlayerController::scheduleMpvTeardown() {
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

bool PlayerController::visible() const { return m_visible; }

bool PlayerController::paused() const { return m_paused; }

QString PlayerController::title() const { return m_title; }

QString PlayerController::statusText() const { return m_statusText; }

QString PlayerController::errorText() const { return m_errorText; }

bool PlayerController::buffering() const { return m_buffering; }

int PlayerController::bufferingPercent() const { return m_bufferingPercent; }

bool PlayerController::seeking() const { return m_seeking; }

bool PlayerController::debugOsdVisible() const { return m_debugOsdVisible; }

bool PlayerController::subtitlesEnabled() const { return m_tracks.subtitlesEnabled(); }

QStringList PlayerController::subtitleTracks() const { return m_tracks.subtitleTracks(); }

int PlayerController::selectedSubtitleIndex() const { return m_tracks.selectedSubtitleIndex(); }

QStringList PlayerController::audioTracks() const { return m_tracks.audioTracks(); }

int PlayerController::selectedAudioIndex() const { return m_tracks.selectedAudioIndex(); }

bool PlayerController::backAllowed() const { return m_backAllowed; }

double PlayerController::positionSeconds() const {
  return m_positionTracker.position();
}

double PlayerController::durationSeconds() const {
  return m_positionTracker.duration();
}

QVariantList PlayerController::chapters() const { return m_tracks.chapters(); }

bool PlayerController::hasChapters() const { return m_tracks.hasChapters(); }

int PlayerController::currentChapter() const { return m_tracks.currentChapter(); }

bool PlayerController::nightModeEnabled() const { return m_nightModeEnabled.load(); }

bool PlayerController::toneMappingVisualizationEnabled() const {
  return m_toneMappingVisualizationEnabled.load();
}

int PlayerController::audioDelayMs() const { return m_audioDelayMs.load(); }

QString PlayerController::audioOutputMode() const { return m_audioOutputMode; }

int PlayerController::volume() const { return m_volume.load(); }

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
         applyMpvRuntimeOption(MpvRuntimeOption::ToneMappingVisualization,
                               mode, handle) &&
         applyMpvRuntimeOption(MpvRuntimeOption::AudioDelay, mode, handle) &&
         applyMpvSubtitleOptions(mode, handle);
}

bool PlayerController::applyMpvSubtitleOptions(MpvOptionApplyMode mode,
                                               mpv_handle *handle) {
  if (!handle)
    return false;

  auto applyString = [mode, handle](const char *name, const QByteArray &value) {
    return mode == MpvOptionApplyMode::Initial
               ? setOption(handle, name, value.constData())
               : setMpvProperty(handle, name, value.constData());
  };

  const SubtitlePreferences prefs = m_subtitlePreferences;
  const QString subtitleMode =
      prefs.mode.isEmpty() ? QStringLiteral("Default") : prefs.mode;
  const bool noSubtitles = subtitleMode == QStringLiteral("None");
  const bool onlyForced = subtitleMode == QStringLiteral("OnlyForced");
  const bool alwaysPlay = subtitleMode == QStringLiteral("Always");
  const bool smart = subtitleMode == QStringLiteral("Smart");
  const bool nativeStyling = prefs.styling == QStringLiteral("Native");
  const int vertical = qBound(-16, prefs.verticalPosition, 16);
  const int margin = vertical < 0 ? std::abs(vertical + 1) * 20 : vertical * 20;
  const SubtitleShadowOptions shadow = subtitleShadowOptions(prefs.dropShadow);

  bool ok = true;
  ok &= applyString("sid", !m_tracks.subtitlesEnabled() || noSubtitles ? QByteArrayLiteral("no")
                                                                       : QByteArrayLiteral("auto"));
  ok &= applyString("slang", prefs.language.toUtf8());
  ok &= applyString("sub-auto", QByteArrayLiteral("all"));
  ok &= applyString("sub-visibility", mpvBool(!noSubtitles));
  ok &= applyString("sub-forced-events-only", mpvBool(onlyForced));
  ok &= applyString("subs-with-matching-audio", mpvBool(alwaysPlay));
  ok &= applyString("subs-fallback", mpvBool(!noSubtitles && !onlyForced && !smart));
  ok &= applyString("subs-fallback-forced", QByteArrayLiteral("yes"));
  ok &= applyString("sub-ass", QByteArrayLiteral("yes"));
  ok &= applyString("sub-ass-override", nativeStyling ? QByteArrayLiteral("no")
                                                      : QByteArrayLiteral("force"));
  ok &= applyString("sub-use-margins", QByteArrayLiteral("yes"));
  ok &= applyString("sub-font", subtitleFontFamily(prefs.font));
  ok &= applyString("sub-font-size", subtitleFontSize(prefs.textSize));
  ok &= applyString("sub-bold", mpvBool(prefs.textWeight == QStringLiteral("bold")));
  ok &= applyString("sub-pos", vertical < 0 ? QByteArrayLiteral("100")
                                            : QByteArrayLiteral("0"));
  ok &= applyString("sub-margin-y", QByteArray::number(margin));
  ok &= applyString("sub-color", mpvArgbColor(prefs.textColor, QByteArrayLiteral("#FFFFFFFF")));
  ok &= applyString("sub-border-size", shadow.borderSize);
  ok &= applyString("sub-border-color", QByteArrayLiteral("#FF000000"));
  ok &= applyString("sub-shadow-offset", shadow.shadowOffset);
  ok &= applyString("sub-shadow-color", shadow.shadowColor);

  if (!ok) {
    qWarning() << "player: failed to apply subtitle preferences"
               << "mode=" << (mode == MpvOptionApplyMode::Initial ? "initial" : "runtime");
  }
  return ok;
}

bool PlayerController::ensureMpv() {
  if (m_mpvLifecycle.handle())
    return true;

#ifdef JELLYFIN_NATIVE_WEBOS
  const bool useStarfishPcm = m_audioOutputMode == QStringLiteral("starfish") ||
                              m_audioOutputMode == QStringLiteral("starfish-pcm");
  const bool useStarfishAudio = useStarfishPcm;
  qputenv("STARFISH_AUDIO_HINT", useStarfishAudio ? QByteArrayLiteral("1") : QByteArrayLiteral("0"));
  qputenv("WEBOS_ALSA_NO_HW_PAUSE", useStarfishAudio ? QByteArrayLiteral("0") : QByteArrayLiteral("1"));
  // Selects the Starfish audio ES the fork builds: raw PCM vs the legacy AAC
  // encode path. Read by both ao_starfish and the starfish VO context.
  qputenv("STARFISH_AUDIO_CODEC", useStarfishPcm ? QByteArrayLiteral("pcm") : QByteArrayLiteral("aac"));
  qInfo() << "player: configuring webOS audio output"
          << m_audioOutputMode
          << "starfishAudioHint=" << qgetenv("STARFISH_AUDIO_HINT")
          << "starfishAudioCodec=" << qgetenv("STARFISH_AUDIO_CODEC")
          << "webosAlsaNoHwPause=" << qgetenv("WEBOS_ALSA_NO_HW_PAUSE");
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

#ifdef JELLYFIN_NATIVE_WEBOS
  constexpr auto platform = MpvOptionProfile::Platform::WebOS;
#else
  constexpr auto platform = MpvOptionProfile::Platform::Desktop;
#endif
  const auto startupOptions = MpvOptionProfile::startupOptions(
      platform, m_audioOutputMode, QByteArray(kMpvLogPath));
  const bool configured =
      applyOptions(handle, startupOptions) &&
      applyMpvRuntimeOptions(MpvOptionApplyMode::Initial, handle);

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
  mpv_observe_property(handle, 0, "volume", MPV_FORMAT_DOUBLE);
  mpv_observe_property(handle, 0, "track-list", MPV_FORMAT_NODE);
  mpv_observe_property(handle, 0, "chapter-list", MPV_FORMAT_NODE);
  mpv_observe_property(handle, 0, "chapter", MPV_FORMAT_INT64);

  qInfo() << "player: mpv initialized in" << startupTimer.elapsed() << "ms";

#ifndef JELLYFIN_NATIVE_WEBOS
  // vo=libmpv requires the embedded MpvVideoItem to host the render context.
  // Fail loudly if QML hasn't constructed one yet — silently falling back
  // would leave us with no video at all.
  auto *videoItem = MpvVideoItem::instance();
  if (!videoItem) {
    qCritical() << "PlayerController: MpvVideoItem instance is missing";
    mpv_terminate_destroy(handle);
    m_errorText = QStringLiteral(
        "The video surface is unavailable. Return to the library and try again.");
    m_statusText = QStringLiteral("Playback unavailable");
    emit stateChanged();
    return false;
  }
  connect(videoItem, &MpvVideoItem::renderError, this,
          &PlayerController::handleVideoRenderError, Qt::UniqueConnection);
  videoItem->setMpvHandle(handle);
#endif

  if (!m_mpvLifecycle.adopt(
          handle, [this](mpv_event *event) { handleMpvEvent(event); })) {
#ifndef JELLYFIN_NATIVE_WEBOS
    if (auto *videoItem = MpvVideoItem::instance())
      videoItem->setMpvHandle(nullptr);
#endif
    mpv_terminate_destroy(handle);
    m_errorText = QStringLiteral("Failed to start the libmpv event loop.");
    emit stateChanged();
    return false;
  }
  return true;
}

void PlayerController::handleVideoRenderError(const QString &message) {
  m_errorText = message;
  m_statusText = QStringLiteral("Playback unavailable");
  emit stateChanged();
}

void PlayerController::play(const PlaybackSession &session) {
  Diagnostics::Task task(QStringLiteral("player_play"), {{QStringLiteral("itemId"), session.itemId}, {QStringLiteral("title"), session.title}});
  qInfo() << "player: play requested" << session.title
          << "method=" << session.playMethod
          << "startTimeTicks=" << session.startTimeTicks;

  if (m_mpvLifecycle.handle()) {
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
  m_timeline.setSession(session);
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
  m_visible = true;
  emit visibleChanged();
  emit stateChanged();

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
    qInfo() << "player: instructing mpv to start at resume position seconds="
            << startSeconds;
  }
  const char *loadCommand[] = {"loadfile", urlBytes.constData(), "replace",
                               nullptr};
  if (mpv_command(handle, loadCommand) < 0) {
    m_mpvLifecycle.cancelFileLoad();
    m_errorText = QStringLiteral("libmpv rejected the playback URL.");
    stopProgressReporting(true);
    return;
  }
  mpv_command_string(handle, "set pause no");
}

void PlayerController::togglePause() {
  qInfo() << "player: toggle pause requested";
  mpvCommand("no-osd cycle pause");
}

void PlayerController::prepareForBackground() {
#ifdef JELLYFIN_NATIVE_WEBOS
  const double position = projectedPositionSeconds();
  qInfo() << "player: playback position snapshot background"
          << "position=" << position;
  setPositionSeconds(position, PlaybackPositionTracker::Source::Lifecycle);
#endif
}

void PlayerController::pauseForBackground() {
#ifdef JELLYFIN_NATIVE_WEBOS
  prepareForBackground();
  if (!m_visible || m_paused)
    return;

  qInfo() << "player: pausing for background/hidden app state";
  mpvCommand("no-osd set pause yes");
#endif
}

void PlayerController::resyncForForeground() {
#ifdef JELLYFIN_NATIVE_WEBOS
  if (!m_visible)
    return;

  qInfo() << "player: foreground position resync requested";
  restoreTrustedPosition("foreground");
  requestMpvPositionRefresh("foreground");

  for (int delayMs : {250, 1000, 2500}) {
    QTimer::singleShot(delayMs, this, [this]() {
      if (!m_visible)
        return;
      restoreTrustedPosition("foreground-delayed");
      requestMpvPositionRefresh("foreground-delayed");
    });
  }
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

  const double clampedSeconds = m_positionTracker.clamp(seconds);
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
  if (const auto target = m_tracks.toggleSubtitleTarget())
    selectSubtitle(*target);
}

void PlayerController::cycleSubtitles() {
  // Off (index 0) -> first track -> second -> ... -> last -> Off.
  if (const auto target = m_tracks.cycleSubtitleTarget())
    selectSubtitle(*target);
}

void PlayerController::enableSubtitles() {
  if (const auto target = m_tracks.enableSubtitleTarget())
    selectSubtitle(*target);
}

void PlayerController::cycleAudio() {
  if (const auto target = m_tracks.cycleAudioTarget())
    selectAudio(*target);
}

void PlayerController::selectSubtitle(int index) {
  const std::optional<QByteArray> command = m_tracks.subtitleCommand(index);
  if (!command)
    return;

  mpvCommand(command->constData());
  m_tracks.applySubtitleSelection(index);
  if (!m_tracks.subtitlesEnabled()) {
    m_window->clearOverlay();
  }
  emit stateChanged();
}

void PlayerController::selectAudio(int index) {
  const std::optional<QByteArray> command = m_tracks.audioCommand(index);
  if (!command)
    return;

  if (!mpvCommand(command->constData()))
    return;
  m_tracks.applyAudioSelection(index);
#ifdef JELLYFIN_NATIVE_WEBOS
  qInfo() << "player: webOS audio track changed" << index;
#endif
  emit stateChanged();
}

void PlayerController::nextChapter() {
  if (!m_tracks.hasChapters())
    return;
  m_positionTracker.allowRegression();
  mpvCommand("add chapter 1");
}

void PlayerController::previousChapter() {
  if (!m_tracks.hasChapters())
    return;
  m_positionTracker.allowRegression();
  mpvCommand("add chapter -1");
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

  mpvCommand("stop");
  scheduleMpvTeardown();
}

void PlayerController::setNightModeEnabled(bool enabled) {
  if (m_nightModeEnabled.load() == enabled)
    return;

  m_nightModeEnabled = enabled;
  if (auto *handle = m_mpvLifecycle.handle()) {
    applyMpvRuntimeOption(MpvRuntimeOption::NightMode,
                          MpvOptionApplyMode::Runtime, handle);
  }

  emit nightModeEnabledChanged();
  emit stateChanged();
}

void PlayerController::setToneMappingVisualizationEnabled(bool enabled) {
  if (m_toneMappingVisualizationEnabled.load() == enabled)
    return;

  m_toneMappingVisualizationEnabled = enabled;
  if (auto *handle = m_mpvLifecycle.handle()) {
    applyMpvRuntimeOption(MpvRuntimeOption::ToneMappingVisualization,
                          MpvOptionApplyMode::Runtime, handle);
  }

  emit toneMappingVisualizationEnabledChanged();
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
  if (auto *handle = m_mpvLifecycle.handle()) {
    applyMpvRuntimeOption(MpvRuntimeOption::AudioDelay,
                          MpvOptionApplyMode::Runtime, handle);
  } else {
    qInfo() << "player: audio delay stored without active mpv";
  }

  emit audioDelayMsChanged();
  emit stateChanged();
}

void PlayerController::setAudioOutputMode(const QString &mode) {
  const QString normalized = normalizedAudioOutputMode(mode);
  if (m_audioOutputMode == normalized)
    return;

  m_audioOutputMode = normalized;
  qInfo() << "player: audio output mode changed" << normalized
          << "visible=" << m_visible;
  emit audioOutputModeChanged();
  emit stateChanged();
}

void PlayerController::setVolume(int volume) {
  const int clampedVolume = qBound(0, volume, 100);
  if (m_volume.load() == clampedVolume)
    return;

  m_volume = clampedVolume;
  const QByteArray command =
      QByteArray("no-osd set volume ") + QByteArray::number(clampedVolume);
  mpvCommand(command.constData());
  emit volumeChanged();
}

void PlayerController::adjustVolume(int delta) {
  if (delta == 0)
    return;
  setVolume(m_volume.load() + delta);
}

void PlayerController::setSubtitlePreferences(const SubtitlePreferences &preferences) {
  m_subtitlePreferences = preferences;
  qInfo() << "player: subtitle preferences changed"
          << "mode=" << preferences.mode
          << "language=" << preferences.language
          << "styling=" << preferences.styling;
  if (auto *handle = m_mpvLifecycle.handle())
    applyMpvSubtitleOptions(MpvOptionApplyMode::Runtime, handle);
  emit stateChanged();
}

void PlayerController::startProgressReporting() {
  Diagnostics::logEvent(QStringLiteral("player"), QStringLiteral("progress_reporting_start"), {{QStringLiteral("itemId"), m_session.itemId}});
  if (m_progressTimer.isActive())
    return;
  m_progressTimer.start();

  m_reporter.start(m_session);
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
  const qint64 positionTicks = secondsToTicks(m_positionTracker.position());
  m_reporter.stop(positionTicks, failed);

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
  m_positionTracker.clear();
  m_debugOsdVisible = false;
  m_timeline.clear();
  m_statusText = QStringLiteral("Ready");
  if (m_tracks.clearChapters()) {
    emit chaptersChanged();
  }
}

bool PlayerController::mpvCommand(const char *command) {
  auto *handle = m_mpvLifecycle.handle();
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
    m_seekWatchdogTimer.start();
    updatePlaybackStatusText();
    emit stateChanged();
  }

  m_positionTracker.beginSeek(clampedTarget);
  setPositionSeconds(clampedTarget, PlaybackPositionTracker::Source::Seek);

  const QByteArray command = buildSeekCommand(clampedTarget, flags);
  if (mpvCommand(command.constData()))
    return true;

  if (markSeeking) {
    m_seeking = false;
    m_seekWatchdogTimer.stop();
    m_positionTracker.cancelSeek();
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
  m_seekWatchdogTimer.start();
  m_positionTracker.beginSeek(optimisticTarget);
  setPositionSeconds(optimisticTarget, PlaybackPositionTracker::Source::Seek);
  updatePlaybackStatusText();
  emit stateChanged();

  const QByteArray command = QByteArray("no-osd seek ") +
                             QByteArray::number(optimisticTarget, 'f', 3) +
                             QByteArray(" absolute+keyframes");
  if (mpvCommand(command.constData()))
    return true;

  m_seeking = false;
  m_seekWatchdogTimer.stop();
  m_positionTracker.cancelSeek();
  updatePlaybackStatusText();
  emit stateChanged();
  return false;
}

void PlayerController::handleMpvEvent(mpv_event *event) {
  if (!event)
    return;

  switch (event->event_id) {
    case MPV_EVENT_FILE_LOADED:
      m_mpvLifecycle.completeFileLoad();
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
          m_positionTracker.settleSeek();
          m_seekWatchdogTimer.stop();
        }
        m_positionTracker.restartProjection();
        updatePlaybackStatusText();
        emit stateChanged();
      });
      break;
    case MPV_EVENT_GET_PROPERTY_REPLY: {
      if (event->reply_userdata != kTimePosRefreshReply &&
          event->reply_userdata != kPlaybackTimeRefreshReply)
        break;

      auto *property = static_cast<mpv_event_property *>(event->data);
      if (!property || !property->data || property->format != MPV_FORMAT_DOUBLE)
        break;

      const double seconds = *static_cast<double *>(property->data);
      QMetaObject::invokeMethod(this, [this, seconds]() {
        setPositionSeconds(seconds, PlaybackPositionTracker::Source::Mpv);
      });
      break;
    }
    case MPV_EVENT_PROPERTY_CHANGE: {
      auto *property = static_cast<mpv_event_property *>(event->data);
      if (!property || !property->data)
        break;

      if (strcmp(property->name, "pause") == 0 &&
          property->format == MPV_FORMAT_FLAG) {
        const bool paused = *static_cast<int *>(property->data);
        QMetaObject::invokeMethod(this, [this, paused]() {
          if (m_paused != paused)
            qInfo() << "player: pause state changed" << paused;
          const double positionBeforeStateChange = projectedPositionSeconds();
          m_paused = paused;
          if (m_paused) {
            setPositionSeconds(
                positionBeforeStateChange,
                PlaybackPositionTracker::Source::Projection);
            m_positionTracker.invalidateProjection();
          } else {
            restoreTrustedPosition("unpause");
            requestMpvPositionRefresh("unpause");
            m_positionTracker.restartProjection();
          }
          updatePlaybackStatusText();
          emit stateChanged();
        });
      } else if (strcmp(property->name, "paused-for-cache") == 0 &&
                 property->format == MPV_FORMAT_FLAG) {
        const bool buffering = *static_cast<int *>(property->data);
        QMetaObject::invokeMethod(this, [this, buffering]() {
          const double positionBeforeStateChange = projectedPositionSeconds();
          m_buffering = buffering;
          if (buffering) {
            setPositionSeconds(
                positionBeforeStateChange,
                PlaybackPositionTracker::Source::Projection);
            m_positionTracker.invalidateProjection();
          } else {
            m_bufferingPercent = 0;
            if (!m_paused) {
              restoreTrustedPosition("buffering-complete");
              m_positionTracker.restartProjection();
            }
          }
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
            m_positionTracker.settleSeek();
          }
          updatePlaybackStatusText();
          emit stateChanged();
        });
      } else if (strcmp(property->name, "time-pos") == 0 &&
                 property->format == MPV_FORMAT_DOUBLE) {
        const double seconds = *static_cast<double *>(property->data);
        QMetaObject::invokeMethod(this, [this, seconds]() {
          // While a seek is in flight, keep the optimistic target visible.
          // Once it settles, mpv updates go through the same stale-regression
          // guard as foreground refresh replies.
          if (m_seeking ||
              m_positionTracker.seekIsFresh(1500))
            return;
          setPositionSeconds(seconds, PlaybackPositionTracker::Source::Mpv);
        });
      } else if (strcmp(property->name, "duration") == 0 &&
                 property->format == MPV_FORMAT_DOUBLE) {
        const double seconds = *static_cast<double *>(property->data);
        QMetaObject::invokeMethod(this, [this, seconds]() {
          m_positionTracker.setDuration(seconds);
          m_timeline.updatePosition(m_positionTracker.position());
          emit stateChanged();
        });
      } else if (strcmp(property->name, "volume") == 0 &&
                 property->format == MPV_FORMAT_DOUBLE) {
        const auto volume = static_cast<int>(std::round(*static_cast<double *>(property->data)));
        QMetaObject::invokeMethod(this, [this, volume]() {
          const int clampedVolume = qBound(0, volume, 100);
          if (m_volume.load() == clampedVolume)
            return;
          m_volume = clampedVolume;
          emit volumeChanged();
        });
      } else if (strcmp(property->name, "track-list") == 0 &&
                 property->format == MPV_FORMAT_NODE) {
        const auto *node = static_cast<mpv_node *>(property->data);
        const ParsedPlaybackTracks tracks =
            PlaybackTrackParser::parseTracks(node);
        QMetaObject::invokeMethod(this, [this, tracks]() {
          m_tracks.applyParsedTracks(tracks);
          qInfo() << "player: subtitle tracks" << tracks.subtitleLabels
                  << "selected" << tracks.selectedSubtitleIndex
                  << "audio tracks" << tracks.audioLabels
                  << "selected" << tracks.selectedAudioIndex;
          emit stateChanged();
        });
      } else if (strcmp(property->name, "chapter-list") == 0 &&
                 property->format == MPV_FORMAT_NODE) {
        const auto *node = static_cast<mpv_node *>(property->data);
        const QVariantList chapters = PlaybackTrackParser::parseChapters(node);
        QMetaObject::invokeMethod(this, [this, chapters]() {
          m_tracks.setChapters(chapters);
          qInfo() << "player: chapters" << chapters.size();
          emit chaptersChanged();
          emit stateChanged();
        });
      } else if (strcmp(property->name, "chapter") == 0 &&
                 property->format == MPV_FORMAT_INT64) {
        const int chapter = static_cast<int>(*static_cast<int64_t *>(property->data));
        QMetaObject::invokeMethod(this, [this, chapter]() {
          if (m_tracks.setCurrentChapter(chapter))
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
      if (m_mpvLifecycle.hasPendingFileLoads()) {
        qInfo() << "player: end file for replaced session, ignoring";
        break;
      }
      QMetaObject::invokeMethod(this, [this, failed, endFileReason, endFileError]() {
        qInfo() << "player: end file (main thread) failed=" << failed
                << "visible=" << m_visible
                << "reason=" << endFileReason
                << endFileReasonName(endFileReason)
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
      m_mpvLifecycle.requestEventLoopStop();
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
  return m_positionTracker.clamp(seconds);
}

double PlayerController::seekAnchorPosition() {
  return m_positionTracker.seekAnchor(m_paused, m_buffering);
}

void PlayerController::requestMpvPositionRefresh(const char *reason) {
  auto *handle = m_mpvLifecycle.handle();
  if (!m_visible || !handle)
    return;

  int error = mpv_get_property_async(handle, kTimePosRefreshReply, "time-pos",
                                     MPV_FORMAT_DOUBLE);
  if (error < 0)
    qWarning() << "player: async time-pos refresh failed"
               << (reason ? reason : "unknown")
               << mpv_error_string(error);

  error = mpv_get_property_async(handle, kPlaybackTimeRefreshReply,
                                 "playback-time", MPV_FORMAT_DOUBLE);
  if (error < 0)
    qWarning() << "player: async playback-time refresh failed"
               << (reason ? reason : "unknown")
               << mpv_error_string(error);
}

void PlayerController::restoreTrustedPosition(const char *reason) {
  if (m_positionTracker.restoreTrusted(reason)) {
    m_timeline.updatePosition(m_positionTracker.position());
    emit stateChanged();
  }
}

double PlayerController::projectedPositionSeconds() const {
  return m_positionTracker.projected(m_paused, m_buffering);
}

void PlayerController::setPositionSeconds(
    double seconds, PlaybackPositionTracker::Source source) {
  if (!m_positionTracker.update(seconds, source))
    return;

  m_timeline.updatePosition(m_positionTracker.position());

  emit stateChanged();
}

QString PlayerController::activeSegmentType() const {
  return m_timeline.activeSegmentType();
}
double PlayerController::activeSegmentEndSeconds() const {
  return m_timeline.activeSegmentEndSeconds();
}
bool PlayerController::trickplayAvailable() const {
  return m_timeline.trickplayAvailable();
}

QStringList PlayerController::trickplaySheetUrls() const {
  QStringList urls;
  if (!trickplayAvailable() || !m_api)
    return urls;

  const int sheetCount = m_timeline.trickplaySheetCount();
  urls.reserve(sheetCount);
  for (int i = 0; i < sheetCount; ++i)
    urls.push_back(
        m_api->trickplayTileUrl(m_session.itemId, m_timeline.trickplayWidth(), i));
  return urls;
}

void PlayerController::skipActiveSegment() {
  if (activeSegmentType().isEmpty() || activeSegmentEndSeconds() <= 0.0)
    return;
  seek(activeSegmentEndSeconds());
}

QVariantMap PlayerController::trickplayForSeconds(double seconds) const {
  // Returns { url, width, height, offsetX, offsetY, available } so QML can
  // paint a single tile sprite from a positioned BorderImage / clipped Image.
  QVariantMap result;
  if (!trickplayAvailable() || !m_api) {
    result.insert(QStringLiteral("available"), false);
    return result;
  }
  const PlaybackTimeline::TrickplayFrame frame =
      m_timeline.trickplayFrameAt(seconds);
  if (!frame.available) {
    result.insert(QStringLiteral("available"), false);
    return result;
  }
  result.insert(QStringLiteral("available"), true);
  result.insert(QStringLiteral("url"),
                m_api->trickplayTileUrl(m_session.itemId,
                                        m_timeline.trickplayWidth(),
                                        frame.sheetIndex));
  result.insert(QStringLiteral("width"), frame.width);
  result.insert(QStringLiteral("height"), frame.height);
  result.insert(QStringLiteral("offsetX"), frame.offsetX);
  result.insert(QStringLiteral("offsetY"), frame.offsetY);
  result.insert(QStringLiteral("sheetWidth"), frame.sheetWidth);
  result.insert(QStringLiteral("sheetHeight"), frame.sheetHeight);
  return result;
}

} // namespace JellyfinNative
