#pragma once

#include <QJsonArray>
#include <QObject>
#include <QString>
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

public:
    SyncPlayController(JellyfinApiFacade *api, PlayerController *player, QObject *parent = nullptr);

    QString currentGroupId() const { return m_groupId; }
    QString currentGroupName() const { return m_groupName; }
    QJsonArray groups() const { return m_groups; }
    bool enabled() const { return !m_groupId.isEmpty(); }

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
    void errorText(const QString &text);

private:
    void handleSocketTextMessage(const QString &message);
    void handleSyncPlayCommand(const QJsonObject &data);
    void handleSyncPlayGroupUpdate(const QJsonObject &data);
    void sendKeepAlive();
    QUrl socketUrl() const;

    JellyfinApiFacade *m_api = nullptr;
    PlayerController *m_player = nullptr;
    QWebSocket m_socket;
    QString m_groupId;
    QString m_groupName;
    QJsonArray m_groups;
};

} // namespace JellyfinNative
