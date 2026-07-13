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
#include <QJsonArray>
#include <QPixmapCache>
#include <QStringList>
#include <QTimer>
#include <QUuid>
#include <QVariantMap>

#include <algorithm>
#include <memory>
#if defined(__GLIBC__)
#include <malloc.h>
#endif

namespace JellyfinNative {

namespace {

    constexpr int kLibraryPageSize = 100;

    bool isBrowseContainer(const MovieItem& item)
    {
        return item.itemType == QStringLiteral("Series") || item.itemType == QStringLiteral("Season")
            || item.itemType == QStringLiteral("Playlist") || item.itemType == QStringLiteral("BoxSet")
            || item.itemType == QStringLiteral("Folder");
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
    m_syncPlay = new SyncPlayController(api, player, this);
    m_playQueue = new PlayQueueController(api, this);
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
    connect(m_session, &SessionController::authenticatedChanged, this, [this](const AuthSession&) {
        if (m_artwork)
            m_artwork->setAuthorizationHeader(m_api->authorizationHeader());
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
        QJsonArray cache;
        for (const auto& entry : m_discoveredServers.servers())
            cache.push_back(metaToJson(entry));
        m_database->saveDiscoveredServers(cache);
    });

    connect(m_player, &PlayerController::playbackStopped, this, &AppController::handlePlaybackStopped);
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
    const bool hasDefaultProfile = !(co_await m_database->loadAuthSessionAsync()).accessToken.isEmpty();
    if (m_hasDefaultProfile != hasDefaultProfile) {
        m_hasDefaultProfile = hasDefaultProfile;
        emit defaultProfileChanged();
    }
    if (!(co_await m_session->initializeAsync())) {
        co_await applyDiscoveredServersCacheAsync();
        m_discovery->start();
    }
}

void AppController::chooseDiscoveredServer(int index)
{
    const auto server = m_discoveredServers.serverAt(index);
    if (server.address.isEmpty())
        return;
    m_session->setServerUrl(server.address);
}

bool AppController::useDefaultProfile()
{
    setErrorText({});
    m_quickConnect->cancel();

    if (!m_session->authenticated()) {
        Async::runScoped(
            this, useDefaultProfileAsync(), [](bool) {},
            [this](const std::exception_ptr& error) { setErrorText(exceptionMessage(error)); }, "default profile");
        return false;
    }

    if (m_libraries.rowCount() <= 0) {
        loadLibraries();
        return true;
    }

    setBusy(false);
    m_discovery->stop();
    refreshHomeRows();
    return true;
}

QCoro::Task<bool> AppController::useDefaultProfileAsync()
{
    const AuthSession session = co_await m_database->loadAuthSessionAsync();
    if (session.accessToken.isEmpty() || m_session->serverUrl().isEmpty()) {
        setErrorText(QStringLiteral("This saved profile needs to sign in again."));
        co_return false;
    }
    m_session->acceptSession(session);
    co_return true;
}

void AppController::switchUser()
{
    qInfo() << "app: switch user requested";
    m_quickConnect->cancel();
    if (m_player->visible())
        m_player->stopWithReason(QStringLiteral("switch-user"));
    if (m_database)
        m_database->invalidateHomePayloads();
    setBusy(false);
    setErrorText({});
    m_discovery->start();
}

void AppController::logout()
{
    qInfo() << "app: logout requested";
    m_quickConnect->cancel();
    if (m_player->visible())
        m_player->stopWithReason(QStringLiteral("logout"));
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
    if (m_hasDefaultProfile) {
        m_hasDefaultProfile = false;
        emit defaultProfileChanged();
    }
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

void AppController::playOrOpen(const MovieItem& item, bool fromStart)
{
    if (item.id.isEmpty())
        return;
    if (item.itemType == QStringLiteral("Season")) {
        openSeason(item);
    } else if (m_browse->enterItem(item)) {
        beginBrowse(false, item.itemType == QStringLiteral("Series"));
    } else {
        playQueuedItem(item, fromStart);
    }
}

void AppController::playFromModel(MovieGridModel *model, int index, bool fromStart)
{
    if (!model)
        return;
    const MovieItem item = model->movieAt(index);
    if (isBrowseContainer(item))
        playOrOpen(item, fromStart);
    else
        playQueuedItems(model->movies(), index, fromStart);
}

void AppController::playQueueNext()
{
    if (!queueMutationAllowed() || !m_playQueue->next())
        return;
    playQueueCurrent(false);
}

void AppController::playQueuePrevious()
{
    if (!queueMutationAllowed() || !m_playQueue->previous())
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

void AppController::openSeriesById(const QString& seriesId, const QString& seriesName)
{
    MovieItem series;
    series.id = seriesId;
    series.title = seriesName.isEmpty() ? QStringLiteral("Series") : seriesName;
    series.itemType = QStringLiteral("Series");
    playOrOpen(series);
}

void AppController::openSeasonById(const QString& seriesId, const QString& seasonId, const QString& seasonName)
{
    if (seriesId.isEmpty())
        return;
    MovieItem season;
    season.id = seasonId;
    season.seriesId = seriesId;
    season.title = seasonName.isEmpty() ? QStringLiteral("Season") : seasonName;
    season.itemType = seasonId.isEmpty() ? QStringLiteral("Series") : QStringLiteral("Season");
    openSeason(season);
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

void AppController::playQueuedItem(const MovieItem& item, bool fromStart)
{
    if (!m_playQueue->playNow(item)) {
        showToast(QStringLiteral("This item cannot be queued."));
        return;
    }
    startQueuedPlayback(fromStart);
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
        [this, fromStart]() { playQueueCurrent(fromStart); },
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

bool AppController::queueMutationAllowed()
{
    if (m_syncPlay && m_syncPlay->enabled()) {
        showToast(QStringLiteral("Leave SyncPlay before changing the play queue."));
        return false;
    }
    return true;
}

QCoro::Task<void> AppController::startPlayback(MovieItem playItem)
{
    Diagnostics::Task task(QStringLiteral("playback_negotiate"),
        { { QStringLiteral("itemId"), playItem.id }, { QStringLiteral("title"), playItem.title },
            { QStringLiteral("type"), playItem.itemType } });

    PlaybackSession session = co_await m_api->negotiatePlayback(playItem);
    session.nowPlayingQueue = m_playQueue->nowPlayingQueue();
    setBusy(false);
    m_player->play(session);

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
    m_discoveredServers.setServers(parsed);
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

RequestGeneration::Token AppController::beginBrowse(bool useWarmCache, bool descendIntoEmptySeries)
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
        if (cachedCount > 0)
            qInfo() << "library open: showing cached page while refreshing" << descriptor.name << cachedCount;
    } else {
        m_browse->clear();
        m_browse->setLoadingMore(true);
    }

    Async::runLatest(
        this, m_api->fetchBrowsePage(descriptor, 0, kLibraryPageSize, query), m_libraryLoadGeneration, generation,
        [this, cacheKey, descendIntoEmptySeries, descriptor](const PagedMovieItems& page) {
            if (descendIntoEmptySeries && page.items.empty()) {
                MovieItem fallback;
                fallback.id = descriptor.id;
                fallback.seriesId = descriptor.id;
                fallback.title = descriptor.name;
                fallback.itemType = QStringLiteral("Series");
                openSeason(fallback);
            } else {
                showCurrentItemsPage(page, cacheKey, false);
            }
        },
        [this](const std::exception_ptr& error) {
            m_browse->setLoadingMore(false);
            showToast(exceptionMessage(error));
        });
    return generation;
}

void AppController::openSeason(const MovieItem& season)
{
    const QString seriesId = !season.seriesId.isEmpty() ? season.seriesId : m_browse->seriesId();
    if (seriesId.isEmpty()) {
        qWarning() << "season open: missing series id" << season.id << season.title << season.itemType;
        return;
    }
    m_browse->enterSeason(seriesId, season);
    beginBrowse();
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
