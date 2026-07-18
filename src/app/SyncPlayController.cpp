#include "SyncPlayController.h"

#include "../api/JellyfinApiFacade.h"
#include "../common/AsyncTask.h"
#include "../common/JellyfinTypes.h"
#include "../common/TlsTrust.h"
#include "../player/PlayQueueController.h"
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
    constexpr int kSpeedCorrectionMinDriftMs = 60;
    constexpr int kSpeedCorrectionBaseDurationMs = 1'500;
    constexpr int kSpeedCorrectionMinimumDurationMs = 1'000;
    constexpr int kSpeedCorrectionMaximumDurationMs = 7'500;
    constexpr double kSpeedCorrectionMaximumDelta = 0.10;
    constexpr int kDriftCorrectionThresholdMs = 750;
    constexpr int kDriftCorrectionCooldownMs = 5'000;
    constexpr int kInternalSeekBufferingSuppressionMs = 3'000;
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

SyncPlayController::SyncPlayController(JellyfinApiFacade *api, PlayerController *player, PlayQueueController *playQueue,
    TlsTrustController *tlsTrust, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_player(player)
    , m_playQueue(playQueue)
{
    m_timeSyncTimer.setSingleShot(true);
    m_commandTimer.setSingleShot(true);
    m_reconnectTimer.setSingleShot(true);
    m_reconnectTimer.setInterval(kReconnectDelayMs);
    // jellyfin-mpv-shim waits one second before declaring buffering. mpv's
    // seeking flag is often only a short command transition, not starvation.
    m_bufferingDebounceTimer.setSingleShot(true);
    m_bufferingDebounceTimer.setInterval(1'000);
    m_speedCorrectionTimer.setSingleShot(true);
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
    if (tlsTrust)
        tlsTrust->attachWebSocket(&m_socket, [this]() { return socketUrl(); }, QStringLiteral("SyncPlay"));
    connect(&m_socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit errorText(QStringLiteral("SyncPlay socket error: %1").arg(m_socket.errorString()));
    });
    connect(&m_timeSyncTimer, &QTimer::timeout, this, &SyncPlayController::requestTimeSync);
    connect(&m_commandTimer, &QTimer::timeout, this, &SyncPlayController::executeScheduledCommand);
    connect(&m_correctionTimer, &QTimer::timeout, this, &SyncPlayController::correctPlaybackDrift);
    connect(&m_speedCorrectionTimer, &QTimer::timeout, this, &SyncPlayController::finishSpeedCorrection);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &SyncPlayController::connectSocket);
    connect(&m_bufferingDebounceTimer, &QTimer::timeout, this, [this]() { sendPlayerBufferingState(true); });
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
            if (m_player)
                m_player->setSyncPlaybackSpeed(1.0);
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

void SyncPlayController::requestUnpauseWhenReady()
{
    if (!enabled())
        return;
    m_unpauseWhenReady = true;
    setWaitingForGroupPlayback(true);
    sendPendingUnpause();
}

void SyncPlayController::requestTogglePause()
{
    if (!enabled() || !m_api || !m_player || !m_player->sessionActive())
        return;

    if (m_player->paused()) {
        if (m_unpauseRequestPending)
            return;
        m_unpauseRequestPending = true;
        setWaitingForGroupPlayback(true);
        Async::runScoped(
            this, m_api->syncPlayUnpause(), []() {},
            [this](const std::exception_ptr& error) {
                m_unpauseRequestPending = false;
                setWaitingForGroupPlayback(false);
                reportRequestError(QStringLiteral("unpause group"), error);
            },
            "SyncPlay unpause request");
    } else {
        // Match jellyfin-mpv-shim: pause locally immediately, then let the
        // scheduled server command establish the authoritative position.
        m_player->togglePause();
        Async::runScoped(
            this, m_api->syncPlayPause(), []() {},
            [this](const std::exception_ptr& error) { reportRequestError(QStringLiteral("pause group"), error); },
            "SyncPlay pause request");
    }
}

void SyncPlayController::requestSeek(double positionSeconds)
{
    if (!enabled() || !m_api || !std::isfinite(positionSeconds))
        return;
    const qint64 ticks = static_cast<qint64>(std::max(0.0, positionSeconds) * kTicksPerSecond);
    Async::runScoped(
        this, m_api->syncPlaySeek(ticks), []() {},
        [this](const std::exception_ptr& error) { reportRequestError(QStringLiteral("seek group"), error); },
        "SyncPlay seek request");
}

void SyncPlayController::requestRelativeSeek(double deltaSeconds)
{
    if (!m_player)
        return;
    requestSeek(m_player->positionSeconds() + deltaSeconds);
}

void SyncPlayController::requestNextItem()
{
    if (!enabled() || !m_api || m_playlistItemId.isEmpty())
        return;
    Async::runScoped(
        this, m_api->syncPlayNextItem(m_playlistItemId), []() {},
        [this](const std::exception_ptr& error) { reportRequestError(QStringLiteral("next group item"), error); },
        "SyncPlay next item request");
}

void SyncPlayController::requestPreviousItem()
{
    if (!enabled() || !m_api || m_playlistItemId.isEmpty())
        return;
    Async::runScoped(
        this, m_api->syncPlayPreviousItem(m_playlistItemId), []() {},
        [this](const std::exception_ptr& error) { reportRequestError(QStringLiteral("previous group item"), error); },
        "SyncPlay previous item request");
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
    else if (type == QStringLiteral("Play"))
        emit remotePlayCommand(data);
    else if (type == QStringLiteral("Playstate"))
        emit remotePlaystateCommand(data);
    else if (type == QStringLiteral("GeneralCommand"))
        emit remoteGeneralCommand(data);
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
    if (!playlistItemId.isEmpty() && !m_playlistItemId.isEmpty() && playlistItemId != m_playlistItemId) {
        qInfo() << "syncplay: ignoring command for stale playlist item" << playlistItemId;
        return;
    }
    m_scheduledPlaylistItemId = playlistItemId;

    const QString key
        = QStringLiteral("%1|%2|%3|%4").arg(command).arg(serverTimeMs).arg(positionTicks).arg(m_playlistItemId);
    if (key == m_lastCommandKey && m_commandTimer.isActive())
        return;

    m_lastCommandKey = key;
    m_scheduledCommand = command;
    m_scheduledPositionTicks = positionTicks;
    m_scheduledServerTimeMs = serverTimeMs;
    m_commandDue = false;
    m_commandTimer.stop();
    m_bufferingDebounceTimer.stop();
    if (command == QStringLiteral("Unpause"))
        setWaitingForGroupPlayback(true);

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
        const QJsonObject state = updateData.toObject();
        m_groupState = state.value(QStringLiteral("State")).toString();
        m_groupStateReason = state.value(QStringLiteral("Reason")).toString();
        if (m_groupState == QStringLiteral("Waiting") && m_groupStateReason == QStringLiteral("Unpause")) {
            setWaitingForGroupPlayback(true);
        } else if (m_groupState == QStringLiteral("Paused")
            && (m_groupStateReason == QStringLiteral("Ready") || m_groupStateReason == QStringLiteral("Pause"))
            && !m_unpauseRequestPending) {
            setWaitingForGroupPlayback(false);
        }
        emit groupChanged();
        return;
    }
    if (type == QStringLiteral("PlayQueue")) {
        applyPlayQueueUpdate(updateData.toObject());
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

void SyncPlayController::applyPlayQueueUpdate(const QJsonObject& queue)
{
    if (!m_api || !m_playQueue)
        return;

    const qint64 updatedAtMs = dateTimeMs(queue.value(QStringLiteral("LastUpdate")));
    if (updatedAtMs > 0 && updatedAtMs < m_lastPlayQueueUpdateMs) {
        qInfo() << "syncplay: ignoring stale play queue update";
        return;
    }
    m_lastPlayQueueUpdateMs = std::max(m_lastPlayQueueUpdateMs, updatedAtMs);

    const QJsonArray playlist = queue.value(QStringLiteral("Playlist")).toArray();
    const int playingIndex = queue.value(QStringLiteral("PlayingItemIndex")).toInt(-1);
    if (playlist.isEmpty() || playingIndex < 0 || playingIndex >= playlist.size()) {
        qWarning() << "syncplay: received empty or invalid play queue";
        return;
    }

    QStringList itemIds;
    QStringList playlistItemIds;
    itemIds.reserve(playlist.size());
    playlistItemIds.reserve(playlist.size());
    for (const QJsonValue& value : playlist) {
        const QJsonObject entry = value.toObject();
        itemIds.push_back(entry.value(QStringLiteral("ItemId")).toString());
        playlistItemIds.push_back(entry.value(QStringLiteral("PlaylistItemId")).toString());
    }
    if (itemIds.contains(QString())) {
        qWarning() << "syncplay: play queue contains an item without an id";
        return;
    }

    const quint64 generation = ++m_playQueueGeneration;
    m_playQueueLoading = true;
    m_playlistItemId = playlistItemIds.at(playingIndex);
    const QString selectedItemId = itemIds.at(playingIndex);
    const bool selectedItemAlreadyActive
        = m_player && m_player->sessionActive() && m_playQueue->currentItem().id == selectedItemId;
    if (!selectedItemAlreadyActive)
        setWaitingForGroupPlayback(true);
    const qint64 requestedTicks = queue.value(QStringLiteral("StartPositionTicks")).toVariant().toLongLong();
    const QString reason = queue.value(QStringLiteral("Reason")).toString();
    qInfo() << "syncplay: resolving incoming queue" << itemIds.size() << "items, index" << playingIndex << "reason"
            << reason;

    Async::runScoped(
        this, m_api->fetchItemsByIds(itemIds),
        [this, generation, itemIds, playlistItemIds, playingIndex, requestedTicks, selectedItemAlreadyActive](
            const std::vector<MovieItem>& fetched) {
            if (generation != m_playQueueGeneration)
                return;
            m_playQueueLoading = false;

            std::vector<MovieItem> ordered;
            ordered.reserve(static_cast<size_t>(itemIds.size()));
            int resolvedIndex = -1;
            for (int requestedIndex = 0; requestedIndex < itemIds.size(); ++requestedIndex) {
                const auto found = std::find_if(fetched.begin(), fetched.end(),
                    [&](const MovieItem& item) { return item.id == itemIds.at(requestedIndex); });
                if (found == fetched.end())
                    continue;
                MovieItem item = *found;
                item.playlistItemId = playlistItemIds.at(requestedIndex);
                if (requestedIndex == playingIndex)
                    resolvedIndex = static_cast<int>(ordered.size());
                ordered.push_back(std::move(item));
            }
            if (resolvedIndex < 0 || !m_playQueue->playNow(ordered, resolvedIndex)) {
                setWaitingForGroupPlayback(false);
                emit errorText(QStringLiteral("SyncPlay could not resolve the selected queue item."));
                return;
            }

            if (selectedItemAlreadyActive) {
                qInfo() << "syncplay: selected queue item is already active; preserving playback session";
                sendPlayerBufferingState(true);
                sendPendingUnpause();
                return;
            }

            m_waitingForPlaybackStart = true;
            m_playerStateKnown = false;
            prepareQueuePlayback(requestedTicks);
            emit queuePlaybackRequested(requestedTicks);
        },
        [this, generation](const std::exception_ptr& error) {
            if (generation != m_playQueueGeneration)
                return;
            m_playQueueLoading = false;
            setWaitingForGroupPlayback(false);
            reportRequestError(QStringLiteral("load play queue"), error);
        },
        "SyncPlay queue resolution");
}

void SyncPlayController::prepareQueuePlayback(qint64 positionTicks)
{
    if (!enabled() || !m_api)
        return;
    const qint64 serverNowMs
        = QDateTime::currentMSecsSinceEpoch() + static_cast<qint64>(std::llround(m_clock.offsetMs()));
    Async::runScoped(
        this,
        m_api->syncPlayReportBuffering(true, std::max<qint64>(0, positionTicks), false, m_playlistItemId,
            QDateTime::fromMSecsSinceEpoch(serverNowMs, QTimeZone(QTimeZone::UTC))),
        []() {},
        [this](const std::exception_ptr& error) { reportRequestError(QStringLiteral("report buffering"), error); },
        "SyncPlay queue buffering report");
}

void SyncPlayController::applyGroupInfo(const QString& groupId, const QJsonObject& info)
{
    const QString infoGroupId = info.value(QStringLiteral("GroupId")).toString();
    m_groupId = !infoGroupId.isEmpty() ? infoGroupId : groupId;
    m_groupName = info.value(QStringLiteral("GroupName")).toString();
    if (m_groupName.isEmpty())
        m_groupName = QStringLiteral("SyncPlay group");
    m_groupState = info.value(QStringLiteral("State")).toString();
    m_groupStateReason = info.value(QStringLiteral("Reason")).toString();
    if (m_player && !m_speedCorrectionActive)
        m_player->setSyncPlaybackSpeed(1.0);

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
    m_speedCorrectionTimer.stop();
    m_speedCorrectionActive = false;
    if (m_player)
        m_player->clearSyncPlaybackSpeed();
    m_groupId.clear();
    m_groupName.clear();
    m_groupState.clear();
    m_groupStateReason.clear();
    m_participants.clear();
    m_playlistItemId.clear();
    m_lastCommandKey.clear();
    m_scheduledCommand.clear();
    m_scheduledPlaylistItemId.clear();
    m_joinedAtServerMs = 0;
    m_lastPlayQueueUpdateMs = 0;
    ++m_playQueueGeneration;
    m_playQueueLoading = false;
    m_waitingForPlaybackStart = false;
    m_commandDue = false;
    m_unpauseWhenReady = false;
    m_unpauseRequestPending = false;
    m_syncCorrectionAttempts = 0;
    m_lastCorrectionAtMs = 0;
    setWaitingForGroupPlayback(false);
    setPlaybackDiff(0, false);
    setSyncMethod(QStringLiteral("None"));
    m_suppressSeekBufferingUntilMs = 0;
    m_playerStateKnown = false;
    emit groupChanged();
}

void SyncPlayController::executeScheduledCommand()
{
    if (!m_player || m_scheduledCommand.isEmpty())
        return;

    if (m_scheduledCommand != QStringLiteral("Stop")
        && (m_playQueueLoading || m_waitingForPlaybackStart || !m_player->sessionActive())) {
        m_commandDue = true;
        qInfo() << "syncplay: deferring" << m_scheduledCommand << "until playback is ready";
        return;
    }

    if (!m_scheduledPlaylistItemId.isEmpty() && !m_playlistItemId.isEmpty()
        && m_scheduledPlaylistItemId != m_playlistItemId) {
        qInfo() << "syncplay: dropping scheduled command for replaced queue item";
        m_commandDue = false;
        return;
    }
    m_commandDue = false;

    const QString command = m_scheduledCommand;
    qint64 positionTicks = m_scheduledPositionTicks;
    if (command == QStringLiteral("Unpause") && m_scheduledServerTimeMs > 0) {
        positionTicks = m_clock.estimatePositionTicks(
            positionTicks, m_scheduledServerTimeMs, QDateTime::currentMSecsSinceEpoch());
    }
    const double targetSeconds = static_cast<double>(positionTicks) / kTicksPerSecond;
    const double positionDelta = std::abs(m_player->positionSeconds() - targetSeconds);
    qInfo() << "syncplay: executing" << command << "positionTicks" << positionTicks << "deltaMs"
            << static_cast<qint64>(positionDelta * 1'000.0);

    if (command == QStringLiteral("Pause")) {
        finishSpeedCorrection();
        m_unpauseRequestPending = false;
        setWaitingForGroupPlayback(false);
        if (!m_player->paused())
            m_player->togglePause();
        if (positionDelta > 0.1)
            m_player->seek(targetSeconds);
    } else if (command == QStringLiteral("Unpause")) {
        finishSpeedCorrection();
        m_unpauseRequestPending = false;
        setWaitingForGroupPlayback(false);
        m_syncCorrectionAttempts = 0;
        m_lastCorrectionAtMs = 0;
        setPlaybackDiff(0, false);
        setSyncMethod(QStringLiteral("None"));
        if (positionDelta * 1'000.0 > kDriftCorrectionThresholdMs && !m_player->seeking()) {
            m_suppressSeekBufferingUntilMs = QDateTime::currentMSecsSinceEpoch() + kInternalSeekBufferingSuppressionMs;
            m_player->seek(targetSeconds);
        }
        if (m_player->paused())
            m_player->togglePause();
    } else if (command == QStringLiteral("Seek")) {
        finishSpeedCorrection();
        if (!m_player->paused())
            m_player->togglePause();
        m_player->seek(targetSeconds);
        QTimer::singleShot(250, this, [this]() { sendPlayerBufferingState(true); });
    } else if (command == QStringLiteral("Stop")) {
        finishSpeedCorrection();
        m_unpauseRequestPending = false;
        setWaitingForGroupPlayback(false);
        m_syncCorrectionAttempts = 0;
        setPlaybackDiff(0, false);
        setSyncMethod(QStringLiteral("None"));
        m_player->stopWithReason(QStringLiteral("syncplay-stop"));
    } else {
        emit errorText(QStringLiteral("Unsupported SyncPlay command: %1").arg(command));
    }
}

void SyncPlayController::correctPlaybackDrift()
{
    if (!enabled() || !m_player || !m_player->sessionActive() || m_scheduledCommand != QStringLiteral("Unpause")
        || m_scheduledServerTimeMs <= 0 || m_player->paused() || m_player->buffering() || m_player->seeking()) {
        setPlaybackDiff(0, false);
        if (m_speedCorrectionActive)
            finishSpeedCorrection();
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 expectedTicks
        = m_clock.estimatePositionTicks(m_scheduledPositionTicks, m_scheduledServerTimeMs, nowMs);
    const qint64 actualTicks = static_cast<qint64>(m_player->positionSeconds() * kTicksPerSecond);
    const qint64 signedDiffTicks = expectedTicks - actualTicks;
    const qint64 diffMs = std::abs(signedDiffTicks) / kTicksPerMillisecond;
    setPlaybackDiff(signedDiffTicks, true);
    if (diffMs < kSpeedCorrectionMinDriftMs) {
        m_syncCorrectionAttempts = 0;
        if (!m_speedCorrectionActive)
            setSyncMethod(QStringLiteral("None"));
        return;
    }

    if (m_speedCorrectionActive) {
        // A large discontinuity should not wait for the gentle rate
        // correction to finish; restore 1x before the authoritative seek.
        if (diffMs < kDriftCorrectionThresholdMs)
            return;
        finishSpeedCorrection();
    }

    // Keep diagnostics live during correction cooldowns even though another
    // correction is intentionally suppressed.
    if (nowMs - m_lastCorrectionAtMs < kDriftCorrectionCooldownMs)
        return;

    if (diffMs < kDriftCorrectionThresholdMs) {
        const double requestedDelta
            = static_cast<double>(signedDiffTicks) / kTicksPerMillisecond / kSpeedCorrectionBaseDurationMs;
        const double speedDelta
            = std::clamp(requestedDelta, -kSpeedCorrectionMaximumDelta, kSpeedCorrectionMaximumDelta);
        const double speed = 1.0 + speedDelta;
        const int durationMs = std::clamp(
            static_cast<int>(std::llround(
                std::abs(static_cast<double>(signedDiffTicks) / kTicksPerMillisecond) / std::abs(speedDelta))),
            kSpeedCorrectionMinimumDurationMs, kSpeedCorrectionMaximumDurationMs);

        m_lastCorrectionAtMs = nowMs;
        ++m_syncCorrectionAttempts;
        m_speedCorrectionActive = true;
        m_player->setSyncPlaybackSpeed(speed);
        setSyncMethod(QStringLiteral("SpeedToSync (x%1)").arg(speed, 0, 'f', 3));
        qInfo() << "syncplay: correcting playback drift by speed" << signedDiffTicks / kTicksPerMillisecond << "ms rate"
                << speed << "durationMs" << durationMs;
        m_speedCorrectionTimer.start(durationMs);
        return;
    }

    m_lastCorrectionAtMs = nowMs;
    ++m_syncCorrectionAttempts;
    setSyncMethod(QStringLiteral("SkipToSync (%1)").arg(m_syncCorrectionAttempts));
    qInfo() << "syncplay: correcting playback drift" << diffMs << "ms";
    m_suppressSeekBufferingUntilMs = nowMs + kInternalSeekBufferingSuppressionMs;
    m_player->seek(static_cast<double>(expectedTicks) / kTicksPerSecond);
}

void SyncPlayController::finishSpeedCorrection()
{
    m_speedCorrectionTimer.stop();
    if (m_player && enabled())
        m_player->setSyncPlaybackSpeed(1.0);
    if (!m_speedCorrectionActive)
        return;
    m_speedCorrectionActive = false;
    setSyncMethod(QStringLiteral("None"));
}

void SyncPlayController::handlePlayerStateChanged()
{
    if (m_waitingForPlaybackStart) {
        if (m_player && m_player->sessionActive() && m_player->fileLoaded()) {
            m_waitingForPlaybackStart = false;
            qInfo() << "syncplay: incoming queue file is loaded and paused; reporting ready";
            if (m_commandDue)
                executeScheduledCommand();
            sendPlayerBufferingState(true);
            sendPendingUnpause();
        }
        // prepareQueuePlayback() already reported this participant as
        // buffering. Do not let intermediate player state changes publish a
        // false Ready before mpv has actually loaded the file.
        return;
    }
    sendPlayerBufferingState(false);
    sendPendingUnpause();
}

void SyncPlayController::sendPlayerBufferingState(bool force)
{
    if (!enabled() || !m_api || !m_player || !m_player->sessionActive())
        return;

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const bool internalSeek = m_player->seeking() && nowMs < m_suppressSeekBufferingUntilMs;
    const bool buffering = internalSeek ? false : (m_player->buffering() || m_player->seeking());
    if (!force && buffering && (!m_playerStateKnown || !m_lastPlayerBuffering)) {
        if (!m_bufferingDebounceTimer.isActive())
            m_bufferingDebounceTimer.start();
        return;
    }
    if (!buffering)
        m_bufferingDebounceTimer.stop();
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

void SyncPlayController::sendPendingUnpause()
{
    if (!m_unpauseWhenReady || !enabled() || !m_api || !m_player || m_playQueueLoading || m_waitingForPlaybackStart
        || !m_player->sessionActive() || !m_player->fileLoaded()) {
        return;
    }

    m_unpauseWhenReady = false;
    m_unpauseRequestPending = true;
    qInfo() << "syncplay: requesting group-wide unpause";
    Async::runScoped(
        this, m_api->syncPlayUnpause(), []() {},
        [this](const std::exception_ptr& error) {
            m_unpauseRequestPending = false;
            setWaitingForGroupPlayback(false);
            reportRequestError(QStringLiteral("start group playback"), error);
        },
        "SyncPlay group unpause");
}

void SyncPlayController::setWaitingForGroupPlayback(bool waiting)
{
    if (m_waitingForGroupPlayback == waiting)
        return;
    m_waitingForGroupPlayback = waiting;
    emit syncStatusChanged();
}

void SyncPlayController::setPlaybackDiff(qint64 diffTicks, bool valid)
{
    const double diffMs = valid ? static_cast<double>(diffTicks) / kTicksPerMillisecond : 0.0;
    if (m_playbackDiffValid == valid && (!valid || qFuzzyCompare(m_playbackDiffMs, diffMs)))
        return;
    m_playbackDiffValid = valid;
    m_playbackDiffMs = diffMs;
    emit syncStatusChanged();
}

void SyncPlayController::setSyncMethod(const QString& method)
{
    if (m_syncMethod == method)
        return;
    m_syncMethod = method;
    emit syncStatusChanged();
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
    if (!m_api->deviceId().isEmpty()) {
        query.addQueryItem(QStringLiteral("deviceId"), m_api->deviceId());
    }
    url.setQuery(query);
    return url;
}

} // namespace JellyfinNative
