#pragma once

#include "SyncPlayClock.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QWebSocket>

namespace JellyfinNative {

class JellyfinApiFacade;
class PlayerController;

class SyncPlayController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentGroupId READ currentGroupId NOTIFY groupChanged)
    Q_PROPERTY(QString currentGroupName READ currentGroupName NOTIFY groupChanged)
    Q_PROPERTY(QJsonArray groups READ groups NOTIFY groupsChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY groupChanged)
    Q_PROPERTY(QStringList participants READ participants NOTIFY groupChanged)
    Q_PROPERTY(int participantCount READ participantCount NOTIFY groupChanged)
    Q_PROPERTY(QString groupState READ groupState NOTIFY groupChanged)
    Q_PROPERTY(bool socketConnected READ socketConnected NOTIFY connectionChanged)
    Q_PROPERTY(double clockOffsetMs READ clockOffsetMs NOTIFY connectionChanged)
    Q_PROPERTY(double pingMs READ pingMs NOTIFY connectionChanged)

public:
    SyncPlayController(JellyfinApiFacade *api, PlayerController *player, QObject *parent = nullptr);

    QString currentGroupId() const { return m_groupId; }
    QString currentGroupName() const { return m_groupName; }
    QJsonArray groups() const { return m_groups; }
    bool enabled() const { return !m_groupId.isEmpty(); }
    QStringList participants() const { return m_participants; }
    int participantCount() const { return m_participants.size(); }
    QString groupState() const { return m_groupState; }
    bool socketConnected() const;
    double clockOffsetMs() const { return m_clock.offsetMs(); }
    double pingMs() const { return m_clock.pingMs(); }

    Q_INVOKABLE void refreshGroups();
    Q_INVOKABLE void connectSocket();
    Q_INVOKABLE void disconnectSocket();
    Q_INVOKABLE void createGroup(const QString &name);
    Q_INVOKABLE void joinGroup(const QString &groupId);
    Q_INVOKABLE void leaveGroup();
    Q_INVOKABLE void requestPause();
    Q_INVOKABLE void requestPlay();
    Q_INVOKABLE void requestSeekSeconds(double seconds);

signals:
    void groupsChanged();
    void groupChanged();
    void connectionChanged();
    void errorText(const QString &text);

private:
    void handleSocketTextMessage(const QString &message);
    void handleSyncPlayCommand(const QJsonObject &data);
    void handleSyncPlayGroupUpdate(const QJsonObject &data);
    void applyGroupInfo(const QString &groupId, const QJsonObject &info);
    void clearGroup();
    void executeScheduledCommand();
    void correctPlaybackDrift();
    void handlePlayerStateChanged();
    void sendPlayerBufferingState(bool force = false);
    void beginTimeSync();
    void requestTimeSync();
    void scheduleTimeSync();
    void reportRequestError(const QString &action,
                            const std::exception_ptr &error);
    void sendKeepAlive();
    QUrl socketUrl() const;

    JellyfinApiFacade *m_api = nullptr;
    PlayerController *m_player = nullptr;
    QWebSocket m_socket;
    QString m_groupId;
    QString m_groupName;
    QStringList m_participants;
    QString m_groupState;
    QJsonArray m_groups;
    SyncPlayClock m_clock;
    QTimer m_timeSyncTimer;
    QTimer m_commandTimer;
    QTimer m_correctionTimer;
    QTimer m_reconnectTimer;
    QString m_playlistItemId;
    QString m_lastCommandKey;
    QString m_scheduledCommand;
    qint64 m_scheduledPositionTicks = 0;
    qint64 m_scheduledServerTimeMs = 0;
    qint64 m_lastCorrectionAtMs = 0;
    qint64 m_joinedAtServerMs = 0;
    int m_greedyTimeSyncRemaining = 0;
    bool m_socketDesired = false;
    bool m_timeSyncInFlight = false;
    bool m_timeSyncErrorReported = false;
    bool m_playerStateKnown = false;
    bool m_lastPlayerBuffering = false;
};

} // namespace JellyfinNative
