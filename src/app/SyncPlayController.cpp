#include "SyncPlayController.h"

#include "../api/JellyfinApiFacade.h"
#include "../common/JellyfinTypes.h"
#include "../player/PlayerController.h"

#include <QCoroTask>

#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>

namespace JellyfinNative {

SyncPlayController::SyncPlayController(JellyfinApiFacade *api, PlayerController *player, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_player(player)
{
    connect(&m_socket, &QWebSocket::connected, this, []() {
        qInfo() << "syncplay: websocket connected";
    });
    connect(&m_socket, &QWebSocket::disconnected, this, []() {
        qInfo() << "syncplay: websocket disconnected";
    });
    connect(&m_socket, &QWebSocket::textMessageReceived,
            this, &SyncPlayController::handleSocketTextMessage);
    connect(&m_socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit errorText(QStringLiteral("SyncPlay socket error: %1").arg(m_socket.errorString()));
    });
}

void SyncPlayController::refreshGroups()
{
    if (!m_api)
        return;
    connectSocket();
    QCoro::runDetached(
        m_api->fetchSyncPlayGroups(),
        [this](const QJsonArray &groups) {
            m_groups = groups;
            emit groupsChanged();
        },
        [this](const std::exception_ptr &error) {
            emit errorText(exceptionMessage(error));
        });
}

void SyncPlayController::connectSocket()
{
    if (!m_api || m_api->session().accessToken.isEmpty() || m_api->serverUrl().isEmpty())
        return;
    if (m_socket.state() == QAbstractSocket::ConnectedState ||
        m_socket.state() == QAbstractSocket::ConnectingState)
        return;

    QNetworkRequest request(socketUrl());
    request.setRawHeader("X-Emby-Token", m_api->session().accessToken.toUtf8());
    m_socket.open(request);
}

void SyncPlayController::disconnectSocket()
{
    if (m_socket.state() != QAbstractSocket::UnconnectedState)
        m_socket.close();
    m_groupId.clear();
    m_groupName.clear();
    m_groups = {};
    emit groupChanged();
    emit groupsChanged();
}

void SyncPlayController::createGroup(const QString &name)
{
    if (!m_api)
        return;
    connectSocket();
    QCoro::runDetached(
        m_api->createSyncPlayGroup(name),
        [this]() { refreshGroups(); },
        [this](const std::exception_ptr &error) { emit errorText(exceptionMessage(error)); });
}

void SyncPlayController::joinGroup(const QString &groupId)
{
    if (!m_api || groupId.isEmpty())
        return;
    connectSocket();
    QCoro::runDetached(
        m_api->joinSyncPlayGroup(groupId),
        [this, groupId]() {
            m_groupId = groupId;
            for (const auto &value : m_groups) {
                const QJsonObject obj = value.toObject();
                if (obj.value(QStringLiteral("GroupId")).toString() == groupId) {
                    m_groupName = obj.value(QStringLiteral("GroupName")).toString();
                    break;
                }
            }
            emit groupChanged();
        },
        [this](const std::exception_ptr &error) { emit errorText(exceptionMessage(error)); });
}

void SyncPlayController::leaveGroup()
{
    if (!m_api || m_groupId.isEmpty())
        return;
    QCoro::runDetached(
        m_api->leaveSyncPlayGroup(),
        [this]() {
            m_groupId.clear();
            m_groupName.clear();
            emit groupChanged();
        },
        [this](const std::exception_ptr &error) { emit errorText(exceptionMessage(error)); });
}

void SyncPlayController::requestPause()
{
    if (!m_api || m_groupId.isEmpty())
        return;
    QCoro::runDetached(m_api->syncPlayRequestPause(),
                       []() {}, [](const std::exception_ptr &) {});
}

void SyncPlayController::handleSocketTextMessage(const QString &message)
{
    const QJsonDocument document = QJsonDocument::fromJson(message.toUtf8());
    if (!document.isObject())
        return;

    const QJsonObject object = document.object();
    const QString type = object.value(QStringLiteral("MessageType")).toString();
    if (type == QStringLiteral("ForceKeepAlive") || type == QStringLiteral("KeepAlive")) {
        sendKeepAlive();
        return;
    }

    const QJsonObject data = object.value(QStringLiteral("Data")).toObject();
    if (type == QStringLiteral("SyncPlayCommand"))
        handleSyncPlayCommand(data);
    else if (type == QStringLiteral("SyncPlayGroupUpdate"))
        handleSyncPlayGroupUpdate(data);
}

void SyncPlayController::handleSyncPlayCommand(const QJsonObject &data)
{
    if (!m_player)
        return;

    const QString command = data.value(QStringLiteral("Command")).toString();
    const qint64 positionTicks = data.value(QStringLiteral("PositionTicks")).toVariant().toLongLong();
    int delayMs = 0;
    const QDateTime when = QDateTime::fromString(data.value(QStringLiteral("When")).toString(), Qt::ISODateWithMs);
    if (when.isValid())
        delayMs = static_cast<int>(std::max<qint64>(0, QDateTime::currentDateTimeUtc().msecsTo(when.toUTC())));

    QTimer::singleShot(delayMs, this, [this, command, positionTicks]() {
        if (command == QStringLiteral("Pause")) {
            if (!m_player->paused())
                m_player->togglePause();
        } else if (command == QStringLiteral("Unpause")) {
            if (m_player->paused())
                m_player->togglePause();
        } else if (command == QStringLiteral("Seek")) {
            m_player->seek(static_cast<double>(positionTicks) / 10000000.0);
        } else if (command == QStringLiteral("Stop")) {
            m_player->stopWithReason(QStringLiteral("syncplay-stop"));
        }
    });
}

void SyncPlayController::handleSyncPlayGroupUpdate(const QJsonObject &data)
{
    const QString type = data.value(QStringLiteral("Type")).toString();
    if (type == QStringLiteral("GroupJoined")) {
        m_groupId = data.value(QStringLiteral("GroupId")).toString();
        if (m_groupName.isEmpty())
            m_groupName = QStringLiteral("SyncPlay group");
        refreshGroups();
        emit groupChanged();
    } else if (type == QStringLiteral("GroupLeft") || type == QStringLiteral("NotInGroup")) {
        m_groupId.clear();
        m_groupName.clear();
        emit groupChanged();
    } else if (type == QStringLiteral("GroupDoesNotExist") || type == QStringLiteral("LibraryAccessDenied")) {
        emit errorText(QStringLiteral("SyncPlay: %1").arg(type));
    }
}

void SyncPlayController::sendKeepAlive()
{
    if (m_socket.state() == QAbstractSocket::ConnectedState)
        m_socket.sendTextMessage(QStringLiteral(R"({"MessageType":"KeepAlive"})"));
}

QUrl SyncPlayController::socketUrl() const
{
    QUrl url(m_api->serverUrl());
    url.setScheme(url.scheme() == QStringLiteral("https") ? QStringLiteral("wss") : QStringLiteral("ws"));
    url.setPath(QStringLiteral("/socket"));

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("api_key"), m_api->session().accessToken);
    if (!m_api->deviceId().isEmpty())
        query.addQueryItem(QStringLiteral("deviceId"), m_api->deviceId());
    url.setQuery(query);
    return url;
}

void SyncPlayController::requestPlay()
{
    if (!m_api || m_groupId.isEmpty())
        return;
    QCoro::runDetached(m_api->syncPlayRequestPlay(),
                       []() {}, [](const std::exception_ptr &) {});
}

void SyncPlayController::requestSeekSeconds(double seconds)
{
    if (!m_api || m_groupId.isEmpty())
        return;
    const qint64 ticks = static_cast<qint64>(seconds * 10000000.0);
    QCoro::runDetached(m_api->syncPlayRequestSeek(ticks),
                       []() {}, [](const std::exception_ptr &) {});
}

} // namespace JellyfinNative
