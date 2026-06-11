#pragma once

#include "../common/JellyfinTypes.h"

#include <QObject>
#include <QTimer>
#include <QtTypes>

namespace JellyfinNative {

class JellyfinApiFacade;

class PlaybackReporter final : public QObject {
  Q_OBJECT

public:
  explicit PlaybackReporter(JellyfinApiFacade *api, QObject *parent = nullptr);

  void start(const PlaybackSession &session);
  void reportProgress(qint64 positionTicks, bool paused);
  void stop(qint64 positionTicks, bool failed);

signals:
  void reportFailed(const QString &operation, const QString &message);

private:
  void sendStart();
  void sendProgress();
  void sendStop(const PlaybackSession &session, qint64 positionTicks,
                bool failed, int attempt);

  JellyfinApiFacade *m_api = nullptr;
  PlaybackSession m_session;
  QTimer m_startRetryTimer;
  QTimer m_progressRetryTimer;
  qint64 m_pendingPositionTicks = 0;
  bool m_pendingPaused = false;
  bool m_active = false;
  bool m_startInFlight = false;
  bool m_startReported = false;
  bool m_progressInFlight = false;
  bool m_progressPending = false;
  quint64 m_generation = 0;
};

} // namespace JellyfinNative
