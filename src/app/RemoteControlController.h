#pragma once

#include "../common/JellyfinTypes.h"
#include "../common/RequestGeneration.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <memory>
#include <vector>

namespace JellyfinNative {

class JellyfinApiFacade;
class PlatformRemoteMediaSession;

class RemoteControlController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList targets READ targets NOTIFY targetsChanged)
    Q_PROPERTY(QString selectedSessionId READ selectedSessionId NOTIFY targetChanged)
    Q_PROPERTY(QString selectedTargetName READ selectedTargetName NOTIFY targetChanged)
    Q_PROPERTY(QString selectedTargetDetail READ selectedTargetDetail NOTIFY targetChanged)
    Q_PROPERTY(bool targetSelected READ targetSelected NOTIFY targetChanged)
    Q_PROPERTY(QVariantMap nowPlayingItem READ nowPlayingItem NOTIFY stateChanged)
    Q_PROPERTY(QVariantList queue READ queue NOTIFY stateChanged)
    Q_PROPERTY(QVariantList audioTracks READ audioTracks NOTIFY stateChanged)
    Q_PROPERTY(QVariantList subtitleTracks READ subtitleTracks NOTIFY stateChanged)
    Q_PROPERTY(QStringList supportedCommands READ supportedCommands NOTIFY stateChanged)
    Q_PROPERTY(qint64 positionTicks READ positionTicks NOTIFY stateChanged)
    Q_PROPERTY(qint64 runtimeTicks READ runtimeTicks NOTIFY stateChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY stateChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY stateChanged)
    Q_PROPERTY(int volume READ volume NOTIFY stateChanged)
    Q_PROPERTY(QString repeatMode READ repeatMode NOTIFY stateChanged)
    Q_PROPERTY(bool shuffled READ shuffled NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit RemoteControlController(JellyfinApiFacade *api, QObject *parent = nullptr);
    ~RemoteControlController() override;

    QVariantList targets() const
    {
        return m_targets;
    }
    QString selectedSessionId() const
    {
        return m_selectedSessionId;
    }
    QString selectedTargetName() const
    {
        return m_selectedTargetName;
    }
    QString selectedTargetDetail() const
    {
        return m_selectedTargetDetail;
    }
    bool targetSelected() const
    {
        return !m_selectedSessionId.isEmpty();
    }
    QVariantMap nowPlayingItem() const
    {
        return m_nowPlayingItem;
    }
    QVariantList queue() const
    {
        return m_queue;
    }
    QVariantList audioTracks() const
    {
        return m_audioTracks;
    }
    QVariantList subtitleTracks() const
    {
        return m_subtitleTracks;
    }
    QStringList supportedCommands() const
    {
        return m_supportedCommands;
    }
    qint64 positionTicks() const
    {
        return m_positionTicks;
    }
    qint64 runtimeTicks() const
    {
        return m_runtimeTicks;
    }
    bool paused() const
    {
        return m_paused;
    }
    bool muted() const
    {
        return m_muted;
    }
    int volume() const
    {
        return m_volume;
    }
    QString repeatMode() const
    {
        return m_repeatMode;
    }
    bool shuffled() const
    {
        return m_shuffled;
    }
    bool busy() const
    {
        return m_busy;
    }

    void start();
    void stop();
    void applySessions(const QJsonArray& sessions);
    bool playItems(const std::vector<MovieItem>& items, int startIndex, const QString& command, bool fromStart);

    Q_INVOKABLE void refreshTargets();
    Q_INVOKABLE void selectTarget(const QString& sessionId);
    Q_INVOKABLE void clearTarget();
    Q_INVOKABLE bool supports(const QString& command) const;
    Q_INVOKABLE qint64 predictedPositionTicks() const;
    Q_INVOKABLE void playItemIds(const QStringList& itemIds, const QString& command = QStringLiteral("PlayNow"),
        int startIndex = 0, qint64 startPositionTicks = -1);
    Q_INVOKABLE void sendPlaystate(const QString& command, qint64 seekPositionTicks = -1);
    Q_INVOKABLE void sendGeneralCommand(const QString& command, const QVariantMap& arguments = {});
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void stopPlayback();
    Q_INVOKABLE void nextTrack();
    Q_INVOKABLE void previousTrack();
    Q_INVOKABLE void seek(qint64 positionTicks);
    Q_INVOKABLE void seekRelative(qint64 deltaTicks);
    Q_INVOKABLE void setVolume(int volume);
    Q_INVOKABLE void adjustVolume(int delta);
    Q_INVOKABLE void toggleMute();
    Q_INVOKABLE void selectAudioTrack(int streamIndex);
    Q_INVOKABLE void selectSubtitleTrack(int streamIndex);
    Q_INVOKABLE void setRepeatMode(const QString& mode);
    Q_INVOKABLE void setShuffled(bool shuffled);
    Q_INVOKABLE void playQueueItem(int index);
    Q_INVOKABLE void moveQueueItem(int from, int to);
    Q_INVOKABLE void removeQueueItem(int index);

signals:
    void targetsChanged();
    void targetChanged();
    void stateChanged();
    void busyChanged();
    void errorText(const QString& text);

private:
    void setBusy(bool busy);
    void applySelectedSession();
    void clearSelectedState();
    void refreshQueueDetails(const QJsonArray& rawQueue);
    void updateMediaSession();
    void runPlay(QStringList itemIds, QString command, int startIndex, qint64 startPositionTicks);
    void reportCommandError(const QString& action, const std::exception_ptr& error);

    JellyfinApiFacade *m_api = nullptr;
    std::unique_ptr<PlatformRemoteMediaSession> m_mediaSession;
    QTimer m_refreshTimer;
    QHash<QString, QJsonObject> m_sessions;
    QVariantList m_targets;
    QString m_selectedSessionId;
    QString m_selectedTargetName;
    QString m_selectedTargetDetail;
    QVariantMap m_nowPlayingItem;
    QVariantList m_queue;
    QVariantList m_audioTracks;
    QVariantList m_subtitleTracks;
    QStringList m_supportedCommands;
    QJsonArray m_rawQueue;
    RequestGeneration m_queueGeneration;
    qint64 m_positionTicks = 0;
    qint64 m_runtimeTicks = 0;
    qint64 m_stateReceivedAtMs = 0;
    double m_playbackRate = 1.0;
    int m_volume = 100;
    bool m_paused = true;
    bool m_muted = false;
    bool m_shuffled = false;
    bool m_busy = false;
    QString m_repeatMode = QStringLiteral("RepeatNone");
};

} // namespace JellyfinNative
