#include "SyncPlayController.h"

#include "../api/JellyfinApiFacade.h"
#include "../common/AsyncTask.h"
#include "../common/JellyfinTypes.h"
#include "../player/PlayerController.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QTimeZone>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace JellyfinNative {

namespace {

    constexpr int kGreedyTimeSyncIntervalMs = 1'000;
    constexpr int kSteadyTimeSyncIntervalMs = 60'000;
    constexpr int kReconnectDelayMs = 3'000;
    constexpr int kDriftCorrectionThresholdMs = 400;
    constexpr int kDriftCorrectionCooldownMs = 3'000;
    constexpr qint64 kTicksPerSecond = 10'000'000;
    constexpr qint64 kTicksPerMillisecond = 10'000;

    qint64 dateTimeMs(const QJsonValue& value)
    {
        QDateTime time = QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
        if (!time.isValid())
            time = QDateTime::fromString(value.toString(), Qt::ISODate);
        return time.isValid() ? time.toUTC().toMSecsSinceEpoch() : 0;
    }

    QString groupErrorMessage(const QString& type)
    {
        if (type == QStringLiteral("GroupDoesNotExist"))
            return QStringLiteral("The SyncPlay group no longer exists.");
        if (type == QStringLiteral("LibraryAccessDenied"))
            return QStringLiteral("This account cannot access the SyncPlay media.");
        if (type == QStringLiteral("CreateGroupDenied"))
            return QStringLiteral("The server denied creating a SyncPlay group.");
        if (type == QStringLiteral("JoinGroupDenied"))
            return QStringLiteral("The server denied joining the SyncPlay group.");
        if (type == QStringLiteral("SyncPlayIsDisabled"))
            return QStringLiteral("SyncPlay is disabled on this server.");
        return QStringLiteral("SyncPlay update failed: %1").arg(type);
    }

} // namespace

SyncPlayController::SyncPlayController(JellyfinApiFacade *api, PlayerController *player, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_player(player)
{
    m_timeSyncTimer.setSingleShot(true);
    m_commandTimer.setSingleShot(true);
    m_reconnectTimer.setSingleShot(true);
    m_reconnectTimer.setInterval(kReconnectDelayMs);
    m_correctionTimer.setInterval(1'000);

    connect(&m_socket, &QWebSocket::connected, this, [this]() {
        qInfo() << "syncplay: websocket connected";
        m_reconnectTimer.stop();
        beginTimeSync();
        m_correctionTimer.start();
        emit connectionChanged();
    });
    connect(&m_socket, &QWebSocket::disconnected, this, [this]() {
        qInfo() << "syncplay: websocket disconnected";
        m_timeSyncTimer.stop();
        m_correctionTimer.stop();
        emit connectionChanged();
        if (m_socketDesired)
            m_reconnectTimer.start();
    });
    connect(&m_socket, &QWebSocket::textMessageReceived, this, &SyncPlayController::handleSocketTextMessage);
    connect(&m_socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit errorText(QStringLiteral("SyncPlay socket error: %1").arg(m_socket.errorString()));
    });
    connect(&m_timeSyncTimer, &QTimer::timeout, this, &SyncPlayController::requestTimeSync);
    connect(&m_commandTimer, &QTimer::timeout, this, &SyncPlayController::executeScheduledCommand);
    connect(&m_correctionTimer, &QTimer::timeout, this, &SyncPlayController::correctPlaybackDrift);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &SyncPlayController::connectSocket);
    if (m_player) {
        connect(m_player, &PlayerController::playbackStateChanged, this, &SyncPlayController::handlePlayerStateChanged);
    }
}

bool SyncPlayController::socketConnected() const
{
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

void SyncPlayController::refreshGroups()
{
    if (!m_api)
        return;
    connectSocket();
    Async::runScoped(
        this, m_api->fetchSyncPlayGroups(),
        [this](const QJsonArray& groups) {
            m_groups = groups;
            if (!m_groupId.isEmpty()) {
                for (const QJsonValue& value : groups) {
                    const QJsonObject group = value.toObject();
                    if (group.value(QStringLiteral("GroupId")).toString() == m_groupId) {
                        applyGroupInfo(m_groupId, group);
                        break;
                    }
                }
            }
            emit groupsChanged();
        },
        [this](const std::exception_ptr& error) { reportRequestError(QStringLiteral("refresh groups"), error); });
}

void SyncPlayController::connectSocket()
{
    m_socketDesired = true;
    if (!m_api || m_api->session().accessToken.isEmpty() || m_api->serverUrl().isEmpty()) {
        return;
    }
    if (m_socket.state() == QAbstractSocket::ConnectedState) {
        if (!m_clock.ready())
            beginTimeSync();
        return;
    }
    if (m_socket.state() == QAbstractSocket::ConnectingState)
        return;

    QNetworkRequest request(socketUrl());
    request.setRawHeader("X-Emby-Token", m_api->session().accessToken.toUtf8());
    m_socket.open(request);
}

void SyncPlayController::disconnectSocket()
{
    m_socketDesired = false;
    m_reconnectTimer.stop();
    m_timeSyncTimer.stop();
    m_commandTimer.stop();
    m_correctionTimer.stop();
    m_clock.reset();
    m_timeSyncInFlight = false;
    if (m_socket.state() != QAbstractSocket::UnconnectedState)
        m_socket.close();
    clearGroup();
    m_groups = {};
    emit connectionChanged();
    emit groupsChanged();
}

void SyncPlayController::createGroup(const QString& name)
{
    if (!m_api)
        return;
    connectSocket();
    Async::runScoped(
        this, m_api->createSyncPlayGroup(name), [this]() { refreshGroups(); },
        [this](const std::exception_ptr& error) { reportRequestError(QStringLiteral("create group"), error); });
}

void SyncPlayController::joinGroup(const QString& groupId)
{
    if (!m_api || groupId.isEmpty())
        return;
    connectSocket();
    Async::runScoped(
        this, m_api->joinSyncPlayGroup(groupId),
        [this, groupId]() {
            m_groupId = groupId;
            for (const QJsonValue& value : m_groups) {
                const QJsonObject group = value.toObject();
                if (group.value(QStringLiteral("GroupId")).toString() == groupId) {
                    applyGroupInfo(groupId, group);
                    break;
                }
            }
            m_joinedAtServerMs
                = QDateTime::currentMSecsSinceEpoch() + static_cast<qint64>(std::llround(m_clock.offsetMs()));
            emit groupChanged();
            sendPlayerBufferingState(true);
        },
        [this](const std::exception_ptr& error) { reportRequestError(QStringLiteral("join group"), error); });
}

void SyncPlayController::leaveGroup()
{
    if (!m_api || m_groupId.isEmpty())
        return;
    Async::runScoped(
        this, m_api->leaveSyncPlayGroup(), [this]() { clearGroup(); },
        [this](const std::exception_ptr& error) { reportRequestError(QStringLiteral("leave group"), error); });
}

void SyncPlayController::requestPause()
{
    if (!m_api || m_groupId.isEmpty())
        return;
    Async::runScoped(
        this, m_api->syncPlayRequestPause(), []() {},
        [this](const std::exception_ptr& error) { reportRequestError(QStringLiteral("pause"), error); },
        "SyncPlay pause request");
}

void SyncPlayController::requestPlay()
{
    if (!m_api || m_groupId.isEmpty())
        return;
    Async::runScoped(
        this, m_api->syncPlayRequestPlay(), []() {},
        [this](const std::exception_ptr& error) { reportRequestError(QStringLiteral("play"), error); },
        "SyncPlay play request");
}

void SyncPlayController::requestSeekSeconds(double seconds)
{
    if (!m_api || m_groupId.isEmpty())
        return;
    const qint64 ticks = static_cast<qint64>(std::max(0.0, seconds) * kTicksPerSecond);
    Async::runScoped(
        this, m_api->syncPlayRequestSeek(ticks), []() {},
        [this](const std::exception_ptr& error) { reportRequestError(QStringLiteral("seek"), error); },
        "SyncPlay seek request");
}

void SyncPlayController::handleSocketTextMessage(const QString& message)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(message.toUtf8(), &parseError);
    if (!document.isObject()) {
        qWarning() << "syncplay: invalid websocket message" << parseError.errorString();
        return;
    }

    const QJsonObject object = document.object();
    const QString type = object.value(QStringLiteral("MessageType")).toString();
    if (type == QStringLiteral("ForceKeepAlive")) {
        sendKeepAlive();
        return;
    }
    if (type == QStringLiteral("KeepAlive"))
        return;

    const QJsonObject data = object.value(QStringLiteral("Data")).toObject();
    if (type == QStringLiteral("SyncPlayCommand"))
        handleSyncPlayCommand(data);
    else if (type == QStringLiteral("SyncPlayGroupUpdate"))
        handleSyncPlayGroupUpdate(data);
}

void SyncPlayController::handleSyncPlayCommand(const QJsonObject& data)
{
    if (!m_player || m_groupId.isEmpty())
        return;

    const QString command = data.value(QStringLiteral("Command")).toString();
    const QString groupId = data.value(QStringLiteral("GroupId")).toString();
    if (!groupId.isEmpty() && groupId != m_groupId)
        return;

    const qint64 emittedAtMs = dateTimeMs(data.value(QStringLiteral("EmittedAt")));
    if (emittedAtMs > 0 && m_joinedAtServerMs > 0 && emittedAtMs < m_joinedAtServerMs) {
        qInfo() << "syncplay: ignoring command emitted before group join";
        return;
    }

    const qint64 serverTimeMs = dateTimeMs(data.value(QStringLiteral("When")));
    const qint64 positionTicks = data.value(QStringLiteral("PositionTicks")).toVariant().toLongLong();
    const QString playlistItemId = data.value(QStringLiteral("PlaylistItemId")).toString();
    if (!playlistItemId.isEmpty())
        m_playlistItemId = playlistItemId;

    const QString key
        = QStringLiteral("%1|%2|%3|%4").arg(command).arg(serverTimeMs).arg(positionTicks).arg(m_playlistItemId);
    if (key == m_lastCommandKey && m_commandTimer.isActive())
        return;

    m_lastCommandKey = key;
    m_scheduledCommand = command;
    m_scheduledPositionTicks = positionTicks;
    m_scheduledServerTimeMs = serverTimeMs;
    m_commandTimer.stop();

    const qint64 localNowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 delayMs = serverTimeMs > 0 ? (m_clock.ready() ? m_clock.localDelayUntil(serverTimeMs, localNowMs)
                                                               : std::max<qint64>(0, serverTimeMs - localNowMs))
                                            : 0;
    m_commandTimer.start(static_cast<int>(std::min<qint64>(delayMs, std::numeric_limits<int>::max())));
}

void SyncPlayController::handleSyncPlayGroupUpdate(const QJsonObject& data)
{
    const QString type = data.value(QStringLiteral("Type")).toString();
    const QString groupId = data.value(QStringLiteral("GroupId")).toString();
    const QJsonValue updateData = data.value(QStringLiteral("Data"));

    if (type == QStringLiteral("GroupJoined") || type == QStringLiteral("GroupUpdate")) {
        applyGroupInfo(groupId, updateData.toObject());
        m_joinedAtServerMs
            = QDateTime::currentMSecsSinceEpoch() + static_cast<qint64>(std::llround(m_clock.offsetMs()));
        refreshGroups();
        sendPlayerBufferingState(true);
        return;
    }
    if (type == QStringLiteral("UserJoined")) {
        const QString participant = updateData.toString();
        if (!participant.isEmpty() && !m_participants.contains(participant)) {
            m_participants.push_back(participant);
            emit groupChanged();
        }
        return;
    }
    if (type == QStringLiteral("UserLeft")) {
        m_participants.removeAll(updateData.toString());
        emit groupChanged();
        return;
    }
    if (type == QStringLiteral("StateUpdate")) {
        m_groupState = updateData.toObject().value(QStringLiteral("State")).toString();
        emit groupChanged();
        return;
    }
    if (type == QStringLiteral("PlayQueue")) {
        const QJsonObject queue = updateData.toObject();
        const QJsonArray playlist = queue.value(QStringLiteral("Playlist")).toArray();
        const int playingIndex = queue.value(QStringLiteral("PlayingItemIndex")).toInt(-1);
        if (playingIndex >= 0 && playingIndex < playlist.size()) {
            m_playlistItemId = playlist.at(playingIndex).toObject().value(QStringLiteral("PlaylistItemId")).toString();
        }
        return;
    }
    if (type == QStringLiteral("GroupLeft") || type == QStringLiteral("NotInGroup")) {
        clearGroup();
        return;
    }
    if (type == QStringLiteral("GroupDoesNotExist") || type == QStringLiteral("LibraryAccessDenied")
        || type == QStringLiteral("CreateGroupDenied") || type == QStringLiteral("JoinGroupDenied")
        || type == QStringLiteral("SyncPlayIsDisabled")) {
        emit errorText(groupErrorMessage(type));
        return;
    }
    qWarning() << "syncplay: unhandled group update" << type;
}

void SyncPlayController::applyGroupInfo(const QString& groupId, const QJsonObject& info)
{
    const QString infoGroupId = info.value(QStringLiteral("GroupId")).toString();
    m_groupId = !infoGroupId.isEmpty() ? infoGroupId : groupId;
    m_groupName = info.value(QStringLiteral("GroupName")).toString();
    if (m_groupName.isEmpty())
        m_groupName = QStringLiteral("SyncPlay group");
    m_groupState = info.value(QStringLiteral("State")).toString();

    QStringList participants;
    for (const QJsonValue& value : info.value(QStringLiteral("Participants")).toArray()) {
        const QString participant = value.toString();
        if (!participant.isEmpty())
            participants.push_back(participant);
    }
    m_participants = participants;
    emit groupChanged();
}

void SyncPlayController::clearGroup()
{
    m_commandTimer.stop();
    m_groupId.clear();
    m_groupName.clear();
    m_groupState.clear();
    m_participants.clear();
    m_playlistItemId.clear();
    m_lastCommandKey.clear();
    m_scheduledCommand.clear();
    m_joinedAtServerMs = 0;
    m_playerStateKnown = false;
    emit groupChanged();
}

void SyncPlayController::executeScheduledCommand()
{
    if (!m_player || m_scheduledCommand.isEmpty())
        return;

    const QString command = m_scheduledCommand;
    qint64 positionTicks = m_scheduledPositionTicks;
    if (command == QStringLiteral("Unpause") && m_scheduledServerTimeMs > 0) {
        positionTicks = m_clock.estimatePositionTicks(
            positionTicks, m_scheduledServerTimeMs, QDateTime::currentMSecsSinceEpoch());
    }
    const double targetSeconds = static_cast<double>(positionTicks) / kTicksPerSecond;
    const double positionDelta = std::abs(m_player->positionSeconds() - targetSeconds);

    if (command == QStringLiteral("Pause")) {
        if (!m_player->paused())
            m_player->togglePause();
        if (positionDelta > 0.1)
            m_player->seek(targetSeconds);
    } else if (command == QStringLiteral("Unpause")) {
        if (positionDelta * 1'000.0 > kDriftCorrectionThresholdMs && !m_player->seeking()) {
            m_player->seek(targetSeconds);
        }
        if (m_player->paused())
            m_player->togglePause();
    } else if (command == QStringLiteral("Seek")) {
        if (!m_player->paused())
            m_player->togglePause();
        m_player->seek(targetSeconds);
        QTimer::singleShot(250, this, [this]() { sendPlayerBufferingState(true); });
    } else if (command == QStringLiteral("Stop")) {
        m_player->stopWithReason(QStringLiteral("syncplay-stop"));
    } else {
        emit errorText(QStringLiteral("Unsupported SyncPlay command: %1").arg(command));
    }
}

void SyncPlayController::correctPlaybackDrift()
{
    if (!enabled() || !m_player || !m_player->visible() || m_scheduledCommand != QStringLiteral("Unpause")
        || m_scheduledServerTimeMs <= 0 || m_player->paused() || m_player->buffering() || m_player->seeking()) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs - m_lastCorrectionAtMs < kDriftCorrectionCooldownMs)
        return;

    const qint64 expectedTicks
        = m_clock.estimatePositionTicks(m_scheduledPositionTicks, m_scheduledServerTimeMs, nowMs);
    const qint64 actualTicks = static_cast<qint64>(m_player->positionSeconds() * kTicksPerSecond);
    const qint64 diffMs = std::abs(expectedTicks - actualTicks) / kTicksPerMillisecond;
    if (diffMs < kDriftCorrectionThresholdMs)
        return;

    m_lastCorrectionAtMs = nowMs;
    qInfo() << "syncplay: correcting playback drift" << diffMs << "ms";
    m_player->seek(static_cast<double>(expectedTicks) / kTicksPerSecond);
}

void SyncPlayController::handlePlayerStateChanged()
{
    sendPlayerBufferingState(false);
}

void SyncPlayController::sendPlayerBufferingState(bool force)
{
    if (!enabled() || !m_api || !m_player || !m_player->visible())
        return;

    const bool buffering = m_player->buffering() || m_player->seeking();
    if (!force && m_playerStateKnown && buffering == m_lastPlayerBuffering) {
        return;
    }
    m_playerStateKnown = true;
    m_lastPlayerBuffering = buffering;

    const qint64 positionTicks = static_cast<qint64>(m_player->positionSeconds() * kTicksPerSecond);
    const bool playing = !m_player->paused();
    const qint64 serverNowMs
        = QDateTime::currentMSecsSinceEpoch() + static_cast<qint64>(std::llround(m_clock.offsetMs()));
    Async::runScoped(
        this,
        m_api->syncPlayReportBuffering(buffering, positionTicks, playing, m_playlistItemId,
            QDateTime::fromMSecsSinceEpoch(serverNowMs, QTimeZone(QTimeZone::UTC))),
        []() {},
        [this, buffering](const std::exception_ptr& error) {
            reportRequestError(buffering ? QStringLiteral("report buffering") : QStringLiteral("report ready"), error);
        },
        buffering ? "SyncPlay buffering report" : "SyncPlay ready report");
}

void SyncPlayController::beginTimeSync()
{
    m_timeSyncTimer.stop();
    m_clock.reset();
    m_greedyTimeSyncRemaining = 3;
    m_timeSyncErrorReported = false;
    requestTimeSync();
}

void SyncPlayController::requestTimeSync()
{
    if (!m_api || !socketConnected() || m_timeSyncInFlight)
        return;

    m_timeSyncInFlight = true;
    const qint64 requestSentMs = QDateTime::currentMSecsSinceEpoch();
    Async::runScoped(
        this, m_api->fetchUtcTime(),
        [this, requestSentMs](const QJsonObject& response) {
            m_timeSyncInFlight = false;
            const qint64 responseReceivedMs = QDateTime::currentMSecsSinceEpoch();
            const qint64 requestReceivedMs = dateTimeMs(response.value(QStringLiteral("RequestReceptionTime")));
            const qint64 responseSentMs = dateTimeMs(response.value(QStringLiteral("ResponseTransmissionTime")));
            if (requestReceivedMs <= 0 || responseSentMs <= 0) {
                if (!m_timeSyncErrorReported) {
                    m_timeSyncErrorReported = true;
                    emit errorText(QStringLiteral("SyncPlay time response was incomplete."));
                }
                scheduleTimeSync();
                return;
            }

            m_clock.addMeasurement({ requestSentMs, requestReceivedMs, responseSentMs, responseReceivedMs });
            m_timeSyncErrorReported = false;
            emit connectionChanged();

            Async::runScoped(
                this, m_api->syncPlayReportPing(static_cast<qint64>(std::llround(m_clock.pingMs()))), []() {},
                [this](const std::exception_ptr& error) { reportRequestError(QStringLiteral("report ping"), error); },
                "SyncPlay ping report");
            scheduleTimeSync();
        },
        [this](const std::exception_ptr& error) {
            m_timeSyncInFlight = false;
            if (!m_timeSyncErrorReported) {
                m_timeSyncErrorReported = true;
                reportRequestError(QStringLiteral("synchronize clock"), error);
            }
            scheduleTimeSync();
        },
        "SyncPlay time sync");
}

void SyncPlayController::scheduleTimeSync()
{
    if (!socketConnected())
        return;
    const int interval = m_greedyTimeSyncRemaining > 0 ? kGreedyTimeSyncIntervalMs : kSteadyTimeSyncIntervalMs;
    m_greedyTimeSyncRemaining = std::max(0, m_greedyTimeSyncRemaining - 1);
    m_timeSyncTimer.start(interval);
}

void SyncPlayController::reportRequestError(const QString& action, const std::exception_ptr& error)
{
    const QString message = exceptionMessage(error);
    qWarning() << "syncplay:" << action << "failed:" << message;
    emit errorText(QStringLiteral("SyncPlay %1 failed: %2").arg(action, message));
}

void SyncPlayController::sendKeepAlive()
{
    if (socketConnected()) {
        m_socket.sendTextMessage(QStringLiteral(R"({"MessageType":"KeepAlive"})"));
    }
}

QUrl SyncPlayController::socketUrl() const
{
    QUrl url(m_api->serverUrl());
    url.setScheme(url.scheme() == QStringLiteral("https") ? QStringLiteral("wss") : QStringLiteral("ws"));
    QString path = url.path();
    while (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    url.setPath(path + QStringLiteral("/socket"));

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("api_key"), m_api->session().accessToken);
    if (!m_api->deviceId().isEmpty()) {
        query.addQueryItem(QStringLiteral("deviceId"), m_api->deviceId());
    }
    url.setQuery(query);
    return url;
}

} // namespace JellyfinNative
