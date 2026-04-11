#pragma once

#include "../common/JellyfinTypes.h"

#include <QObject>
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
    Q_PROPERTY(bool backAllowed READ backAllowed NOTIFY stateChanged)
    Q_PROPERTY(double positionSeconds READ positionSeconds NOTIFY stateChanged)
    Q_PROPERTY(double durationSeconds READ durationSeconds NOTIFY stateChanged)

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
    bool backAllowed() const;
    double positionSeconds() const;
    double durationSeconds() const;

    Q_INVOKABLE void play(const JellyfinNative::PlaybackSession &session);
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void seekBack();
    Q_INVOKABLE void seekForward();
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void toggleDebugOsd();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void stopWithReason(const QString &reason);

signals:
    void visibleChanged();
    void stateChanged();
    void playbackStopped();

private:
    void startProgressReporting();
    void stopProgressReporting(bool failed = false);
    void mpvCommand(const char *command);
    void runPlayerThread(PlaybackSession session);
    void updatePlaybackStatusText();
    void joinPlayerThread();

    NativeAppWindow *m_window = nullptr;
    JellyfinApiFacade *m_api = nullptr;
    PlaybackSession m_session;
    std::thread m_thread;
    std::thread m_cleanupThread;
    std::atomic_bool m_stopRequested = false;
    std::atomic<mpv_handle *> m_mpv { nullptr };
    QTimer m_progressTimer;
    QTimer m_backGuardTimer;
    bool m_visible = false;
    bool m_paused = false;
    bool m_buffering = false;
    int m_bufferingPercent = 0;
    bool m_seeking = false;
    bool m_debugOsdVisible = false;
    bool m_backAllowed = true;
    QString m_title;
    QString m_statusText = QStringLiteral("Ready");
    QString m_errorText;
    double m_positionSeconds = 0.0;
    double m_durationSeconds = 0.0;
};

} // namespace JellyfinNative
