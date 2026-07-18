#include "AppController.h"

#include "ArtworkService.h"

#include "../api/JellyfinApiFacade.h"
#include "../common/AsyncTask.h"
#include "../common/MetaJson.h"
#include "../diagnostics/Diagnostics.h"
#include "../player/PlayQueueController.h"
#include "../player/PlayerController.h"
#include "BrowseSessionController.h"
#include "ContentModelController.h"
#include "HomeModelController.h"
#include "LibraryPrefetchController.h"
#include "LibraryQuery.h"
#include "QuickConnectController.h"
#include "SearchController.h"
#include "SessionController.h"
#include "SettingsController.h"
#include "UserItemStateController.h"

#include <QDebug>
#include <QGuiApplication>
#include <QJsonArray>
#include <QKeyEvent>
#include <QPixmapCache>
#include <QStringList>
#include <QTimer>
#include <QUuid>
#include <QVariantMap>
#include <QWindow>

#include <algorithm>
#include <memory>
#if defined(__GLIBC__)
#include <malloc.h>
#endif

namespace JellyfinNative {

namespace {

    constexpr int kLibraryPageSize = 100;
    // Reopening a library within this window shows the cached page as-is;
    // a server refresh so soon after the last one only causes delegate churn.
    constexpr qint64 kFreshLibraryCacheMs = 30000;

    bool isBrowseContainer(const MovieItem& item)
    {
        return item.itemType == QStringLiteral("Playlist") || item.itemType == QStringLiteral("BoxSet")
            || item.itemType == QStringLiteral("Folder") || item.itemType == QStringLiteral("PhotoAlbum")
            || item.itemType == QStringLiteral("MusicAlbum") || item.itemType == QStringLiteral("MusicArtist");
    }

}

AppController::AppController(DatabaseManager *database, DiscoveryController *discovery, JellyfinApiFacade *api,
    ArtworkService *artwork, PlayerController *player, QObject *parent)
    : QObject(parent)
    , m_database(database)
    , m_discovery(discovery)
    , m_api(api)
    , m_artwork(artwork)
    , m_player(player)
{
    m_playQueue = new PlayQueueController(api, this);
    m_syncPlay = new SyncPlayController(api, player, m_playQueue, this);
    m_quickConnect = new QuickConnectController(api, this);
    m_settings = new SettingsController(database, api, player, this);
    m_session = new SessionController(database, api, this);
    m_prefetch = new LibraryPrefetchController(api, artwork, this);
    m_browse = new BrowseSessionController(m_prefetch, this);
    m_management = new LibraryManagementController(api, m_browse, this);
    m_home = new HomeModelController(database, api, m_prefetch, this);
    m_content = new ContentModelController(api, m_prefetch, this);
    m_search = new SearchController(api, m_prefetch, this);
    m_itemState = new UserItemStateController(api, m_browse, m_home, m_content, m_search, this);
    if (m_artwork) {
        m_artwork->setServerUrl(m_session->serverUrl());
        connect(m_session, &SessionController::serverUrlChanged, m_artwork,
            [this]() { m_artwork->setServerUrl(m_session->serverUrl()); });
    }
    connect(m_playQueue, &PlayQueueController::successorPlaybackReady, this, [this]() { playQueueCurrent(false); });
    connect(m_browse, &BrowseSessionController::reloadRequested, this, [this]() { beginBrowse(); });
    connect(m_browse, &BrowseSessionController::moreItemsRequested, this, &AppController::loadMoreCurrentItems);
    connect(m_api, &JellyfinApiFacade::authenticationExpired, m_session, &SessionController::expireSession);
    connect(m_syncPlay, &SyncPlayController::errorText, this, &AppController::showToast);
    connect(m_syncPlay, &SyncPlayController::groupChanged, this,
        [this]() { m_player->setKeepPlayingInBackground(m_syncPlay->enabled()); });
    connect(m_syncPlay, &SyncPlayController::remotePlayCommand, this, &AppController::handleRemotePlay);
    connect(m_syncPlay, &SyncPlayController::remotePlaystateCommand, this, &AppController::handleRemotePlaystate);
    connect(m_syncPlay, &SyncPlayController::remoteGeneralCommand, this, &AppController::handleRemoteGeneralCommand);
    connect(m_syncPlay, &SyncPlayController::queuePlaybackRequested, this, [this](qint64 positionTicks) {
        MovieItem item = m_playQueue->currentItem();
        if (item.id.isEmpty())
            return;
        item.resumeTicks = std::max<qint64>(0, positionTicks);
        if (m_player->sessionActive() && m_activePlaybackItem.id != item.id)
            m_player->stopWithReason(QStringLiteral("syncplay-group-switch"));
        m_activePlaybackItem = item;
        setBusy(true, QStringLiteral("Joining SyncPlay playback…"));
        Async::runScoped(
            this, startPlayback(item, true), []() {},
            [this](const std::exception_ptr& error) {
                setBusy(false);
                showToast(exceptionMessage(error));
            },
            "SyncPlay playback startup");
    });
    connect(m_content, &ContentModelController::errorOccurred, this, &AppController::showToast);
    connect(m_search, &SearchController::errorOccurred, this, &AppController::showToast);
    connect(m_itemState, &UserItemStateController::errorOccurred, this, &AppController::setErrorText);
    connect(m_management, &LibraryManagementController::errorOccurred, this, &AppController::showToast);
    connect(m_management, &LibraryManagementController::operationSucceeded, this, &AppController::showToast);
    connect(m_management, &LibraryManagementController::refreshRequested, this, [this](const QString& changedItemId) {
        if (!changedItemId.isEmpty() && m_browse->descriptor().id == changedItemId)
            goHome();
        else
            beginBrowse();
    });
    connect(m_quickConnect, &QuickConnectController::busyChanged, this, &AppController::setBusy);
    connect(m_quickConnect, &QuickConnectController::errorOccurred, this, &AppController::setErrorText);
    connect(m_settings, &SettingsController::errorOccurred, this, &AppController::showToast);
    connect(m_session, &SessionController::busyChanged, this, &AppController::setBusy);
    connect(m_session, &SessionController::errorOccurred, this, &AppController::setErrorText);
    connect(m_session, &SessionController::accountProfilesChanged, this, [this]() {
        const bool hasProfiles = !m_session->accountProfiles().isEmpty();
        if (m_hasDefaultProfile == hasProfiles)
            return;
        m_hasDefaultProfile = hasProfiles;
        emit defaultProfileChanged();
    });
    connect(m_session, &SessionController::authenticatedChanged, this, [this](const AuthSession&) {
        if (m_artwork)
            m_artwork->setAuthorizationHeader(m_api->authorizationHeader());
        Async::runScoped(
            this, m_api->refreshPlaybackNetworkState(), []() {},
            [](const std::exception_ptr& error) {
                qWarning() << "playback bandwidth: route measurement failed" << exceptionMessage(error);
            });
        if (!m_hasDefaultProfile) {
            m_hasDefaultProfile = true;
            emit defaultProfileChanged();
        }
        m_home->loadCachedPayload();
        m_settings->loadRemote();
        m_syncPlay->connectSocket();
        loadLibraries();
        m_management->loadCurrentUserPolicy();
    });
    connect(m_session, &SessionController::loggedOut, this, &AppController::resetApplicationState);
    connect(m_quickConnect, &QuickConnectController::authenticated, this,
        [this](const AuthSession& session) { m_session->acceptSession(session); });
    connect(m_discovery, &DiscoveryController::serverDiscovered, this, [this](const DiscoveredServer& server) {
        m_discoveredServers.upsertServer(server);
        cacheDiscoveredServers();
    });

    connect(m_player, &PlayerController::playbackStopped, this, &AppController::handlePlaybackStopped);
    connect(m_player, &PlayerController::playbackLoadFailed, this,
        [this](const QString& itemId, qint64 positionTicks, const QString&) {
            if (m_codecFallbackAttempted || itemId.isEmpty() || itemId != m_activePlaybackItem.id)
                return;
            m_codecFallbackAttempted = true;
            MovieItem retryItem = m_activePlaybackItem;
            retryItem.resumeTicks = std::max<qint64>(0, positionTicks);
            m_player->teardownMpv();
            setBusy(true, QStringLiteral("Trying a compatible server stream…"));
            showToast(QStringLiteral("Direct playback was not supported; retrying once with server transcoding."));
            Async::runScoped(
                this, startPlayback(retryItem, false, true), []() {},
                [this](const std::exception_ptr& error) {
                    setBusy(false);
                    showToast(exceptionMessage(error));
                },
                "playback codec fallback");
        });
}

void AppController::initialize()
{
    Async::runScoped(
        this, initializeAsync(), []() {},
        [this](const std::exception_ptr& error) { setErrorText(exceptionMessage(error)); }, "app initialize");
}

QCoro::Task<void> AppController::initializeAsync()
{
    Diagnostics::Task task(QStringLiteral("app_initialize"));
    QString deviceId = co_await m_database->loadDeviceIdAsync();
    if (deviceId.isEmpty()) {
        deviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_database->saveDeviceId(deviceId);
    }
    m_api->setDeviceId(deviceId);

    co_await m_settings->loadLocalAsync();
    m_prefetch->configureImagePrefetch(m_settings->value(QStringLiteral("network/imagePrefetchAhead")).toInt(),
        m_settings->value(QStringLiteral("network/imagePrefetchConcurrency")).toInt());
    const bool hasDefaultProfile = co_await m_session->initializeAsync();
    if (m_hasDefaultProfile != hasDefaultProfile) {
        m_hasDefaultProfile = hasDefaultProfile;
        emit defaultProfileChanged();
    }
    co_await applyDiscoveredServersCacheAsync();
    m_discovery->start();
    m_initialized = true;
    emit initializedChanged();
}

void AppController::chooseDiscoveredServer(int index)
{
    const auto server = m_discoveredServers.serverAt(index);
    if (server.address.isEmpty())
        return;
    m_session->setServerName(server.name);
    m_session->setServerUrl(server.address);
}

void AppController::rememberServer(const QString& name, const QString& address)
{
    const QString normalizedAddress = address.trimmed();
    if (normalizedAddress.isEmpty())
        return;

    m_discoveredServers.upsertServer({ normalizedAddress,
        name.trimmed().isEmpty() ? QStringLiteral("Jellyfin Server") : name.trimmed(), normalizedAddress });
    cacheDiscoveredServers();
    m_session->setServerName(name.trimmed().isEmpty() ? QStringLiteral("Jellyfin Server") : name.trimmed());
    m_session->setServerUrl(normalizedAddress);
}

void AppController::cacheDiscoveredServers()
{
    QJsonArray cache;
    for (const auto& entry : m_discoveredServers.servers())
        cache.push_back(metaToJson(entry));
    m_database->saveDiscoveredServers(cache);
}

void AppController::useProfile(const QString& profileId)
{
    setErrorText({});
    m_quickConnect->cancel();
    m_syncPlay->disconnectSocket();
    m_session->activateProfile(profileId);
}

void AppController::switchUser()
{
    qInfo() << "app: switch user requested";
    m_quickConnect->cancel();
    if (m_player->visible())
        m_player->stopWithReason(QStringLiteral("switch-user"));
    m_syncPlay->disconnectSocket();
    if (m_database)
        m_database->invalidateHomePayloads();
    setBusy(false);
    setErrorText({});
    m_session->deactivate();
    m_discovery->start();
}

void AppController::logout()
{
    qInfo() << "app: logout requested";
    m_quickConnect->cancel();
    if (m_player->visible())
        m_player->stopWithReason(QStringLiteral("logout"));
    m_syncPlay->disconnectSocket();
    m_session->logout();
}

void AppController::resetApplicationState()
{
    m_syncPlay->disconnectSocket();
    m_settings->clearRemote();
    m_prefetch->stop();
    if (m_artwork) {
        m_artwork->setAuthorizationHeader({});
        m_artwork->cancelPrefetches();
    }
    if (m_database)
        m_database->invalidateHomePayloads();
    m_libraries.clear();
    m_browse->clear();
    m_home->reset();
    m_content->reset();
    m_search->reset();
    m_activePlaybackItem = {};
    m_management->clear();
    m_libraryLoadGeneration.invalidate();
    m_browse->reset();
    setBusy(false);
    setErrorText({});
    Async::runScoped(
        this, applyDiscoveredServersCacheAsync(), []() {},
        [](const std::exception_ptr& error) {
            qWarning() << "discovery: cached server load failed" << exceptionMessage(error);
        },
        "discovery cache");
    m_discovery->start();
}

void AppController::goHome()
{
    if (!m_session->authenticated())
        return;

    qInfo() << "app: go home viewKind=" << m_browse->viewKind();
    m_libraryLoadGeneration.invalidate();
    setBusy(false);
    m_discovery->stop();
    refreshHomeRows();
}

void AppController::openLibrary(int index)
{
    const LibraryItem library = m_libraries.libraryAt(index);
    if (library.id.isEmpty())
        return;
    m_browse->enterLibrary(library, libraryContentLabel(library), defaultLibraryQuery(library));
    m_home->recordLibraryUse(library);
    loadLibraryFilterOptions(beginBrowse(true), library);
}

bool AppController::openLibraryById(const QString& libraryId)
{
    if (libraryId.isEmpty())
        return false;
    for (int index = 0; index < m_libraries.count(); ++index) {
        if (m_libraries.libraryAt(index).id == libraryId) {
            openLibrary(index);
            return true;
        }
    }
    return false;
}

void AppController::playOrOpen(const MovieItem& item, bool fromStart)
{
    if (item.id.isEmpty())
        return;
    if (m_browse->enterItem(item)) {
        beginBrowse();
    } else {
        playQueuedItem(item, fromStart);
    }
}

void AppController::playFromModel(QObject *model, int index, bool fromStart)
{
    if (!model)
        return;

    if (auto *queue = qobject_cast<PlayQueueController *>(model)) {
        if (queue != m_playQueue || !queue->playAt(index)) {
            showToast(QStringLiteral("This item is no longer in the play queue."));
            return;
        }
        startQueuedPlayback(fromStart);
        return;
    }

    auto *movieModel = qobject_cast<MovieGridModel *>(model);
    if (!movieModel) {
        showToast(QStringLiteral("This item cannot be played from the current list."));
        return;
    }
    const MovieItem item = movieModel->movieAt(index);
    if (isBrowseContainer(item))
        playOrOpen(item, fromStart);
    else if (item.itemType == QStringLiteral("Episode") && !item.seriesId.isEmpty())
        playQueuedItem(item, fromStart);
    else
        playQueuedItems(movieModel->movies(), index, fromStart);
}

void AppController::playQueueNext()
{
    if (!m_playQueue->canGoNext()) {
        playEpisodeWithContext(m_playQueue->currentItem(), 1, true);
        return;
    }
    if (m_syncPlay && m_syncPlay->enabled()) {
        m_syncPlay->requestNextItem();
        return;
    }
    if (!m_playQueue->next())
        return;
    playQueueCurrent(false);
}

void AppController::playQueuePrevious()
{
    if (!m_playQueue->canGoPrevious()) {
        playEpisodeWithContext(m_playQueue->currentItem(), -1, true);
        return;
    }
    if (m_syncPlay && m_syncPlay->enabled()) {
        m_syncPlay->requestPreviousItem();
        return;
    }
    if (!m_playQueue->previous())
        return;
    playQueueCurrent(true);
}

void AppController::playQueueItem(int index)
{
    if (!queueMutationAllowed() || !m_playQueue->playAt(index))
        return;
    playQueueCurrent(false);
}

void AppController::playNextFromItem(const MovieItem& item)
{
    if (!queueMutationAllowed())
        return;
    if (!m_playQueue->playNext(item))
        setErrorText(QStringLiteral("This item cannot be queued."));
}

void AppController::addToQueueFromItem(const MovieItem& item)
{
    if (!queueMutationAllowed())
        return;
    if (!m_playQueue->addToQueue(item))
        setErrorText(QStringLiteral("This item cannot be queued."));
}

void AppController::loadMoreCurrentItems()
{
    if (m_browse->loadingMore() || !m_browse->hasMore())
        return;
    if (!m_api || m_api->session().accessToken.isEmpty())
        return;
    const BrowseDescriptor descriptor = m_browse->descriptor();
    if (!descriptor.isValid())
        return;

    const int startIndex = std::max(m_browse->nextStartIndex(), m_browse->rowCount());
    const RequestGeneration::Token loadGeneration = m_libraryLoadGeneration.current();
    const QString cacheKey = m_browse->cacheKey();
    const QVariantMap query = descriptor.kind == BrowseKind::Library ? m_browse->query() : QVariantMap {};
    if (m_artwork)
        m_artwork->cancelPrefetches();
    m_browse->setLoadingMore(true);

    const auto onDone = [this, cacheKey](const PagedMovieItems& page) { showCurrentItemsPage(page, cacheKey, true); };
    const auto onError = [this](const std::exception_ptr& error) {
        m_browse->setLoadingMore(false);
        showToast(exceptionMessage(error));
    };

    Async::runLatest(this, m_api->fetchBrowsePage(descriptor, startIndex, kLibraryPageSize, query),
        m_libraryLoadGeneration, loadGeneration, onDone, onError);
}

void AppController::playQueuedItems(const std::vector<MovieItem>& items, int startIndex, bool fromStart)
{
    if (!m_playQueue->playNow(items, startIndex)) {
        showToast(QStringLiteral("This item cannot be queued."));
        return;
    }
    startQueuedPlayback(fromStart);
}

void AppController::playModel(MovieGridModel *model, bool shuffled)
{
    if (!model)
        return;
    const std::vector<MovieItem>& items = model->movies();
    const auto firstPlayable = std::find_if(
        items.begin(), items.end(), [](const MovieItem& item) { return !item.id.isEmpty() && isPlayableItem(item); });
    if (firstPlayable == items.end()) {
        showToast(QStringLiteral("This list has no playable items."));
        return;
    }
    playQueuedItems(items, static_cast<int>(std::distance(items.begin(), firstPlayable)), false);
    if (shuffled)
        m_playQueue->setShuffled(true);
}

void AppController::playEpisodicContainer(const QString& seriesId, const QString& seasonId)
{
    if (!m_api || seriesId.isEmpty())
        return;

    const quint64 generation = ++m_episodeQueueGeneration;
    setBusy(true,
        seasonId.isEmpty() ? QStringLiteral("Finding the next episode…")
                           : QStringLiteral("Finding the next episode in this season…"));
    Async::runScoped(
        this, m_api->fetchEpisodes(seriesId, seasonId),
        [this, generation](const std::vector<MovieItem>& episodes) {
            if (generation != m_episodeQueueGeneration)
                return;
            const int startIndex = episodicPlaybackStartIndex(episodes);
            if (startIndex < 0) {
                setBusy(false);
                showToast(QStringLiteral("There is no unplayed episode available after the last watched episode."));
                return;
            }
            playQueuedItems(episodes, startIndex, false);
        },
        [this, generation](const std::exception_ptr& error) {
            if (generation != m_episodeQueueGeneration)
                return;
            setBusy(false);
            showToast(exceptionMessage(error));
        },
        "episodic container playback");
}

void AppController::playQueuedItem(const MovieItem& item, bool fromStart)
{
    if (item.itemType == QStringLiteral("Episode") && !item.seriesId.isEmpty()) {
        playEpisodeWithContext(item, 0, fromStart);
        return;
    }
    if (!m_playQueue->playNow(item)) {
        showToast(QStringLiteral("This item cannot be queued."));
        return;
    }
    startQueuedPlayback(fromStart);
}

void AppController::playEpisodeWithContext(const MovieItem& episode, int direction, bool fromStart)
{
    if (!m_api || episode.itemType != QStringLiteral("Episode") || episode.seriesId.isEmpty()) {
        if (direction == 0 && m_playQueue->playNow(episode))
            startQueuedPlayback(fromStart);
        return;
    }

    const quint64 generation = ++m_episodeQueueGeneration;
    setBusy(true, direction == 0 ? QStringLiteral("Loading episode queue…") : QStringLiteral("Finding episode…"));
    Async::runScoped(
        this, m_api->fetchEpisodes(episode.seriesId),
        [this, generation, episode, direction, fromStart](const std::vector<MovieItem>& episodes) {
            if (generation != m_episodeQueueGeneration)
                return;
            const auto current = std::find_if(episodes.begin(), episodes.end(),
                [&episode](const MovieItem& candidate) { return candidate.id == episode.id; });
            if (current == episodes.end()) {
                setBusy(false);
                if (direction == 0 && m_playQueue->playNow(episode))
                    startQueuedPlayback(fromStart);
                else
                    showToast(QStringLiteral("This episode was not found in its series."));
                return;
            }

            int targetIndex = static_cast<int>(std::distance(episodes.begin(), current));
            if (direction != 0) {
                int candidate = targetIndex + direction;
                while (candidate >= 0 && candidate < static_cast<int>(episodes.size())
                    && !isPlayableItem(episodes[static_cast<size_t>(candidate)])) {
                    candidate += direction;
                }
                if (candidate < 0 || candidate >= static_cast<int>(episodes.size())) {
                    setBusy(false);
                    showToast(direction < 0 ? QStringLiteral("There is no previous episode.")
                                            : QStringLiteral("There is no next episode."));
                    return;
                }
                targetIndex = candidate;
            }

            if (!m_playQueue->playNow(episodes, targetIndex)) {
                setBusy(false);
                showToast(QStringLiteral("The adjacent episode could not be queued."));
                return;
            }
            qInfo() << "play queue: loaded episode context" << episodes.size() << "items, target" << targetIndex;
            startQueuedPlayback(direction == 0 ? fromStart : true);
        },
        [this, generation, episode, direction, fromStart](const std::exception_ptr& error) {
            if (generation != m_episodeQueueGeneration)
                return;
            setBusy(false);
            if (direction == 0 && m_playQueue->playNow(episode)) {
                startQueuedPlayback(fromStart);
                return;
            }
            showToast(exceptionMessage(error));
        },
        "episode queue context");
}

void AppController::startQueuedPlayback(bool fromStart)
{
    if (!m_syncPlay || !m_syncPlay->enabled()) {
        playQueueCurrent(fromStart);
        return;
    }

    const std::vector<PlaybackQueueItem> queue = m_playQueue->nowPlayingQueue();
    QStringList itemIds;
    itemIds.reserve(static_cast<qsizetype>(queue.size()));
    for (const PlaybackQueueItem& item : queue)
        itemIds.push_back(item.itemId);

    const MovieItem item = m_playQueue->currentItem();
    const qint64 startPositionTicks
        = fromStart || !isMeaningfulResumePosition(item.resumeTicks, item.runtimeTicks) ? 0 : item.resumeTicks;
    setBusy(true, QStringLiteral("Updating SyncPlay queue…"));
    Async::runScoped(
        this, m_api->syncPlaySetNewQueue(itemIds, m_playQueue->currentIndex(), startPositionTicks),
        [this]() { m_syncPlay->requestUnpauseWhenReady(); },
        [this](const std::exception_ptr& error) {
            setBusy(false);
            showToast(exceptionMessage(error));
        },
        "syncplay queue update");
}

void AppController::playQueueCurrent(bool fromStart)
{
    MovieItem item = m_playQueue->currentItem();
    if (item.id.isEmpty())
        return;
    if (fromStart || !isMeaningfulResumePosition(item.resumeTicks, item.runtimeTicks))
        item.resumeTicks = 0;
    m_activePlaybackItem = item;
    setBusy(true, QStringLiteral("Negotiating playback…"));
    Async::runScoped(
        this, startPlayback(item), []() {},
        [this](const std::exception_ptr& error) {
            setBusy(false);
            showToast(exceptionMessage(error));
        },
        "playback startup");
}

void AppController::handleRemotePlay(const QJsonObject& data)
{
    QStringList itemIds;
    for (const QJsonValue& value : data.value(QStringLiteral("ItemIds")).toArray()) {
        const QString id = value.toString();
        if (!id.isEmpty())
            itemIds.push_back(id);
    }
    if (itemIds.isEmpty())
        return;

    const QString command = data.value(QStringLiteral("PlayCommand")).toString(QStringLiteral("PlayNow"));
    const int requestedIndex = data.value(QStringLiteral("StartIndex")).toInt(0);
    const qint64 startTicks = data.value(QStringLiteral("StartPositionTicks")).toVariant().toLongLong();
    qInfo() << "remote: play" << command << itemIds.size() << "items, index" << requestedIndex;
    Async::runScoped(
        this, m_api->fetchItemsByIds(itemIds),
        [this, itemIds, command, requestedIndex, startTicks](const std::vector<MovieItem>& fetched) {
            std::vector<MovieItem> ordered;
            ordered.reserve(static_cast<size_t>(itemIds.size()));
            for (const QString& id : itemIds) {
                const auto found = std::find_if(
                    fetched.begin(), fetched.end(), [&id](const MovieItem& item) { return item.id == id; });
                if (found != fetched.end())
                    ordered.push_back(*found);
            }
            if (ordered.empty()) {
                showToast(QStringLiteral("The remote playback item is unavailable."));
                return;
            }

            if (command == QStringLiteral("PlayNext")) {
                for (auto item = ordered.rbegin(); item != ordered.rend(); ++item)
                    m_playQueue->playNext(*item);
                return;
            }
            if (command == QStringLiteral("PlayLast")) {
                for (const MovieItem& item : ordered)
                    m_playQueue->addToQueue(item);
                return;
            }

            const int index = std::clamp(requestedIndex, 0, static_cast<int>(ordered.size()) - 1);
            ordered[static_cast<size_t>(index)].resumeTicks = std::max<qint64>(0, startTicks);
            if (command == QStringLiteral("PlayShuffle")) {
                m_playQueue->playNow(ordered, index);
                m_playQueue->setShuffled(true);
            } else {
                m_playQueue->playNow(ordered, index);
            }
            startQueuedPlayback(startTicks <= 0);
        },
        [this](const std::exception_ptr& error) { showToast(exceptionMessage(error)); }, "remote playback request");
}

void AppController::handleRemotePlaystate(const QJsonObject& data)
{
    const QString command = data.value(QStringLiteral("Command")).toString();
    qInfo() << "remote: playstate" << command;
    if (command == QStringLiteral("Stop")) {
        m_player->stopWithReason(QStringLiteral("remote-stop"));
    } else if (command == QStringLiteral("Pause")) {
        if (!m_player->paused())
            m_syncPlay->enabled() ? m_syncPlay->requestTogglePause() : m_player->togglePause();
    } else if (command == QStringLiteral("Unpause")) {
        if (m_player->paused())
            m_syncPlay->enabled() ? m_syncPlay->requestTogglePause() : m_player->togglePause();
    } else if (command == QStringLiteral("PlayPause")) {
        m_syncPlay->enabled() ? m_syncPlay->requestTogglePause() : m_player->togglePause();
    } else if (command == QStringLiteral("Seek")) {
        const double seconds
            = static_cast<double>(data.value(QStringLiteral("SeekPositionTicks")).toVariant().toLongLong())
            / 10'000'000.0;
        m_syncPlay->enabled() ? m_syncPlay->requestSeek(seconds) : m_player->seek(seconds);
    } else if (command == QStringLiteral("Rewind")) {
        m_syncPlay->enabled() ? m_syncPlay->requestRelativeSeek(-10.0) : m_player->seekBack();
    } else if (command == QStringLiteral("FastForward")) {
        m_syncPlay->enabled() ? m_syncPlay->requestRelativeSeek(10.0) : m_player->seekForward();
    } else if (command == QStringLiteral("NextTrack")) {
        playQueueNext();
    } else if (command == QStringLiteral("PreviousTrack")) {
        playQueuePrevious();
    }
}

void AppController::handleRemoteGeneralCommand(const QJsonObject& data)
{
    const QString command = data.value(QStringLiteral("Name")).toString();
    const QJsonObject arguments = data.value(QStringLiteral("Arguments")).toObject();
    qInfo() << "remote: general command" << command;

    if (command == QStringLiteral("SetVolume")) {
        m_player->setVolume(arguments.value(QStringLiteral("Volume")).toString().toInt());
    } else if (command == QStringLiteral("VolumeUp")) {
        m_player->adjustVolume(5);
    } else if (command == QStringLiteral("VolumeDown")) {
        m_player->adjustVolume(-5);
    } else if (command == QStringLiteral("SetAudioStreamIndex")) {
        m_player->selectAudioStreamIndex(arguments.value(QStringLiteral("Index")).toString().toInt());
    } else if (command == QStringLiteral("SetSubtitleStreamIndex")) {
        m_player->selectSubtitleStreamIndex(arguments.value(QStringLiteral("Index")).toString().toInt());
    } else if (command == QStringLiteral("ToggleStats")) {
        m_player->toggleDebugOsd();
    } else if (command == QStringLiteral("ToggleOsd")) {
        emit remoteUiActionRequested(QStringLiteral("toggle-osd"));
    } else if (command == QStringLiteral("ToggleContextMenu")) {
        emit remoteUiActionRequested(QStringLiteral("context-menu"));
    } else if (command == QStringLiteral("GoHome")) {
        emit remoteUiActionRequested(QStringLiteral("home"));
    } else if (command == QStringLiteral("GoToSettings")) {
        emit remoteUiActionRequested(QStringLiteral("settings"));
    } else if (command == QStringLiteral("GoToSearch")) {
        emit remoteUiActionRequested(QStringLiteral("search"));
    } else if (command == QStringLiteral("Play")) {
        if (m_player->paused())
            m_syncPlay->enabled() ? m_syncPlay->requestTogglePause() : m_player->togglePause();
    } else if (command == QStringLiteral("DisplayMessage")) {
        showToast(arguments.value(QStringLiteral("Text")).toString());
    } else {
        const QHash<QString, int> keys = {
            { QStringLiteral("MoveUp"), Qt::Key_Up },
            { QStringLiteral("MoveDown"), Qt::Key_Down },
            { QStringLiteral("MoveLeft"), Qt::Key_Left },
            { QStringLiteral("MoveRight"), Qt::Key_Right },
            { QStringLiteral("Select"), Qt::Key_Return },
            { QStringLiteral("Back"), Qt::Key_Back },
        };
        const auto found = keys.constFind(command);
        if (found != keys.cend() && QGuiApplication::focusWindow()) {
            QKeyEvent press(QEvent::KeyPress, *found, Qt::NoModifier);
            QKeyEvent release(QEvent::KeyRelease, *found, Qt::NoModifier);
            QCoreApplication::sendEvent(QGuiApplication::focusWindow(), &press);
            QCoreApplication::sendEvent(QGuiApplication::focusWindow(), &release);
        }
    }
}

bool AppController::queueMutationAllowed()
{
    if (m_syncPlay && m_syncPlay->enabled()) {
        showToast(QStringLiteral("Leave SyncPlay before changing the play queue."));
        return false;
    }
    return true;
}

QCoro::Task<void> AppController::startPlayback(MovieItem playItem, bool startPaused, bool forceTranscode)
{
    Diagnostics::Task task(QStringLiteral("playback_negotiate"),
        { { QStringLiteral("itemId"), playItem.id }, { QStringLiteral("title"), playItem.title },
            { QStringLiteral("type"), playItem.itemType } });

    if (!forceTranscode)
        m_codecFallbackAttempted = false;
    PlaybackSession session = co_await m_api->negotiatePlayback(playItem, forceTranscode);
    session.nowPlayingQueue = m_playQueue->nowPlayingQueue();
    setBusy(false);
    m_player->play(session, startPaused);

    const QString itemId = playItem.id;
    Async::runScoped(
        this, m_api->fetchMediaSegments(itemId),
        [this, itemId](const std::vector<MediaSegment>& segments) {
            if (m_player && !segments.empty())
                m_player->setMediaSegments(itemId, segments);
        },
        [itemId](const std::exception_ptr& error) {
            qInfo() << "app: media segments unavailable for" << itemId << ":" << exceptionMessage(error);
        });
}

void AppController::onMemoryPressure(const QString& level)
{
    const QString normalized = level.trimmed().toLower();
    if (normalized != QStringLiteral("low") && normalized != QStringLiteral("critical")) {
        return;
    }

    const bool aggressive = normalized == QStringLiteral("critical");
    qInfo() << "memory pressure:" << normalized << "aggressive=" << aggressive;
    if (m_artwork)
        m_artwork->releaseMemory(aggressive);
    QPixmapCache::clear();
#if defined(__GLIBC__)
    malloc_trim(0);
#endif
    if (aggressive)
        emit aggressiveMemoryPressure();
}

void AppController::shutdown()
{
    Diagnostics::Phase phase(QStringLiteral("shutdown"), QStringLiteral("app_controller_shutdown"));
    if (m_shuttingDown)
        return;
    m_shuttingDown = true;
    qInfo() << "app: shutdown requested";
    m_player->prepareForShutdown();
    m_quickConnect->cancel();
    m_prefetch->stop();
    m_api->cancelRequests();
    m_discovery->stop();
    m_player->teardownMpv();
}

void AppController::clearError()
{
    setErrorText({});
}

void AppController::setBusy(bool busy, const QString& busyText)
{
    if (m_busy == busy && m_busyText == busyText)
        return;
    m_busy = busy;
    m_busyText = busyText;
    emit busyChanged();
}

void AppController::setErrorText(const QString& errorText)
{
    if (m_errorText == errorText)
        return;
    m_errorText = errorText;
    emit errorTextChanged();
}

void AppController::showToast(const QString& message)
{
    if (message.trimmed().isEmpty())
        return;
    emit toastMessage(message);
}

QCoro::Task<void> AppController::applyDiscoveredServersCacheAsync()
{
    const auto servers = co_await m_database->loadDiscoveredServersAsync();
    std::vector<DiscoveredServer> parsed;
    parsed.reserve(servers.size());
    for (const auto& value : servers)
        parsed.push_back(metaFromJson<DiscoveredServer>(value.toObject()));
    m_discoveredServers.setServers(parsed, false);
}

void AppController::loadLibraries()
{
    m_prefetch->stop();
    Async::runScoped(
        this, m_api->fetchLibraries(),
        [this](const std::vector<LibraryItem>& libraries) {
            m_libraries.setLibraries(libraries);
            setBusy(false);
            m_discovery->stop();
            refreshHomeRows();
        },
        [this](const std::exception_ptr& error) {
            setBusy(false);
            if (!m_session->handleUnauthorized(error))
                setErrorText(exceptionMessage(error));
        });
}

void AppController::loadLibraryFilterOptions(RequestGeneration::Token generation, const LibraryItem& library)
{
    if (!m_api || m_api->session().accessToken.isEmpty() || library.id.isEmpty())
        return;

    Async::runLatest(
        this, m_api->fetchLibraryFilterOptions(library.id, library.collectionType), m_libraryLoadGeneration, generation,
        [this, library](const QVariantMap& options) {
            if (library.id != m_browse->libraryId())
                return;
            m_browse->setFilterOptions(options);
        },
        [this, library](const std::exception_ptr& error) {
            if (library.id != m_browse->libraryId())
                return;
            qWarning() << "library filters: failed" << library.name << exceptionMessage(error);
            m_browse->clearFilterOptions();
        });
}

void AppController::showCurrentItemsPage(const PagedMovieItems& page, const QString& cacheKey, bool append)
{
    m_browse->setPage(page, cacheKey, append);
    // Keep the warm cache in sync with what the user just saw so the next
    // open of this library can skip the refresh while the data is fresh.
    if (!append && page.startIndex == 0 && m_prefetch)
        m_prefetch->storePage(cacheKey, page);
    setBusy(false);
}

RequestGeneration::Token AppController::beginBrowse(bool useWarmCache)
{
    const BrowseDescriptor descriptor = m_browse->descriptor();
    if (!descriptor.isValid() || !m_api || m_api->session().accessToken.isEmpty())
        return 0;

    const RequestGeneration::Token generation = m_libraryLoadGeneration.next();
    const QVariantMap query = descriptor.kind == BrowseKind::Library ? m_browse->query() : QVariantMap {};
    QString cacheKey = descriptor.cacheKey(query);
    if (descriptor.kind == BrowseKind::Library) {
        LibraryItem library;
        library.id = m_browse->libraryId();
        library.collectionType = m_browse->libraryCollectionType();
        cacheKey = libraryCacheKey(library, query);
    }

    m_browse->resetPaging(cacheKey);
    m_prefetch->stop();
    if (m_artwork)
        m_artwork->cancelPrefetches();
    if (useWarmCache) {
        const int cachedCount = m_browse->applyCachedPage(cacheKey);
        m_browse->setWarmCachePaging(cachedCount, kLibraryPageSize);
        if (cachedCount > 0) {
            const qint64 ageMs = m_prefetch ? m_prefetch->pageAgeMs(cacheKey) : -1;
            if (ageMs >= 0 && ageMs < kFreshLibraryCacheMs) {
                qInfo() << "library open: cache fresh, skipping refresh" << descriptor.name << cachedCount
                        << "age_ms=" << ageMs;
                m_browse->setLoadingMore(false);
                return generation;
            }
            qInfo() << "library open: showing cached page while refreshing" << descriptor.name << cachedCount;
        }
    } else {
        m_browse->clear();
        m_browse->setLoadingMore(true);
    }

    Async::runLatest(
        this, m_api->fetchBrowsePage(descriptor, 0, kLibraryPageSize, query), m_libraryLoadGeneration, generation,
        [this, cacheKey](const PagedMovieItems& page) { showCurrentItemsPage(page, cacheKey, false); },
        [this](const std::exception_ptr& error) {
            m_browse->setLoadingMore(false);
            showToast(exceptionMessage(error));
        });
    return generation;
}

void AppController::openNamedCollection(const QString& kind, const QString& value)
{
    const QString name = value.trimmed();
    if (name.isEmpty() || (kind != QStringLiteral("genre") && kind != QStringLiteral("studio")))
        return;
    m_browse->enterNamedCollection(kind, name);
    beginBrowse();
}

void AppController::handlePlaybackStopped(const QString& itemId, qint64 positionTicks, bool completed)
{
    qInfo() << "app: playback stopped" << itemId << positionTicks << completed;
    m_itemState->recordPlaybackStopped(m_activePlaybackItem, itemId, positionTicks, completed);
    if (!completed || m_activePlaybackItem.id != itemId || (m_syncPlay && m_syncPlay->enabled()))
        return;
    if (m_playQueue->next())
        playQueueCurrent(false);
    else
        m_playQueue->enqueueEpisodeSuccessors(m_activePlaybackItem);
}

} // namespace JellyfinNative
