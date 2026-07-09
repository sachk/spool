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
#include <QVariantMap>

#include <algorithm>
#include <memory>
#if defined(__GLIBC__)
#include <malloc.h>
#endif

namespace JellyfinNative {

namespace {

    constexpr int kLibraryPageSize = 100;
    constexpr int kLibraryPrefetchDistance = 200;
    constexpr int kHomePayloadSchemaVersion = 2;

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
    m_playQueue = new PlayQueueController(this);
    m_quickConnect = new QuickConnectController(api, this);
    m_settings = new SettingsController(database, api, player, this);
    m_session = new SessionController(database, api, this);
    m_prefetch = new LibraryPrefetchController(api, artwork, this);
    m_browse = new BrowseSessionController(m_prefetch, this);
    m_management = new LibraryManagementController(api, m_browse, this);
    m_home = new HomeModelController(api, m_prefetch, this);
    m_content = new ContentModelController(api, m_prefetch, this);
    m_search = new SearchController(api, m_prefetch, this);
    m_itemState = new UserItemStateController(m_browse, m_home, m_content, m_search, this);
    m_prefetch->configureImagePrefetch(16, 3);
    connect(m_api, &JellyfinApiFacade::authenticationExpired, m_session, &SessionController::expireSession);
    connect(m_syncPlay, &SyncPlayController::errorText, this, &AppController::showToast);
    connect(m_content, &ContentModelController::errorOccurred, this, &AppController::showToast);
    connect(m_search, &SearchController::errorOccurred, this, &AppController::showToast);
    connect(m_itemState, &UserItemStateController::favoriteChanged, this, &AppController::itemFavoriteChanged);
    connect(m_itemState, &UserItemStateController::playedChanged, this, &AppController::itemPlayedChanged);
    connect(m_home, &HomeModelController::homePayloadReady, this, &AppController::saveHomePayload);
    connect(m_management, &LibraryManagementController::errorOccurred, this, &AppController::showToast);
    connect(m_management, &LibraryManagementController::operationSucceeded, this, &AppController::showToast);
    connect(m_management, &LibraryManagementController::refreshRequested, this, [this](const QString& changedItemId) {
        if (!changedItemId.isEmpty() && m_browse->descriptor().id == changedItemId)
            goHome();
        else
            refreshCurrentLibrary();
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
        Async::runScoped(
            this, applyCachedHomePayloadAsync(), []() {},
            [](const std::exception_ptr& error) {
                qWarning() << "home: warm payload cache failed" << exceptionMessage(error);
            },
            "home cache");
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

QString AppController::currentViewKind() const
{
    return m_browse->viewKind();
}

DiscoveredServerModel *AppController::discoveredServers()
{
    return &m_discoveredServers;
}

LibraryListModel *AppController::libraries()
{
    return &m_libraries;
}

BrowseSessionController *AppController::browse()
{
    return m_browse;
}

HomeModelController *AppController::home()
{
    return m_home;
}

ContentModelController *AppController::content()
{
    return m_content;
}

SearchController *AppController::search()
{
    return m_search;
}

SyncPlayController *AppController::syncPlay()
{
    return m_syncPlay;
}

PlayerController *AppController::player()
{
    return m_player;
}

PlayQueueController *AppController::playQueue()
{
    return m_playQueue;
}

SettingsController *AppController::settings()
{
    return m_settings;
}

SessionController *AppController::session()
{
    return m_session;
}

QuickConnectController *AppController::quickConnect()
{
    return m_quickConnect;
}

LibraryManagementController *AppController::management()
{
    return m_management;
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
    co_await configureImagePrefetchAsync();
    co_await m_settings->loadLocalAsync();
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

void AppController::login()
{
    setErrorText({});
    m_session->login();
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
    emit currentLibraryNameChanged();
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

    qInfo() << "app: go home viewKind=" << currentViewKind();
    m_libraryLoadGeneration.invalidate();
    setBusy(false);
    m_discovery->stop();
    refreshHomeRows();
}

void AppController::openLibrary(int index)
{
    const auto library = m_libraries.libraryAt(index);
    if (library.id.isEmpty())
        return;

    const RequestGeneration::Token loadGeneration = m_libraryLoadGeneration.next();
    m_browse->enterLibrary(library, libraryContentLabel(library), defaultLibraryQuery(library));
    loadLibraryFilterOptions(loadGeneration, library);
    const QString cacheKey = libraryCacheKey(library, m_browse->query());
    m_browse->resetPaging(cacheKey);
    m_home->recordLibraryUse(library);
    emit currentLibraryNameChanged();
    const int cachedCount = m_browse->applyCachedPage(cacheKey);
    const bool hasWarmCache = cachedCount > 0;
    m_browse->setWarmCachePaging(cachedCount, kLibraryPageSize);
    m_prefetch->stop();
    if (m_artwork)
        m_artwork->cancelPrefetches();
    if (hasWarmCache) {
        qInfo() << "library open: showing cached page while refreshing" << library.name << cachedCount;
    } else {
        m_browse->setLoadingMore(true);
    }

    Async::runLatest(
        this, m_api->fetchBrowsePage(m_browse->descriptor(), 0, kLibraryPageSize, m_browse->query()),
        m_libraryLoadGeneration, loadGeneration,
        [this, cacheKey](const PagedMovieItems& page) { showCurrentItemsPage(page, cacheKey, false); },
        [this](const std::exception_ptr& error) {
            m_browse->setLoadingMore(false);
            const QString message = exceptionMessage(error);
            qWarning() << "library open: refresh failed" << message;
            showToast(message);
        });
}

void AppController::playOrOpen(const MovieItem& item, bool fromStart)
{
    if (item.id.isEmpty())
        return;
    if (item.itemType == QStringLiteral("Series")) {
        openSeries(item);
        return;
    }
    if (item.itemType == QStringLiteral("Season")) {
        openSeason(item);
        return;
    }
    if (item.itemType == QStringLiteral("Playlist")) {
        openPlaylist(item);
        return;
    }
    if (item.itemType == QStringLiteral("BoxSet")) {
        openBoxSet(item);
        return;
    }
    if (item.itemType == QStringLiteral("Folder")) {
        openFolder(item);
        return;
    }
    playQueuedItem(item, fromStart);
}

void AppController::playOrOpenFromModel(MovieGridModel *model, int index, bool fromStart)
{
    if (!model)
        return;
    const MovieItem item = model->movieAt(index);
    if (item.id.isEmpty())
        return;
    if (item.itemType == QStringLiteral("Series") || item.itemType == QStringLiteral("Season")
        || item.itemType == QStringLiteral("Playlist") || item.itemType == QStringLiteral("BoxSet")
        || item.itemType == QStringLiteral("Folder")) {
        playOrOpen(item, fromStart);
        return;
    }
    playQueuedItems(model->movies(), index, fromStart);
}

void AppController::playFromModel(MovieGridModel *model, int index, bool fromStart)
{
    playOrOpenFromModel(model, index, fromStart);
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
    if (seriesId.isEmpty())
        return;
    MovieItem series;
    series.id = seriesId;
    series.title = seriesName.isEmpty() ? QStringLiteral("Series") : seriesName;
    series.itemType = QStringLiteral("Series");
    series.playable = false;
    openSeries(series);
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
    season.playable = false;
    openSeason(season);
}

void AppController::maybeLoadMoreCurrentItems(int visibleIndex)
{
    if (visibleIndex < 0)
        return;
    if (visibleIndex + kLibraryPrefetchDistance < m_browse->rowCount())
        return;
    loadMoreCurrentItems();
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

void AppController::prefetchCurrentItems(int firstIndex, int lastIndex)
{
    m_browse->prefetchVisibleRange(firstIndex, lastIndex);
}

void AppController::setLibraryQuery(const QVariantMap& query)
{
    m_browse->setQuery(query);
}

void AppController::setLibrarySort(const QString& sortBy, const QString& sortOrder)
{
    QVariantMap query = m_browse->query();
    query.insert(QStringLiteral("sortBy"), sortBy.isEmpty() ? QStringLiteral("SortName") : sortBy);
    query.insert(QStringLiteral("sortOrder"),
        sortOrder == QStringLiteral("Descending") ? QStringLiteral("Descending") : QStringLiteral("Ascending"));
    setLibraryQuery(query);
    refreshCurrentLibrary();
}

void AppController::setLibraryQueryListValue(const QString& key, const QString& value, bool enabled)
{
    if (key.isEmpty() || value.isEmpty())
        return;

    QVariantMap query = m_browse->query();
    QStringList values = libraryQueryStringList(query, key);
    values.removeAll(value);
    if (enabled)
        values.push_back(value);
    values.removeDuplicates();

    if (values.isEmpty())
        query.remove(key);
    else
        query.insert(key, values);
    setLibraryQuery(query);
    refreshCurrentLibrary();
}

void AppController::setLibraryQueryBoolValue(const QString& key, bool enabled)
{
    if (key.isEmpty())
        return;

    QVariantMap query = m_browse->query();
    if (enabled)
        query.insert(key, true);
    else
        query.remove(key);
    setLibraryQuery(query);
    refreshCurrentLibrary();
}

void AppController::setLibraryQueryNullableBoolValue(const QString& key, const QVariant& value)
{
    if (key.isEmpty())
        return;

    QVariantMap query = m_browse->query();
    if (!value.isValid() || value.isNull())
        query.remove(key);
    else
        query.insert(key, value.toBool());
    setLibraryQuery(query);
    refreshCurrentLibrary();
}

void AppController::clearLibraryFilters()
{
    if (m_browse->libraryId().isEmpty())
        return;

    QVariantMap query;
    query.insert(QStringLiteral("sortBy"),
        m_browse->query().value(QStringLiteral("sortBy"), QStringLiteral("SortName")).toString());
    query.insert(QStringLiteral("sortOrder"),
        m_browse->query().value(QStringLiteral("sortOrder"), QStringLiteral("Ascending")).toString());
    setLibraryQuery(query);
    refreshCurrentLibrary();
}

void AppController::refreshCurrentLibrary()
{
    const BrowseDescriptor descriptor = m_browse->descriptor();
    if (!descriptor.isValid())
        return;
    if (!m_api || m_api->session().accessToken.isEmpty())
        return;

    const RequestGeneration::Token loadGeneration = m_libraryLoadGeneration.next();
    const QVariantMap query = descriptor.kind == BrowseKind::Library ? m_browse->query() : QVariantMap {};
    QString cacheKey = descriptor.cacheKey(query);
    if (descriptor.kind == BrowseKind::Library) {
        LibraryItem library;
        library.id = m_browse->libraryId();
        library.collectionType = m_browse->libraryCollectionType();
        cacheKey = libraryCacheKey(library, query);
    }
    m_browse->resetPaging(cacheKey);
    m_browse->clear();
    m_prefetch->stop();
    if (m_artwork)
        m_artwork->cancelPrefetches();
    m_browse->setLoadingMore(true);

    Async::runLatest(
        this, m_api->fetchBrowsePage(descriptor, 0, kLibraryPageSize, query), m_libraryLoadGeneration, loadGeneration,
        [this, cacheKey](const PagedMovieItems& page) { showCurrentItemsPage(page, cacheKey, false); },
        [this](const std::exception_ptr& error) {
            m_browse->setLoadingMore(false);
            showToast(exceptionMessage(error));
        });
}

void AppController::openDetailSeason(int index)
{
    const auto item = m_content->detailSeasonAt(index);
    if (item.id.isEmpty())
        return;
    openSeason(item);
}

void AppController::playDetailContext(bool shuffled)
{
    playQueuedModel(m_content->detailSeasons(), shuffled);
}

void AppController::setFavorite(const QString& itemId, bool favorite)
{
    if (itemId.isEmpty() || !m_api || m_api->session().accessToken.isEmpty())
        return;

    m_itemState->applyFavorite(itemId, favorite);
    Async::runScoped(
        this, m_api->setItemFavorite(itemId, favorite), []() {},
        [this, itemId, favorite](const std::exception_ptr& error) {
            m_itemState->applyFavorite(itemId, !favorite);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::setPlayed(const QString& itemId, bool played)
{
    if (itemId.isEmpty() || !m_api || m_api->session().accessToken.isEmpty())
        return;

    m_itemState->applyPlayed(itemId, played);
    Async::runScoped(
        this, m_api->setItemPlayed(itemId, played), []() {},
        [this, itemId, played](const std::exception_ptr& error) {
            m_itemState->applyPlayed(itemId, !played);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::clearProgress(const QString& itemId)
{
    if (itemId.isEmpty() || !m_api || m_api->session().accessToken.isEmpty())
        return;

    m_itemState->applyResumeTicks(itemId, 0);
    m_itemState->applyPlayed(itemId, false);
    Async::runScoped(
        this, m_api->setItemPlaybackPosition(itemId, 0), []() {},
        [this](const std::exception_ptr& error) { setErrorText(exceptionMessage(error)); });
}

void AppController::playQueuedItems(const std::vector<MovieItem>& items, int startIndex, bool fromStart)
{
    if (!queueMutationAllowed())
        return;
    if (!m_playQueue->playNow(items, startIndex)) {
        setErrorText(QStringLiteral("This item cannot be queued."));
        return;
    }
    playQueueCurrent(fromStart);
}

void AppController::playQueuedModel(MovieGridModel *model, bool shuffled)
{
    if (!model)
        return;
    const std::vector<MovieItem>& items = model->movies();
    const auto firstPlayable = std::find_if(
        items.begin(), items.end(), [](const MovieItem& item) { return !item.id.isEmpty() && item.playable; });
    if (firstPlayable == items.end()) {
        setErrorText(QStringLiteral("This list has no playable items."));
        return;
    }
    playQueuedItems(items, static_cast<int>(std::distance(items.begin(), firstPlayable)), false);
    if (shuffled)
        m_playQueue->setShuffled(true);
}

void AppController::playQueuedItem(const MovieItem& item, bool fromStart)
{
    if (!queueMutationAllowed())
        return;
    if (!m_playQueue->playNow(item)) {
        setErrorText(QStringLiteral("This item cannot be queued."));
        return;
    }
    playQueueCurrent(fromStart);
}

void AppController::playQueueCurrent(bool fromStart)
{
    const MovieItem item = m_playQueue->currentItem();
    if (item.id.isEmpty())
        return;
    playMediaItem(item, fromStart);
}

bool AppController::queueMutationAllowed()
{
    if (m_syncPlay && m_syncPlay->enabled()) {
        setErrorText(QStringLiteral("Leave SyncPlay before changing the play queue."));
        return false;
    }
    return true;
}

void AppController::playMediaItem(const MovieItem& item, bool fromStart)
{
    MovieItem playItem = item;
    if (fromStart || !isMeaningfulResumePosition(playItem.resumeTicks, playItem.runtimeTicks))
        playItem.resumeTicks = 0;
    m_activePlaybackItem = playItem;
    setBusy(true, QStringLiteral("Negotiating playback…"));
    Async::runScoped(
        this, startPlayback(playItem), []() {},
        [this](const std::exception_ptr& error) {
            setBusy(false);
            setErrorText(exceptionMessage(error));
        },
        "playback startup");
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

void AppController::showCurrentItemsPage(const PagedMovieItems& page, const QString& cacheKey, bool append)
{
    m_browse->setPage(page, cacheKey, append);
    setBusy(false);
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

void AppController::loadCurrentBrowsePage()
{
    if (!m_api || m_api->session().accessToken.isEmpty())
        return;
    const BrowseDescriptor descriptor = m_browse->descriptor();
    if (!descriptor.isValid())
        return;

    const RequestGeneration::Token loadGeneration = m_libraryLoadGeneration.next();
    const QString cacheKey = descriptor.cacheKey();
    m_browse->resetPaging(cacheKey);
    m_browse->clear();
    m_browse->setLoadingMore(true);

    Async::runLatest(
        this, m_api->fetchBrowsePage(descriptor, 0, kLibraryPageSize), m_libraryLoadGeneration, loadGeneration,
        [this, cacheKey](const PagedMovieItems& page) { showCurrentItemsPage(page, cacheKey, false); },
        [this](const std::exception_ptr& error) {
            m_browse->setLoadingMore(false);
            showToast(exceptionMessage(error));
        });
}

void AppController::openSeries(const MovieItem& series)
{
    if (series.id.isEmpty())
        return;

    const RequestGeneration::Token loadGeneration = m_libraryLoadGeneration.next();
    m_browse->enterSeries(series);
    emit currentLibraryNameChanged();
    const BrowseDescriptor descriptor = m_browse->descriptor();
    const QString cacheKey = descriptor.cacheKey();
    m_browse->resetPaging(cacheKey);
    m_browse->clear();
    m_browse->setLoadingMore(true);

    Async::runLatest(
        this, m_api->fetchBrowsePage(descriptor, 0, kLibraryPageSize), m_libraryLoadGeneration, loadGeneration,
        [this, series, cacheKey](const PagedMovieItems& page) {
            if (page.items.empty()) {
                MovieItem fallback;
                fallback.id = series.id;
                fallback.title = series.title;
                fallback.itemType = QStringLiteral("Series");
                fallback.seriesId = series.id;
                fallback.playable = false;
                openSeason(fallback);
                return;
            }
            showCurrentItemsPage(page, cacheKey, false);
        },
        [this](const std::exception_ptr& error) {
            m_browse->setLoadingMore(false);
            showToast(exceptionMessage(error));
        });
}

void AppController::openSeason(const MovieItem& season)
{
    const QString seriesId = !season.seriesId.isEmpty() ? season.seriesId : m_browse->seriesId();
    if (seriesId.isEmpty()) {
        qWarning() << "season open: missing series id" << season.id << season.title << season.itemType;
        return;
    }

    const RequestGeneration::Token loadGeneration = m_libraryLoadGeneration.next();
    qInfo() << "season open: loading episodes"
            << "series=" << seriesId << "season=" << season.id << "type=" << season.itemType
            << "title=" << season.title;
    m_browse->enterSeason(seriesId, season);
    emit currentLibraryNameChanged();
    const BrowseDescriptor descriptor = m_browse->descriptor();
    const QString cacheKey = descriptor.cacheKey();
    m_browse->resetPaging(cacheKey);
    m_browse->clear();
    m_browse->setLoadingMore(true);

    Async::runLatest(
        this, m_api->fetchBrowsePage(descriptor, 0, kLibraryPageSize), m_libraryLoadGeneration, loadGeneration,
        [this, seriesId, season, cacheKey](const PagedMovieItems& page) {
            qInfo() << "season open: episodes loaded"
                    << "series=" << seriesId << "season=" << season.id << "count=" << page.items.size();
            showCurrentItemsPage(page, cacheKey, false);
        },
        [this](const std::exception_ptr& error) {
            qWarning() << "season open: episodes fetch failed" << exceptionMessage(error);
            m_browse->setLoadingMore(false);
            showToast(exceptionMessage(error));
        });
}

void AppController::openPlaylist(const MovieItem& playlist)
{
    if (playlist.id.isEmpty())
        return;
    m_browse->enterPlaylist(playlist);
    emit currentLibraryNameChanged();
    loadCurrentBrowsePage();
}

void AppController::openBoxSet(const MovieItem& boxSet)
{
    if (boxSet.id.isEmpty())
        return;
    m_browse->enterBoxSet(boxSet);
    emit currentLibraryNameChanged();
    loadCurrentBrowsePage();
}

void AppController::openFolder(const MovieItem& folder)
{
    if (folder.id.isEmpty())
        return;
    m_browse->enterFolder(folder);
    emit currentLibraryNameChanged();
    loadCurrentBrowsePage();
}

void AppController::openGenre(const QString& genre)
{
    const QString name = genre.trimmed();
    if (name.isEmpty())
        return;
    m_browse->enterNamedCollection(QStringLiteral("genre"), name);
    emit currentLibraryNameChanged();
    loadCurrentBrowsePage();
}

void AppController::openStudio(const QString& studio)
{
    const QString name = studio.trimmed();
    if (name.isEmpty())
        return;
    m_browse->enterNamedCollection(QStringLiteral("studio"), name);
    emit currentLibraryNameChanged();
    loadCurrentBrowsePage();
}

void AppController::refreshHomeRows()
{
    m_home->refresh(m_libraries.libraries());
}

QString AppController::homePayloadCacheKey() const
{
    if (!m_api)
        return {};
    const AuthSession session = m_api->session();
    const QString userKey = session.userId.isEmpty() ? session.userName : session.userId;
    const QString serverKey = session.serverId.isEmpty() ? m_api->serverUrl() : session.serverId;
    if (userKey.isEmpty() || serverKey.isEmpty())
        return {};
    return QStringLiteral("%1/%2").arg(serverKey, userKey);
}

QCoro::Task<void> AppController::configureImagePrefetchAsync()
{
    if (!m_database || !m_prefetch)
        co_return;
    const int ahead
        = (co_await m_database->loadSettingAsync(QStringLiteral("network/imagePrefetchAhead"), QStringLiteral("16")))
              .toInt();
    const int concurrency = (co_await m_database->loadSettingAsync(
                                 QStringLiteral("network/imagePrefetchConcurrency"), QStringLiteral("3")))
                                .toInt();
    m_prefetch->configureImagePrefetch(ahead, concurrency);
}

QCoro::Task<void> AppController::applyCachedHomePayloadAsync()
{
    if (!m_database || !m_home)
        co_return;
    const QString key = homePayloadCacheKey();
    if (key.isEmpty())
        co_return;
    const QJsonObject payload = co_await m_database->loadHomePayloadAsync(key, kHomePayloadSchemaVersion);
    if (m_home->applyCachedPayload(payload))
        qInfo() << "home: warm payload cache applied" << key;
}

void AppController::saveHomePayload(const QJsonObject& payload)
{
    if (!m_database || payload.isEmpty())
        return;
    const QString key = homePayloadCacheKey();
    if (key.isEmpty())
        return;
    m_database->saveHomePayload(key, kHomePayloadSchemaVersion, payload);
}

void AppController::handlePlaybackStopped(const QString& itemId, qint64 positionTicks, bool completed)
{
    qInfo() << "app: playbackStopped itemId=" << itemId << "positionTicks=" << positionTicks
            << "completed=" << completed;

    if (completed) {
        m_itemState->applyPlayed(itemId, true);
        if (m_api && !m_api->session().accessToken.isEmpty()) {
            Async::runScoped(
                this, m_api->setItemPlayed(itemId, true), []() {},
                [](const std::exception_ptr& error) {
                    qWarning() << "app: completed playback mark-watched failed" << exceptionMessage(error);
                });
        }
        if (m_activePlaybackItem.id == itemId && (!m_syncPlay || !m_syncPlay->enabled())) {
            if (m_playQueue->next()) {
                playQueueCurrent(false);
                return;
            }
            enqueueEpisodeSuccessors(m_activePlaybackItem);
        }
    } else {
        m_itemState->applyResumeTicks(itemId, positionTicks);
        if (m_activePlaybackItem.id == itemId)
            m_home->upsertResumeItem(m_activePlaybackItem, positionTicks);
    }
}

void AppController::enqueueEpisodeSuccessors(const MovieItem& episode)
{
    if (episode.itemType != QStringLiteral("Episode") || episode.seriesId.isEmpty())
        return;
    if (!m_api || m_api->session().accessToken.isEmpty())
        return;

    Async::runScoped(
        this, m_api->fetchEpisodes(episode.seriesId),
        [this, episode](const std::vector<MovieItem>& episodes) {
            auto current = std::find_if(episodes.begin(), episodes.end(),
                [&episode](const MovieItem& candidate) { return candidate.id == episode.id; });
            if (current == episodes.end())
                return;

            std::vector<MovieItem> successors;
            for (++current; current != episodes.end(); ++current) {
                if (!current->id.isEmpty() && current->playable)
                    successors.push_back(*current);
            }
            if (successors.empty() || !m_playQueue->playNow(successors, 0))
                return;

            qInfo() << "app: auto-playing next episode" << successors.front().seriesName << successors.front().title
                    << "queued=" << successors.size();
            playQueueCurrent(false);
        },
        [](const std::exception_ptr& error) {
            qWarning() << "app: next episode queue lookup failed" << exceptionMessage(error);
        });
}

} // namespace JellyfinNative
