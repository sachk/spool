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
    m_home = new HomeModelController(api, m_prefetch, this);
    m_content = new ContentModelController(api, m_prefetch, this);
    m_search = new SearchController(api, m_prefetch, this);
    m_itemState = new UserItemStateController(m_browse, m_home, m_content, m_search, this);
    m_prefetch->configureImagePrefetch(
        database->loadSetting(QStringLiteral("network/imagePrefetchAhead"), QStringLiteral("16")).toInt(),
        database->loadSetting(QStringLiteral("network/imagePrefetchConcurrency"), QStringLiteral("3")).toInt());
    connect(m_api, &JellyfinApiFacade::authenticationExpired, m_session, &SessionController::expireSession);
    connect(m_syncPlay, &SyncPlayController::errorText, this, &AppController::setErrorText);
    connect(m_content, &ContentModelController::errorOccurred, this, &AppController::setErrorText);
    connect(m_search, &SearchController::errorOccurred, this, &AppController::setErrorText);
    connect(m_itemState, &UserItemStateController::favoriteChanged, this, &AppController::itemFavoriteChanged);
    connect(m_itemState, &UserItemStateController::playedChanged, this, &AppController::itemPlayedChanged);
    connect(m_home, &HomeModelController::homePayloadReady, this, &AppController::saveHomePayload);
    connect(m_quickConnect, &QuickConnectController::busyChanged, this, &AppController::setBusy);
    connect(m_quickConnect, &QuickConnectController::errorOccurred, this, &AppController::setErrorText);
    connect(m_settings, &SettingsController::errorOccurred, this, &AppController::setErrorText);
    connect(m_session, &SessionController::busyChanged, this, &AppController::setBusy);
    connect(m_session, &SessionController::errorOccurred, this, &AppController::setErrorText);
    connect(m_session, &SessionController::authenticatedChanged, this, [this](const AuthSession&) {
        if (m_artwork)
            m_artwork->setAuthorizationHeader(m_api->authorizationHeader());
        if (!m_hasDefaultProfile) {
            m_hasDefaultProfile = true;
            emit defaultProfileChanged();
        }
        applyCachedHomePayload();
        m_settings->loadRemote();
        m_syncPlay->connectSocket();
        loadLibraries();
        loadCurrentUserPolicy();
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

void AppController::initialize()
{
    Diagnostics::Task task(QStringLiteral("app_initialize"));
    m_settings->loadLocal();
    const bool hasDefaultProfile = !m_database->loadAuthSession().accessToken.isEmpty();
    if (m_hasDefaultProfile != hasDefaultProfile) {
        m_hasDefaultProfile = hasDefaultProfile;
        emit defaultProfileChanged();
    }
    if (!m_session->initialize()) {
        applyDiscoveredServersCache();
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
        const AuthSession session = m_database->loadAuthSession();
        if (session.accessToken.isEmpty() || m_session->serverUrl().isEmpty()) {
            setErrorText(QStringLiteral("This saved profile needs to sign in again."));
            return false;
        }
        m_session->acceptSession(session);
        return true;
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
    m_playlistTargets.clear();
    m_collectionTargets.clear();
    m_currentUserCanManagePlaylists = false;
    m_currentUserCanManageCollections = false;
    m_currentUserCanRenameItems = false;
    m_currentUserCanDeleteItems = false;
    m_libraryLoadGeneration.invalidate();
    m_browse->reset();
    setBusy(false);
    setErrorText({});
    if (m_hasDefaultProfile) {
        m_hasDefaultProfile = false;
        emit defaultProfileChanged();
    }
    emit currentLibraryNameChanged();
    emit managementPolicyChanged();
    emit managementTargetsChanged();
    applyDiscoveredServersCache();
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
        setBusy(false);
        qInfo() << "library open: showing cached page while refreshing" << library.name << cachedCount;
    } else {
        setBusy(true, QStringLiteral("Loading %1…").arg(m_browse->contentLabel().toLower()));
    }

    Async::runLatest(
        this, m_api->fetchBrowsePage(m_browse->descriptor(), 0, kLibraryPageSize, m_browse->query()),
        m_libraryLoadGeneration, loadGeneration,
        [this, cacheKey](const PagedMovieItems& page) { showCurrentItemsPage(page, cacheKey, false); },
        [this, hasWarmCache](const std::exception_ptr& error) {
            setBusy(false);
            m_browse->setLoadingMore(false);
            const QString message = exceptionMessage(error);
            if (hasWarmCache)
                qWarning() << "library open: background refresh failed" << message;
            else
                setErrorText(message);
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
        setErrorText(exceptionMessage(error));
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
    setBusy(true, QStringLiteral("Loading %1…").arg(m_browse->contentLabel().toLower()));

    Async::runLatest(
        this, m_api->fetchBrowsePage(descriptor, 0, kLibraryPageSize, query), m_libraryLoadGeneration, loadGeneration,
        [this, cacheKey](const PagedMovieItems& page) { showCurrentItemsPage(page, cacheKey, false); },
        [this](const std::exception_ptr& error) {
            setBusy(false);
            m_browse->setLoadingMore(false);
            setErrorText(exceptionMessage(error));
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

void AppController::loadCurrentUserPolicy()
{
    if (!m_api || m_api->session().accessToken.isEmpty()) {
        m_currentUserCanManagePlaylists = false;
        m_currentUserCanManageCollections = false;
        m_currentUserCanRenameItems = false;
        m_currentUserCanDeleteItems = false;
        emit managementPolicyChanged();
        return;
    }

    m_currentUserCanManagePlaylists = true;
    emit managementPolicyChanged();
    Async::runScoped(
        this, m_api->fetchCurrentUserPolicy(),
        [this](const QJsonObject& policy) {
            const bool administrator = policy.value(QStringLiteral("IsAdministrator")).toBool(false);
            m_currentUserCanManagePlaylists = !m_api->session().accessToken.isEmpty();
            m_currentUserCanManageCollections
                = administrator || policy.value(QStringLiteral("EnableCollectionManagement")).toBool(false);
            m_currentUserCanRenameItems = administrator;
            m_currentUserCanDeleteItems
                = administrator || policy.value(QStringLiteral("EnableContentDeletion")).toBool(false);
            emit managementPolicyChanged();
        },
        [this](const std::exception_ptr& error) {
            qWarning() << "management policy fetch failed" << exceptionMessage(error);
            m_currentUserCanManagePlaylists = false;
            m_currentUserCanManageCollections = false;
            m_currentUserCanRenameItems = false;
            m_currentUserCanDeleteItems = false;
            emit managementPolicyChanged();
        });
}

void AppController::setManagementTargets(const QString& kind, const std::vector<MovieItem>& items)
{
    QVariantList targets;
    targets.reserve(static_cast<qsizetype>(items.size()));
    for (const MovieItem& item : items)
        targets.push_back(metaToJson(item).toVariantMap());

    if (kind == QStringLiteral("playlist"))
        m_playlistTargets = targets;
    else if (kind == QStringLiteral("collection"))
        m_collectionTargets = targets;
    emit managementTargetsChanged();
}

QStringList AppController::itemIdsForManagement(const MovieItem& item) const
{
    return item.id.isEmpty() ? QStringList {} : QStringList { item.id };
}

void AppController::refreshAfterManagementMutation(const QString& changedItemId)
{
    if (!changedItemId.isEmpty() && m_browse->descriptor().id == changedItemId) {
        goHome();
    } else {
        refreshCurrentLibrary();
    }
    refreshManagementTargets(QStringLiteral("playlist"));
    refreshManagementTargets(QStringLiteral("collection"));
}

bool AppController::authenticatedForManagement()
{
    if (!m_api || m_api->session().accessToken.isEmpty()) {
        setErrorText(QStringLiteral("Sign in before managing library items."));
        return false;
    }
    return true;
}

bool AppController::playlistMutationAllowed()
{
    if (!authenticatedForManagement())
        return false;
    if (!m_currentUserCanManagePlaylists) {
        setErrorText(QStringLiteral("Your Jellyfin user cannot manage playlists."));
        return false;
    }
    return true;
}

bool AppController::collectionMutationAllowed()
{
    if (!authenticatedForManagement())
        return false;
    if (!m_currentUserCanManageCollections) {
        setErrorText(QStringLiteral("Your Jellyfin user cannot manage collections."));
        return false;
    }
    return true;
}

bool AppController::renameMutationAllowed(const QString& itemType)
{
    if (itemType == QStringLiteral("Playlist"))
        return playlistMutationAllowed();
    if (!authenticatedForManagement())
        return false;
    if (!m_currentUserCanRenameItems) {
        setErrorText(QStringLiteral("Your Jellyfin user cannot rename this item."));
        return false;
    }
    return true;
}

bool AppController::deleteMutationAllowed()
{
    if (!authenticatedForManagement())
        return false;
    if (!m_currentUserCanDeleteItems) {
        setErrorText(QStringLiteral("Your Jellyfin user cannot delete items."));
        return false;
    }
    return true;
}

void AppController::refreshManagementTargets(const QString& kind)
{
    if (!authenticatedForManagement())
        return;

    const QString normalized
        = kind == QStringLiteral("collection") ? QStringLiteral("collection") : QStringLiteral("playlist");
    const QString itemType
        = normalized == QStringLiteral("collection") ? QStringLiteral("BoxSet") : QStringLiteral("Playlist");
    Async::runScoped(
        this, m_api->fetchManagementTargets(itemType),
        [this, normalized](const std::vector<MovieItem>& items) { setManagementTargets(normalized, items); },
        [this](const std::exception_ptr& error) { setErrorText(exceptionMessage(error)); });
}

void AppController::createPlaylistForItem(const QString& name, const MovieItem& item)
{
    if (!playlistMutationAllowed())
        return;
    const QStringList itemIds = itemIdsForManagement(item);
    setBusy(true, QStringLiteral("Creating playlist…"));
    Async::runScoped(
        this, m_api->createPlaylist(name, itemIds),
        [this](const QString&) {
            setBusy(false);
            emit managementOperationSucceeded(QStringLiteral("Playlist created"));
            refreshAfterManagementMutation();
        },
        [this](const std::exception_ptr& error) {
            setBusy(false);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::addItemToPlaylist(const QString& playlistId, const MovieItem& item)
{
    if (!playlistMutationAllowed())
        return;
    const QStringList itemIds = itemIdsForManagement(item);
    if (playlistId.isEmpty() || itemIds.isEmpty()) {
        setErrorText(QStringLiteral("Choose an item and playlist first."));
        return;
    }
    setBusy(true, QStringLiteral("Adding to playlist…"));
    Async::runScoped(
        this, m_api->addPlaylistItems(playlistId, itemIds),
        [this]() {
            setBusy(false);
            emit managementOperationSucceeded(QStringLiteral("Added to playlist"));
            refreshAfterManagementMutation();
        },
        [this](const std::exception_ptr& error) {
            setBusy(false);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::createCollectionForItem(const QString& name, const MovieItem& item)
{
    if (!collectionMutationAllowed())
        return;
    const QStringList itemIds = itemIdsForManagement(item);
    setBusy(true, QStringLiteral("Creating collection…"));
    Async::runScoped(
        this, m_api->createCollection(name, itemIds),
        [this](const QString&) {
            setBusy(false);
            emit managementOperationSucceeded(QStringLiteral("Collection created"));
            refreshAfterManagementMutation();
        },
        [this](const std::exception_ptr& error) {
            setBusy(false);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::addItemToCollection(const QString& collectionId, const MovieItem& item)
{
    if (!collectionMutationAllowed())
        return;
    const QStringList itemIds = itemIdsForManagement(item);
    if (collectionId.isEmpty() || itemIds.isEmpty()) {
        setErrorText(QStringLiteral("Choose an item and collection first."));
        return;
    }
    setBusy(true, QStringLiteral("Adding to collection…"));
    Async::runScoped(
        this, m_api->addCollectionItems(collectionId, itemIds),
        [this]() {
            setBusy(false);
            emit managementOperationSucceeded(QStringLiteral("Added to collection"));
            refreshAfterManagementMutation();
        },
        [this](const std::exception_ptr& error) {
            setBusy(false);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::removeItemFromCurrentParent(const MovieItem& movie)
{
    const BrowseDescriptor descriptor = m_browse->descriptor();
    if (descriptor.kind == BrowseKind::Playlist) {
        if (!playlistMutationAllowed())
            return;
        if (movie.playlistItemId.isEmpty()) {
            setErrorText(QStringLiteral("This playlist entry cannot be removed."));
            return;
        }
        setBusy(true, QStringLiteral("Removing from playlist…"));
        Async::runScoped(
            this, m_api->removePlaylistItems(descriptor.id, { movie.playlistItemId }),
            [this]() {
                setBusy(false);
                emit managementOperationSucceeded(QStringLiteral("Removed from playlist"));
                refreshAfterManagementMutation();
            },
            [this](const std::exception_ptr& error) {
                setBusy(false);
                setErrorText(exceptionMessage(error));
            });
        return;
    }
    if (descriptor.kind == BrowseKind::BoxSet) {
        if (!collectionMutationAllowed())
            return;
        if (movie.id.isEmpty()) {
            setErrorText(QStringLiteral("This collection item cannot be removed."));
            return;
        }
        setBusy(true, QStringLiteral("Removing from collection…"));
        Async::runScoped(
            this, m_api->removeCollectionItems(descriptor.id, { movie.id }),
            [this]() {
                setBusy(false);
                emit managementOperationSucceeded(QStringLiteral("Removed from collection"));
                refreshAfterManagementMutation();
            },
            [this](const std::exception_ptr& error) {
                setBusy(false);
                setErrorText(exceptionMessage(error));
            });
        return;
    }
    setErrorText(QStringLiteral("Open a playlist or collection before removing items."));
}

void AppController::movePlaylistItemInCurrent(const MovieItem& movie, int delta)
{
    if (!playlistMutationAllowed())
        return;
    const BrowseDescriptor descriptor = m_browse->descriptor();
    if (descriptor.kind != BrowseKind::Playlist || movie.playlistItemId.isEmpty()) {
        setErrorText(QStringLiteral("This playlist entry cannot be moved."));
        return;
    }

    int currentIndex = -1;
    const std::vector<MovieItem>& items = m_browse->items()->movies();
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        if (items[static_cast<size_t>(i)].playlistItemId == movie.playlistItemId) {
            currentIndex = i;
            break;
        }
    }
    if (currentIndex < 0)
        return;
    const int newIndex = std::clamp(currentIndex + delta, 0, std::max(0, static_cast<int>(items.size()) - 1));
    if (newIndex == currentIndex)
        return;

    setBusy(true, QStringLiteral("Moving playlist item…"));
    Async::runScoped(
        this, m_api->movePlaylistItem(descriptor.id, movie.playlistItemId, newIndex),
        [this]() {
            setBusy(false);
            emit managementOperationSucceeded(QStringLiteral("Playlist item moved"));
            refreshAfterManagementMutation();
        },
        [this](const std::exception_ptr& error) {
            setBusy(false);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::renameManagedItem(const MovieItem& movie, const QString& name)
{
    const QString trimmed = name.trimmed();
    if (movie.id.isEmpty() || trimmed.isEmpty()) {
        setErrorText(QStringLiteral("Choose an item and name first."));
        return;
    }
    if (!renameMutationAllowed(movie.itemType))
        return;

    setBusy(true, QStringLiteral("Renaming item…"));
    auto onRenamed = [this, itemId = movie.id]() {
        setBusy(false);
        emit managementOperationSucceeded(QStringLiteral("Item renamed"));
        refreshAfterManagementMutation(itemId);
    };
    auto onError = [this](const std::exception_ptr& error) {
        setBusy(false);
        setErrorText(exceptionMessage(error));
    };
    if (movie.itemType == QStringLiteral("Playlist")) {
        Async::runScoped(this, m_api->updatePlaylistName(movie.id, trimmed), onRenamed, onError);
        return;
    }
    Async::runScoped(this, m_api->renameItem(movie.id, trimmed), onRenamed, onError);
}

void AppController::deleteManagedItem(const MovieItem& movie)
{
    if (movie.id.isEmpty()) {
        setErrorText(QStringLiteral("Choose an item before deleting."));
        return;
    }
    if (!deleteMutationAllowed())
        return;

    setBusy(true, QStringLiteral("Deleting item…"));
    Async::runScoped(
        this, m_api->deleteItem(movie.id),
        [this, itemId = movie.id]() {
            setBusy(false);
            emit managementOperationSucceeded(QStringLiteral("Item deleted"));
            refreshAfterManagementMutation(itemId);
        },
        [this](const std::exception_ptr& error) {
            setBusy(false);
            setErrorText(exceptionMessage(error));
        });
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
    Diagnostics::Task task(QStringLiteral("playback_negotiate"),
        { { QStringLiteral("itemId"), item.id }, { QStringLiteral("title"), item.title },
            { QStringLiteral("type"), item.itemType } });
    setBusy(true, QStringLiteral("Negotiating playback…"));
    MovieItem playItem = item;
    if (fromStart || !isMeaningfulResumePosition(playItem.resumeTicks, playItem.runtimeTicks))
        playItem.resumeTicks = 0;
    m_activePlaybackItem = playItem;
    const QString itemId = playItem.id;
    Async::runScoped(
        this, m_api->negotiatePlayback(playItem),
        [this, itemId](const PlaybackSession& session) {
            // Enrich the negotiated session with media segments (skip-intro)
            // and trickplay (scrubber thumbnails). Both are advisory — if the
            // server doesn't expose them, playback still starts normally.
            JellyfinApiFacade *api = m_api;
            PlayerController *player = m_player;
            const QString mediaSourceId = session.mediaSourceId;
            auto sharedSession = std::make_shared<PlaybackSession>(session);
            sharedSession->nowPlayingQueue = m_playQueue->nowPlayingQueue();
            auto pending = std::make_shared<int>(2);
            auto kickoff = [player, sharedSession, pending]() {
                if (--(*pending) == 0)
                    player->play(*sharedSession);
            };
            Async::runScoped(
                this, api->fetchMediaSegments(itemId),
                [sharedSession, kickoff](const std::vector<MediaSegment>& segments) {
                    sharedSession->segments = segments;
                    kickoff();
                },
                [kickoff](const std::exception_ptr&) { kickoff(); });
            Async::runScoped(
                this, api->fetchTrickplay(itemId, mediaSourceId),
                [sharedSession, kickoff](const TrickplayInfo& info) {
                    sharedSession->trickplay = info;
                    kickoff();
                },
                [kickoff](const std::exception_ptr&) { kickoff(); });
            setBusy(false);
        },
        [this](const std::exception_ptr& error) {
            setBusy(false);
            setErrorText(exceptionMessage(error));
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

void AppController::applyDiscoveredServersCache()
{
    const auto servers = m_database->loadDiscoveredServers();
    std::vector<DiscoveredServer> parsed;
    parsed.reserve(servers.size());
    for (const auto& value : servers)
        parsed.push_back(metaFromJson<DiscoveredServer>(value.toObject()));
    m_discoveredServers.setServers(parsed);
}

void AppController::loadLibraries()
{
    m_prefetch->stop();
    setBusy(true, QStringLiteral("Loading libraries…"));
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

void AppController::loadCurrentBrowsePage(const QString& loadingText)
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
    setBusy(true, loadingText);

    Async::runLatest(
        this, m_api->fetchBrowsePage(descriptor, 0, kLibraryPageSize), m_libraryLoadGeneration, loadGeneration,
        [this, cacheKey](const PagedMovieItems& page) { showCurrentItemsPage(page, cacheKey, false); },
        [this](const std::exception_ptr& error) {
            setBusy(false);
            setErrorText(exceptionMessage(error));
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
    setBusy(true, QStringLiteral("Loading seasons…"));

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
            setBusy(false);
            setErrorText(exceptionMessage(error));
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
    setBusy(true, QStringLiteral("Loading episodes…"));

    Async::runLatest(
        this, m_api->fetchBrowsePage(descriptor, 0, kLibraryPageSize), m_libraryLoadGeneration, loadGeneration,
        [this, seriesId, season, cacheKey](const PagedMovieItems& page) {
            qInfo() << "season open: episodes loaded"
                    << "series=" << seriesId << "season=" << season.id << "count=" << page.items.size();
            showCurrentItemsPage(page, cacheKey, false);
        },
        [this](const std::exception_ptr& error) {
            qWarning() << "season open: episodes fetch failed" << exceptionMessage(error);
            setBusy(false);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::openPlaylist(const MovieItem& playlist)
{
    if (playlist.id.isEmpty())
        return;
    m_browse->enterPlaylist(playlist);
    emit currentLibraryNameChanged();
    loadCurrentBrowsePage(QStringLiteral("Loading %1…").arg(m_browse->contentLabel().toLower()));
}

void AppController::openBoxSet(const MovieItem& boxSet)
{
    if (boxSet.id.isEmpty())
        return;
    m_browse->enterBoxSet(boxSet);
    emit currentLibraryNameChanged();
    loadCurrentBrowsePage(QStringLiteral("Loading %1…").arg(m_browse->contentLabel().toLower()));
}

void AppController::openFolder(const MovieItem& folder)
{
    if (folder.id.isEmpty())
        return;
    m_browse->enterFolder(folder);
    emit currentLibraryNameChanged();
    loadCurrentBrowsePage(QStringLiteral("Loading %1…").arg(m_browse->contentLabel().toLower()));
}

void AppController::openGenre(const QString& genre)
{
    const QString name = genre.trimmed();
    if (name.isEmpty())
        return;
    m_browse->enterNamedCollection(QStringLiteral("genre"), name);
    emit currentLibraryNameChanged();
    loadCurrentBrowsePage(QStringLiteral("Loading %1…").arg(name));
}

void AppController::openStudio(const QString& studio)
{
    const QString name = studio.trimmed();
    if (name.isEmpty())
        return;
    m_browse->enterNamedCollection(QStringLiteral("studio"), name);
    emit currentLibraryNameChanged();
    loadCurrentBrowsePage(QStringLiteral("Loading %1…").arg(name));
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

void AppController::applyCachedHomePayload()
{
    if (!m_database || !m_home)
        return;
    const QString key = homePayloadCacheKey();
    if (key.isEmpty())
        return;
    const QJsonObject payload = m_database->loadHomePayload(key, kHomePayloadSchemaVersion);
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
