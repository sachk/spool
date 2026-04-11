#include "PlayerController.h"

#include "../api/JellyfinApiFacade.h"
#include "../app/NativeAppWindow.h"
#include "../common/JellyfinTypes.h"

#include <QCoroTask>

extern "C" {
#include <mpv/client.h>
}

#include <QCoreApplication>
#include <QMetaObject>

#include <cstdint>
#include <cstring>

namespace JellyfinNative {

namespace {

constexpr auto kMpvLogPath = "/tmp/com.codex.jellyfinnative-mpv.log";

bool setOption(mpv_handle *handle, const char *name, const char *value) {
  const int error = mpv_set_option_string(handle, name, value);
  return error >= 0 || error == MPV_ERROR_OPTION_NOT_FOUND;
}

qint64 secondsToTicks(double seconds) {
  return static_cast<qint64>(seconds * 10000000.0);
}

} // namespace

PlayerController::PlayerController(NativeAppWindow *window,
                                   JellyfinApiFacade *api, QObject *parent)
    : QObject(parent), m_window(window), m_api(api) {
  m_progressTimer.setInterval(5000);
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

PlayerController::~PlayerController() { stop(); }

bool PlayerController::visible() const { return m_visible; }

bool PlayerController::paused() const { return m_paused; }

QString PlayerController::title() const { return m_title; }

QString PlayerController::statusText() const { return m_statusText; }

QString PlayerController::errorText() const { return m_errorText; }

bool PlayerController::buffering() const { return m_buffering; }

int PlayerController::bufferingPercent() const { return m_bufferingPercent; }

bool PlayerController::seeking() const { return m_seeking; }

bool PlayerController::debugOsdVisible() const { return m_debugOsdVisible; }

double PlayerController::positionSeconds() const { return m_positionSeconds; }

double PlayerController::durationSeconds() const { return m_durationSeconds; }

void PlayerController::play(const PlaybackSession &session) {
  stop();
  if (!m_window->prepareForPlaybackSurface()) {
    m_errorText =
        QStringLiteral("Failed to prepare the native playback surface.");
    emit stateChanged();
    return;
  }

  m_session = session;
  m_title = session.title;
  m_statusText = QStringLiteral("Preparing libmpv + Starfish...");
  m_errorText.clear();
  m_positionSeconds = 0.0;
  m_durationSeconds = 0.0;
  m_paused = false;
  m_buffering = false;
  m_bufferingPercent = 0;
  m_seeking = false;
  m_debugOsdVisible = false;
  m_visible = true;
  emit visibleChanged();
  emit stateChanged();

  m_stopRequested = false;
  m_thread = std::thread([this, session]() { runPlayerThread(session); });
}

void PlayerController::togglePause() { mpvCommand("cycle pause"); }

void PlayerController::seekBack() {
  mpvCommand("seek -10 exact");
}

void PlayerController::seekForward() {
  mpvCommand("seek 30 exact");
}

void PlayerController::toggleDebugOsd() {
  mpvCommand("script-binding stats/display-stats-toggle");
  m_debugOsdVisible = !m_debugOsdVisible;
  emit stateChanged();
}

void PlayerController::stop() {
  m_stopRequested = true;
  mpvCommand("quit");
  if (m_thread.joinable())
    m_thread.join();
  stopProgressReporting(false);
}

void PlayerController::startProgressReporting() {
  if (m_progressTimer.isActive())
    return;
  m_progressTimer.start();

  const auto session = m_session;
  QCoro::runDetached(
      m_api->reportPlaybackStart(session), []() {},
      [](const std::exception_ptr &) {});
}

void PlayerController::stopProgressReporting(bool failed) {
  if (!m_visible && !m_progressTimer.isActive())
    return;

  m_progressTimer.stop();

  const auto session = m_session;
  const qint64 positionTicks = secondsToTicks(m_positionSeconds);
  QCoro::runDetached(
      m_api->reportPlaybackStopped(session, positionTicks, failed), []() {},
      [](const std::exception_ptr &) {});

  m_visible = false;
  m_paused = false;
  m_buffering = false;
  m_bufferingPercent = 0;
  m_seeking = false;
  m_debugOsdVisible = false;
  m_statusText = QStringLiteral("Ready");
  emit stateChanged();
  emit visibleChanged();
  emit playbackStopped();
}

void PlayerController::mpvCommand(const char *command) {
  if (auto *handle = m_mpv.load())
    mpv_command_string(handle, command);
}

void PlayerController::runPlayerThread(PlaybackSession session) {
  mpv_handle *handle = mpv_create();
  if (!handle) {
    QMetaObject::invokeMethod(this, [this]() {
      m_errorText = QStringLiteral("mpv_create failed.");
      stopProgressReporting(true);
    });
    return;
  }

  const bool configured =
      setOption(handle, "config", "no") &&
      setOption(handle, "terminal", "no") &&
      setOption(handle, "msg-level", "all=debug") &&
      setOption(handle, "log-file", kMpvLogPath) &&
      setOption(handle, "ytdl", "no") &&
      setOption(handle, "demuxer-lavf-analyzeduration", "0.1") &&
      setOption(handle, "demuxer-lavf-probesize", "32768") &&
      setOption(handle, "cache", "yes") &&
      setOption(handle, "cache-pause", "yes") &&
      setOption(handle, "cache-pause-wait", "1") &&
      setOption(handle, "demuxer-max-bytes", "32M") &&
      setOption(handle, "demuxer-max-back-bytes", "8M") &&
      setOption(handle, "force-window", "immediate") &&
      setOption(handle, "vo", "starfish") &&
      setOption(handle, "vd", "starfish") &&
      setOption(handle, "ao", "starfish,null") &&
      setOption(handle, "sid", "no") && setOption(handle, "sub-auto", "no") &&
      setOption(handle, "audio-file-auto", "no") &&
      setOption(handle, "osc", "no") &&
      setOption(handle, "input-default-bindings", "no") &&
      setOption(handle, "input-vo-keyboard", "no") &&
      setOption(handle, "keep-open", "no") && setOption(handle, "idle", "yes");

  if (!configured || mpv_initialize(handle) < 0) {
    mpv_terminate_destroy(handle);
    QMetaObject::invokeMethod(this, [this]() {
      m_errorText = QStringLiteral("Failed to initialize libmpv.");
      stopProgressReporting(true);
    });
    return;
  }

  m_mpv = handle;
  mpv_observe_property(handle, 0, "pause", MPV_FORMAT_FLAG);
  mpv_observe_property(handle, 0, "paused-for-cache", MPV_FORMAT_FLAG);
  mpv_observe_property(handle, 0, "cache-buffering-state", MPV_FORMAT_INT64);
  mpv_observe_property(handle, 0, "seeking", MPV_FORMAT_FLAG);
  mpv_observe_property(handle, 0, "time-pos", MPV_FORMAT_DOUBLE);
  mpv_observe_property(handle, 0, "duration", MPV_FORMAT_DOUBLE);

  const QByteArray urlBytes = session.url.toUtf8();
  const char *loadCommand[] = {"loadfile", urlBytes.constData(), nullptr};
  if (mpv_command(handle, loadCommand) < 0) {
    m_mpv = nullptr;
    mpv_terminate_destroy(handle);
    QMetaObject::invokeMethod(this, [this]() {
      m_errorText = QStringLiteral("libmpv rejected the playback URL.");
      stopProgressReporting(true);
    });
    return;
  }
  mpv_command_string(handle, "set pause no");

  while (!m_stopRequested) {
    mpv_event *event = mpv_wait_event(handle, 0.1);
    if (!event)
      continue;

    switch (event->event_id) {
    case MPV_EVENT_FILE_LOADED:
      QMetaObject::invokeMethod(this, [this]() {
        updatePlaybackStatusText();
        emit stateChanged();
        startProgressReporting();
      });
      break;
    case MPV_EVENT_PLAYBACK_RESTART:
      QMetaObject::invokeMethod(this, [this]() {
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
          updatePlaybackStatusText();
          emit stateChanged();
        });
      } else if (strcmp(property->name, "time-pos") == 0 &&
                 property->format == MPV_FORMAT_DOUBLE) {
        const double seconds = *static_cast<double *>(property->data);
        QMetaObject::invokeMethod(this, [this, seconds]() {
          m_positionSeconds = seconds;
          emit stateChanged();
        });
      } else if (strcmp(property->name, "duration") == 0 &&
                 property->format == MPV_FORMAT_DOUBLE) {
        const double seconds = *static_cast<double *>(property->data);
        QMetaObject::invokeMethod(this, [this, seconds]() {
          m_durationSeconds = seconds;
          emit stateChanged();
        });
      }
      break;
    }
    case MPV_EVENT_END_FILE: {
      auto *endFile = static_cast<mpv_event_end_file *>(event->data);
      const bool failed = endFile && endFile->error < 0;
      QMetaObject::invokeMethod(this, [this, failed]() {
        if (failed)
          m_errorText = QStringLiteral("Playback ended with an mpv error.");
        stopProgressReporting(failed);
      });
      m_stopRequested = true;
      break;
    }
    case MPV_EVENT_SHUTDOWN:
      QMetaObject::invokeMethod(this,
                                [this]() { stopProgressReporting(false); });
      m_stopRequested = true;
      break;
    default:
      break;
    }
  }

  m_mpv = nullptr;
  mpv_terminate_destroy(handle);
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

} // namespace JellyfinNative
