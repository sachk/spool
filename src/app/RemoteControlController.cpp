#include "RemoteControlController.h"

#include "../api/JellyfinApiFacade.h"
#include "../common/AsyncTask.h"
#include "../common/MetaJson.h"
#include "../platform/PlatformRemoteMediaSession.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonValue>

#include <algorithm>

namespace JellyfinNative {
namespace {

    qint64 jsonInteger(const QJsonValue& value)
    {
        return value.toVariant().toLongLong();
    }

    QString streamLabel(const QJsonObject& stream)
    {
        QString label = stream.value(QStringLiteral("DisplayTitle")).toString().trimmed();
        if (!label.isEmpty())
            return label;
        QStringList parts;
        const QString language = stream.value(QStringLiteral("Language")).toString().trimmed();
        const QString codec = stream.value(QStringLiteral("Codec")).toString().trimmed().toUpper();
        if (!language.isEmpty())
            parts.push_back(language);
        if (!codec.isEmpty())
            parts.push_back(codec);
        return parts.isEmpty() ? QStringLiteral("Track %1").arg(stream.value(QStringLiteral("Index")).toInt() + 1)
                               : parts.join(QStringLiteral(" · "));
    }

    QVariantMap normalizedNowPlayingItem(const QJsonObject& object)
    {
        if (object.isEmpty())
            return {};
        return metaToJson(metaFromJson<MovieItem>(object, MetaJsonKeyPolicy::PascalCase)).toVariantMap();
    }

} // namespace

RemoteControlController::RemoteControlController(JellyfinApiFacade *api, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_mediaSession(createPlatformRemoteMediaSession(this))
{
    m_refreshTimer.setInterval(5'000);
    connect(&m_refreshTimer, &QTimer::timeout, this, &RemoteControlController::refreshTargets);

    connect(m_mediaSession.get(), &PlatformRemoteMediaSession::playRequested, this,
        [this]() { sendPlaystate(QStringLiteral("Unpause")); });
    connect(m_mediaSession.get(), &PlatformRemoteMediaSession::pauseRequested, this,
        [this]() { sendPlaystate(QStringLiteral("Pause")); });
    connect(m_mediaSession.get(), &PlatformRemoteMediaSession::playPauseRequested, this,
        &RemoteControlController::togglePause);
    connect(
        m_mediaSession.get(), &PlatformRemoteMediaSession::stopRequested, this, &RemoteControlController::stopPlayback);
    connect(
        m_mediaSession.get(), &PlatformRemoteMediaSession::nextRequested, this, &RemoteControlController::nextTrack);
    connect(m_mediaSession.get(), &PlatformRemoteMediaSession::previousRequested, this,
        &RemoteControlController::previousTrack);
    connect(m_mediaSession.get(), &PlatformRemoteMediaSession::seekRequested, this,
        [this](qint64 positionMs) { seek(positionMs * 10'000); });
    connect(m_mediaSession.get(), &PlatformRemoteMediaSession::seekRelativeRequested, this,
        [this](qint64 deltaMs) { seekRelative(deltaMs * 10'000); });
    connect(
        m_mediaSession.get(), &PlatformRemoteMediaSession::volumeRequested, this, &RemoteControlController::setVolume);
}

RemoteControlController::~RemoteControlController() = default;

void RemoteControlController::start()
{
    if (!m_refreshTimer.isActive())
        m_refreshTimer.start();
    refreshTargets();
}

void RemoteControlController::stop()
{
    m_refreshTimer.stop();
    m_sessions.clear();
    m_targets.clear();
    clearTarget();
    emit targetsChanged();
}

void RemoteControlController::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void RemoteControlController::refreshTargets()
{
    if (!m_api || m_api->session().accessToken.isEmpty() || m_busy)
        return;
    setBusy(true);
    Async::runScoped(
        this, m_api->fetchControllableSessions(),
        [this](const QJsonArray& sessions) {
            setBusy(false);
            applySessions(sessions);
        },
        [this](const std::exception_ptr& error) {
            setBusy(false);
            reportCommandError(QStringLiteral("refresh remote targets"), error);
        },
        "remote target refresh");
}

void RemoteControlController::applySessions(const QJsonArray& sessions)
{
    QHash<QString, QJsonObject> nextSessions;
    QVariantList nextTargets;
    const QString localDeviceId = m_api ? m_api->deviceId() : QString();

    for (const QJsonValue& value : sessions) {
        const QJsonObject session = value.toObject();
        const QString id = session.value(QStringLiteral("Id")).toString();
        if (id.isEmpty() || session.value(QStringLiteral("DeviceId")).toString() == localDeviceId)
            continue;
        const QJsonObject capabilities = session.value(QStringLiteral("Capabilities")).toObject();
        if (!capabilities.value(QStringLiteral("SupportsMediaControl")).toBool(false))
            continue;

        nextSessions.insert(id, session);
        const QJsonObject nowPlaying = session.value(QStringLiteral("NowPlayingItem")).toObject();
        nextTargets.push_back(QVariantMap {
            { QStringLiteral("sessionId"), id },
            { QStringLiteral("deviceName"), session.value(QStringLiteral("DeviceName")).toString() },
            { QStringLiteral("deviceType"), session.value(QStringLiteral("DeviceType")).toString() },
            { QStringLiteral("client"), session.value(QStringLiteral("Client")).toString() },
            { QStringLiteral("userName"), session.value(QStringLiteral("UserName")).toString() },
            { QStringLiteral("nowPlayingTitle"), nowPlaying.value(QStringLiteral("Name")).toString() },
            { QStringLiteral("appVersion"), session.value(QStringLiteral("ApplicationVersion")).toString() },
        });
    }

    std::sort(nextTargets.begin(), nextTargets.end(), [](const QVariant& left, const QVariant& right) {
        return QString::localeAwareCompare(left.toMap().value(QStringLiteral("deviceName")).toString(),
                   right.toMap().value(QStringLiteral("deviceName")).toString())
            < 0;
    });

    m_sessions = std::move(nextSessions);
    m_targets = std::move(nextTargets);
    qInfo() << "remote control: discovered" << m_targets.size() << "controllable client(s)";
    emit targetsChanged();

    if (!m_selectedSessionId.isEmpty() && !m_sessions.contains(m_selectedSessionId)) {
        clearTarget();
        return;
    }
    if (!m_selectedSessionId.isEmpty())
        applySelectedSession();
}

void RemoteControlController::selectTarget(const QString& sessionId)
{
    const QString normalized = sessionId.trimmed();
    if (!m_sessions.contains(normalized)) {
        emit errorText(QStringLiteral("That remote client is no longer available."));
        refreshTargets();
        return;
    }
    if (m_selectedSessionId == normalized) {
        applySelectedSession();
        return;
    }
    m_selectedSessionId = normalized;
    applySelectedSession();
    emit targetChanged();
}

void RemoteControlController::clearTarget()
{
    if (m_selectedSessionId.isEmpty() && m_selectedTargetName.isEmpty()) {
        clearSelectedState();
        return;
    }
    m_selectedSessionId.clear();
    m_selectedTargetName.clear();
    m_selectedTargetDetail.clear();
    clearSelectedState();
    emit targetChanged();
}

void RemoteControlController::clearSelectedState()
{
    m_queueGeneration.invalidate();
    m_nowPlayingItem.clear();
    m_queue.clear();
    m_audioTracks.clear();
    m_subtitleTracks.clear();
    m_supportedCommands.clear();
    m_rawQueue = {};
    m_positionTicks = 0;
    m_runtimeTicks = 0;
    m_stateReceivedAtMs = 0;
    m_playbackRate = 1.0;
    m_volume = 100;
    m_paused = true;
    m_muted = false;
    m_shuffled = false;
    m_repeatMode = QStringLiteral("RepeatNone");
    if (m_mediaSession)
        m_mediaSession->clear();
    emit stateChanged();
}

void RemoteControlController::applySelectedSession()
{
    const QJsonObject session = m_sessions.value(m_selectedSessionId);
    if (session.isEmpty())
        return;

    m_selectedTargetName = session.value(QStringLiteral("DeviceName")).toString().trimmed();
    if (m_selectedTargetName.isEmpty())
        m_selectedTargetName = session.value(QStringLiteral("Client")).toString().trimmed();
    const QString client = session.value(QStringLiteral("Client")).toString().trimmed();
    const QString user = session.value(QStringLiteral("UserName")).toString().trimmed();
    QStringList detailParts;
    if (!client.isEmpty())
        detailParts.push_back(client);
    if (!user.isEmpty())
        detailParts.push_back(user);
    m_selectedTargetDetail = detailParts.join(QStringLiteral(" · "));

    const QJsonObject nowPlaying = session.value(QStringLiteral("NowPlayingItem")).toObject();
    const QJsonObject playState = session.value(QStringLiteral("PlayState")).toObject();
    m_nowPlayingItem = normalizedNowPlayingItem(nowPlaying);
    m_positionTicks = jsonInteger(playState.value(QStringLiteral("PositionTicks")));
    m_runtimeTicks = jsonInteger(nowPlaying.value(QStringLiteral("RunTimeTicks")));
    m_stateReceivedAtMs = QDateTime::currentMSecsSinceEpoch();
    m_playbackRate = playState.value(QStringLiteral("PlaybackRate")).toDouble(1.0);
    m_paused = playState.value(QStringLiteral("IsPaused")).toBool(true);
    m_muted = playState.value(QStringLiteral("IsMuted")).toBool(false);
    m_volume = std::clamp(playState.value(QStringLiteral("VolumeLevel")).toInt(100), 0, 100);
    m_repeatMode = playState.value(QStringLiteral("RepeatMode")).toString(QStringLiteral("RepeatNone"));
    m_shuffled = playState.value(QStringLiteral("PlaybackOrder")).toString() == QStringLiteral("Shuffle");

    const QJsonArray commands
        = session.value(QStringLiteral("Capabilities")).toObject().value(QStringLiteral("SupportedCommands")).toArray();
    m_supportedCommands.clear();
    m_supportedCommands.reserve(commands.size());
    for (const QJsonValue& command : commands) {
        const QString name = command.toString();
        if (!name.isEmpty())
            m_supportedCommands.push_back(name);
    }

    const int selectedAudio = playState.value(QStringLiteral("AudioStreamIndex")).toInt(-1);
    const int selectedSubtitle = playState.value(QStringLiteral("SubtitleStreamIndex")).toInt(-1);
    m_audioTracks.clear();
    m_subtitleTracks.clear();
    for (const QJsonValue& value : nowPlaying.value(QStringLiteral("MediaStreams")).toArray()) {
        const QJsonObject stream = value.toObject();
        const int index = stream.value(QStringLiteral("Index")).toInt(-1);
        QVariantMap row {
            { QStringLiteral("index"), index },
            { QStringLiteral("label"), streamLabel(stream) },
            { QStringLiteral("selected"), false },
        };
        if (stream.value(QStringLiteral("Type")).toString() == QStringLiteral("Audio")) {
            row.insert(QStringLiteral("selected"), index == selectedAudio);
            m_audioTracks.push_back(row);
        } else if (stream.value(QStringLiteral("Type")).toString() == QStringLiteral("Subtitle")) {
            row.insert(QStringLiteral("selected"), index == selectedSubtitle);
            m_subtitleTracks.push_back(row);
        }
    }
    if (!m_subtitleTracks.isEmpty()) {
        m_subtitleTracks.prepend(QVariantMap {
            { QStringLiteral("index"), -1 },
            { QStringLiteral("label"), QStringLiteral("Off") },
            { QStringLiteral("selected"), selectedSubtitle < 0 },
        });
    }

    m_rawQueue = session.value(QStringLiteral("NowPlayingQueue")).toArray();
    refreshQueueDetails(m_rawQueue);
    updateMediaSession();
    emit targetChanged();
    emit stateChanged();
}

void RemoteControlController::refreshQueueDetails(const QJsonArray& rawQueue)
{
    const RequestGeneration::Token generation = m_queueGeneration.next();
    if (rawQueue.isEmpty()) {
        m_queue.clear();
        emit stateChanged();
        return;
    }

    QStringList ids;
    ids.reserve(rawQueue.size());
    for (const QJsonValue& value : rawQueue) {
        const QString id = value.toObject().value(QStringLiteral("Id")).toString();
        if (!id.isEmpty())
            ids.push_back(id);
    }
    if (ids.isEmpty() || !m_api) {
        m_queue.clear();
        emit stateChanged();
        return;
    }

    Async::runScoped(
        this, m_api->fetchItemsByIds(ids),
        [this, generation, rawQueue](const std::vector<MovieItem>& items) {
            if (generation != m_queueGeneration.current())
                return;
            QVariantList queue;
            queue.reserve(rawQueue.size());
            for (const QJsonValue& value : rawQueue) {
                const QJsonObject raw = value.toObject();
                const QString id = raw.value(QStringLiteral("Id")).toString();
                const auto item = std::find_if(
                    items.cbegin(), items.cend(), [&id](const MovieItem& candidate) { return candidate.id == id; });
                QVariantMap row = item == items.cend() ? QVariantMap {} : metaToJson(*item).toVariantMap();
                row.insert(QStringLiteral("movieId"), id);
                row.insert(QStringLiteral("playlistItemId"), raw.value(QStringLiteral("PlaylistItemId")).toString());
                queue.push_back(row);
            }
            m_queue = std::move(queue);
            emit stateChanged();
        },
        [this, generation](const std::exception_ptr& error) {
            if (generation != m_queueGeneration.current())
                return;
            reportCommandError(QStringLiteral("load remote queue"), error);
        },
        "remote queue details");
}

bool RemoteControlController::supports(const QString& command) const
{
    return m_supportedCommands.contains(command);
}

qint64 RemoteControlController::predictedPositionTicks() const
{
    if (m_paused || m_positionTicks <= 0 || m_stateReceivedAtMs <= 0)
        return m_positionTicks;
    const qint64 elapsedMs = std::max<qint64>(0, QDateTime::currentMSecsSinceEpoch() - m_stateReceivedAtMs);
    const qint64 predicted = m_positionTicks + static_cast<qint64>(elapsedMs * 10'000.0 * m_playbackRate);
    return m_runtimeTicks > 0 ? std::min(predicted, m_runtimeTicks) : predicted;
}

bool RemoteControlController::playItems(
    const std::vector<MovieItem>& items, int startIndex, const QString& command, bool fromStart)
{
    if (!targetSelected())
        return false;
    QStringList ids;
    int remoteStartIndex = -1;
    qint64 startPositionTicks = -1;
    for (int index = 0; index < static_cast<int>(items.size()); ++index) {
        const MovieItem& item = items[static_cast<size_t>(index)];
        if (item.id.isEmpty() || !isPlayableItem(item))
            continue;
        if (index == startIndex) {
            remoteStartIndex = ids.size();
            startPositionTicks = fromStart ? 0 : std::max<qint64>(0, item.resumeTicks);
        }
        ids.push_back(item.id);
    }
    if (ids.isEmpty()) {
        emit errorText(QStringLiteral("This list has no remotely playable items."));
        return true;
    }
    if (remoteStartIndex < 0)
        remoteStartIndex = 0;
    runPlay(ids, command, remoteStartIndex, startPositionTicks);
    return true;
}

void RemoteControlController::playItemIds(
    const QStringList& itemIds, const QString& command, int startIndex, qint64 startPositionTicks)
{
    runPlay(itemIds, command, startIndex, startPositionTicks);
}

void RemoteControlController::runPlay(QStringList itemIds, QString command, int startIndex, qint64 startPositionTicks)
{
    if (!targetSelected() || !m_api)
        return;
    const QString target = m_selectedSessionId;
    Async::runScoped(
        this, m_api->sendRemotePlay(target, std::move(itemIds), std::move(command), startPositionTicks, startIndex),
        [this]() { QTimer::singleShot(300, this, &RemoteControlController::refreshTargets); },
        [this](const std::exception_ptr& error) { reportCommandError(QStringLiteral("start remote playback"), error); },
        "remote play command");
}

void RemoteControlController::sendPlaystate(const QString& command, qint64 seekPositionTicks)
{
    if (!targetSelected() || !m_api)
        return;
    const QString target = m_selectedSessionId;
    Async::runScoped(
        this, m_api->sendRemotePlaystate(target, command, seekPositionTicks),
        [this]() { QTimer::singleShot(250, this, &RemoteControlController::refreshTargets); },
        [this, command](const std::exception_ptr& error) { reportCommandError(command, error); },
        "remote playstate command");
}

void RemoteControlController::sendGeneralCommand(const QString& command, const QVariantMap& arguments)
{
    if (!targetSelected() || !m_api)
        return;
    const QString target = m_selectedSessionId;
    Async::runScoped(
        this, m_api->sendRemoteGeneralCommand(target, command, QJsonObject::fromVariantMap(arguments)),
        [this]() { QTimer::singleShot(250, this, &RemoteControlController::refreshTargets); },
        [this, command](const std::exception_ptr& error) { reportCommandError(command, error); },
        "remote general command");
}

void RemoteControlController::togglePause()
{
    sendPlaystate(m_paused ? QStringLiteral("Unpause") : QStringLiteral("Pause"));
    m_paused = !m_paused;
    m_stateReceivedAtMs = QDateTime::currentMSecsSinceEpoch();
    updateMediaSession();
    emit stateChanged();
}

void RemoteControlController::stopPlayback()
{
    sendPlaystate(QStringLiteral("Stop"));
}

void RemoteControlController::nextTrack()
{
    sendPlaystate(QStringLiteral("NextTrack"));
}

void RemoteControlController::previousTrack()
{
    sendPlaystate(QStringLiteral("PreviousTrack"));
}

void RemoteControlController::seek(qint64 positionTicks)
{
    const qint64 clamped = std::clamp<qint64>(positionTicks, 0, m_runtimeTicks > 0 ? m_runtimeTicks : positionTicks);
    m_positionTicks = clamped;
    m_stateReceivedAtMs = QDateTime::currentMSecsSinceEpoch();
    sendPlaystate(QStringLiteral("Seek"), clamped);
    updateMediaSession();
    emit stateChanged();
}

void RemoteControlController::seekRelative(qint64 deltaTicks)
{
    seek(std::max<qint64>(0, predictedPositionTicks() + deltaTicks));
}

void RemoteControlController::setVolume(int volume)
{
    m_volume = std::clamp(volume, 0, 100);
    sendGeneralCommand(QStringLiteral("SetVolume"), { { QStringLiteral("Volume"), m_volume } });
    updateMediaSession();
    emit stateChanged();
}

void RemoteControlController::adjustVolume(int delta)
{
    setVolume(m_volume + delta);
}

void RemoteControlController::toggleMute()
{
    m_muted = !m_muted;
    sendGeneralCommand(m_muted ? QStringLiteral("Mute") : QStringLiteral("Unmute"));
    emit stateChanged();
}

void RemoteControlController::selectAudioTrack(int streamIndex)
{
    sendGeneralCommand(QStringLiteral("SetAudioStreamIndex"), { { QStringLiteral("Index"), streamIndex } });
}

void RemoteControlController::selectSubtitleTrack(int streamIndex)
{
    sendGeneralCommand(QStringLiteral("SetSubtitleStreamIndex"), { { QStringLiteral("Index"), streamIndex } });
}

void RemoteControlController::setRepeatMode(const QString& mode)
{
    m_repeatMode = mode;
    sendGeneralCommand(QStringLiteral("SetRepeatMode"), { { QStringLiteral("RepeatMode"), mode } });
    emit stateChanged();
}

void RemoteControlController::setShuffled(bool shuffled)
{
    m_shuffled = shuffled;
    sendGeneralCommand(QStringLiteral("SetShuffleQueue"),
        { { QStringLiteral("ShuffleMode"), shuffled ? QStringLiteral("Shuffle") : QStringLiteral("Sorted") } });
    emit stateChanged();
}

void RemoteControlController::playQueueItem(int index)
{
    if (index < 0 || index >= m_rawQueue.size())
        return;
    QStringList ids;
    for (const QJsonValue& value : m_rawQueue)
        ids.push_back(value.toObject().value(QStringLiteral("Id")).toString());
    runPlay(ids, QStringLiteral("PlayNow"), index, 0);
}

void RemoteControlController::moveQueueItem(int from, int to)
{
    if (from < 0 || from >= m_rawQueue.size() || to < 0 || to >= m_rawQueue.size() || from == to)
        return;
    QJsonArray reordered = m_rawQueue;
    const QJsonValue moved = reordered.takeAt(from);
    reordered.insert(to, moved);
    QStringList ids;
    int current = 0;
    const QString currentPlaylistId
        = m_sessions.value(m_selectedSessionId).value(QStringLiteral("PlaylistItemId")).toString();
    for (int index = 0; index < reordered.size(); ++index) {
        const QJsonObject row = reordered.at(index).toObject();
        ids.push_back(row.value(QStringLiteral("Id")).toString());
        if (!currentPlaylistId.isEmpty() && row.value(QStringLiteral("PlaylistItemId")).toString() == currentPlaylistId)
            current = index;
    }
    runPlay(ids, QStringLiteral("PlayNow"), current, predictedPositionTicks());
}

void RemoteControlController::removeQueueItem(int index)
{
    if (index < 0 || index >= m_rawQueue.size())
        return;
    QJsonArray remaining = m_rawQueue;
    const QJsonObject removed = remaining.takeAt(index).toObject();
    if (remaining.isEmpty()) {
        stopPlayback();
        return;
    }
    QStringList ids;
    int current = 0;
    const QString currentPlaylistId
        = m_sessions.value(m_selectedSessionId).value(QStringLiteral("PlaylistItemId")).toString();
    for (int rowIndex = 0; rowIndex < remaining.size(); ++rowIndex) {
        const QJsonObject row = remaining.at(rowIndex).toObject();
        ids.push_back(row.value(QStringLiteral("Id")).toString());
        if (!currentPlaylistId.isEmpty() && row.value(QStringLiteral("PlaylistItemId")).toString() == currentPlaylistId)
            current = rowIndex;
    }
    const bool removedCurrent = removed.value(QStringLiteral("PlaylistItemId")).toString() == currentPlaylistId;
    runPlay(ids, QStringLiteral("PlayNow"), current, removedCurrent ? 0 : predictedPositionTicks());
}

void RemoteControlController::updateMediaSession()
{
    if (!m_mediaSession || !targetSelected() || m_nowPlayingItem.isEmpty()) {
        if (m_mediaSession)
            m_mediaSession->clear();
        return;
    }
    RemoteMediaSessionState state;
    state.title = m_nowPlayingItem.value(QStringLiteral("title")).toString();
    state.artist = m_nowPlayingItem.value(QStringLiteral("seriesName")).toString();
    if (state.artist.isEmpty())
        state.artist = m_nowPlayingItem.value(QStringLiteral("albumArtist")).toString();
    state.album = m_nowPlayingItem.value(QStringLiteral("album")).toString();
    state.targetName = m_selectedTargetName;
    state.durationMs = m_runtimeTicks / 10'000;
    state.positionMs = predictedPositionTicks() / 10'000;
    state.playbackRate = m_playbackRate;
    state.volume = m_volume;
    state.playing = !m_paused;
    state.canSeek = m_runtimeTicks > 0;
    m_mediaSession->update(state);
}

void RemoteControlController::reportCommandError(const QString& action, const std::exception_ptr& error)
{
    emit errorText(QStringLiteral("Could not %1: %2").arg(action, exceptionMessage(error)));
}

} // namespace JellyfinNative
