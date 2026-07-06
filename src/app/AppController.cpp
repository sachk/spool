#include "AppController.h"

#include "ArtworkService.h"

#include "../api/JellyfinApiFacade.h"
#include "../common/AsyncTask.h"
#include "../diagnostics/Diagnostics.h"
#include "../player/PlayerController.h"
#include "ContentModelController.h"
#include "CurrentItemsController.h"
#include "HomeModelController.h"
#include "LibraryQuery.h"
#include "LibraryPrefetchController.h"
#include "QuickConnectController.h"
#include "SessionController.h"
#include "SearchController.h"
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
constexpr int kHomePayloadSchemaVersion = 1;

}

AppController::AppController(DatabaseManager *database,
                             DiscoveryController *discovery,
                             JellyfinApiFacade *api,
                             ArtworkService *artwork,
                             PlayerController *player,
                             QObject *parent)
    : QObject(parent)
    , m_database(database)
    , m_discovery(discovery)
    , m_api(api)
    , m_artwork(artwork)
    , m_player(player)
{
    m_syncPlay = new SyncPlayController(api, player, this);
    m_quickConnect = new QuickConnectController(api, this);
    m_settings = new SettingsController(database, api, player, this);
    m_session = new SessionController(database, api, this);
    m_prefetch = new LibraryPrefetchController(api, artwork, this);
    m_currentItems = new CurrentItemsController(m_prefetch, this);
    m_home = new HomeModelController(api, m_prefetch, this);
    m_content = new ContentModelController(api, m_prefetch, this);
    m_search = new SearchController(api, m_prefetch, this);
    m_itemState =
        new UserItemStateController(m_currentItems, m_home, m_content, m_search, this);
    m_prefetch->configureImagePrefetch(
        database->loadSetting(QStringLiteral("network/imagePrefetchAhead"),
                              QStringLiteral("16")).toInt(),
        database->loadSetting(QStringLiteral("network/imagePrefetchConcurrency"),
                              QStringLiteral("3")).toInt());
    connect(m_api, &JellyfinApiFacade::authenticationExpired,
            m_session, &SessionController::expireSession);
    connect(m_syncPlay, &SyncPlayController::errorText, this, &AppController::setErrorText);
    connect(m_content, &ContentModelController::detailRowsChanged,
            this, &AppController::detailRowsChanged);
    connect(m_content, &ContentModelController::detailItemChanged,
            this, &AppController::detailItemChanged);
    connect(m_content, &ContentModelController::personItemsChanged,
            this, &AppController::personItemsChanged);
    connect(m_content, &ContentModelController::errorOccurred,
            this, &AppController::setErrorText);
    connect(m_search, &SearchController::errorOccurred,
            this, &AppController::setErrorText);
    connect(m_itemState, &UserItemStateController::favoriteChanged,
            this, &AppController::itemFavoriteChanged);
    connect(m_itemState, &UserItemStateController::playedChanged,
            this, &AppController::itemPlayedChanged);
    connect(m_currentItems, &CurrentItemsController::pagingChanged,
            this, &AppController::currentItemsPagingChanged);
    connect(m_home, &HomeModelController::latestLibraryRowsChanged,
            this, &AppController::latestLibraryRowsChanged);
    connect(m_home, &HomeModelController::homePayloadReady,
            this, &AppController::saveHomePayload);
    connect(m_quickConnect, &QuickConnectController::busyChanged,
            this, &AppController::setBusy);
    connect(m_quickConnect, &QuickConnectController::errorOccurred,
            this, &AppController::setErrorText);
    connect(m_settings, &SettingsController::errorOccurred,
            this, &AppController::setErrorText);
    connect(m_session, &SessionController::busyChanged,
            this, &AppController::setBusy);
    connect(m_session, &SessionController::errorOccurred,
            this, &AppController::setErrorText);
    connect(m_session, &SessionController::authenticatedChanged, this,
            [this](const AuthSession &) {
                if (!m_hasDefaultProfile) {
                    m_hasDefaultProfile = true;
                    emit defaultProfileChanged();
                }
                applyCachedHomePayload();
                m_settings->loadRemote();
                m_syncPlay->connectSocket();
                loadLibraries();
            });
    connect(m_session, &SessionController::loggedOut,
            this, &AppController::resetApplicationState);
    connect(m_quickConnect, &QuickConnectController::authenticated, this,
            [this](const AuthSession &session) {
                m_session->acceptSession(session);
            });
    connect(m_discovery, &DiscoveryController::serverDiscovered, this, [this](const DiscoveredServer &server) {
        m_discoveredServers.upsertServer(server);
        QJsonArray cache;
        for (const auto &entry : m_discoveredServers.servers())
            cache.push_back(toJson(entry));
        m_database->saveDiscoveredServers(cache);
    });

    connect(m_player, &PlayerController::playbackStopped, this,
            &AppController::handlePlaybackStopped);
}

QString AppController::page() const
{
    return m_navigation.page();
}

bool AppController::busy() const
{
    return m_busy;
}

QString AppController::busyText() const
{
    return m_busyText;
}

QString AppController::errorText() const
{
    return m_errorText;
}

bool AppController::hasDefaultProfile() const
{
    return m_hasDefaultProfile;
}

QString AppController::currentLibraryName() const
{
    return m_navigation.title();
}

QString AppController::currentContentLabel() const
{
    return m_navigation.contentLabel();
}

QString AppController::currentViewKind() const
{
    return m_navigation.viewKind();
}

QString AppController::currentLibraryId() const
{
    return m_navigation.libraryId();
}

QString AppController::currentLibraryCollectionType() const
{
    return m_navigation.libraryCollectionType();
}

QVariantMap AppController::libraryQuery() const
{
    return m_navigation.query();
}

QVariantMap AppController::libraryFilterOptions() const
{
    return m_navigation.filterOptions();
}

int AppController::libraryFilterActiveCount() const
{
    return activeLibraryFilterCount(m_navigation.query());
}

DiscoveredServerModel *AppController::discoveredServers()
{
    return &m_discoveredServers;
}

LibraryListModel *AppController::libraries()
{
    return &m_libraries;
}

MovieGridModel *AppController::movies()
{
    return m_currentItems->items();
}

MovieGridModel *AppController::resumeItems()
{
    return m_home->resumeItems();
}

MovieGridModel *AppController::nextUpItems()
{
    return m_home->nextUpItems();
}

QVariantList AppController::latestLibraryRows() const
{
    return m_home->latestLibraryRows();
}


bool AppController::currentItemsLoadingMore() const
{
    return m_currentItems->loadingMore();
}

bool AppController::currentItemsHasMore() const
{
    return m_currentItems->hasMore();
}

int AppController::currentItemsTotalCount() const
{
    return m_currentItems->totalCount();
}

MovieGridModel *AppController::detailSeasons()
{
    return m_content->detailSeasons();
}

MovieGridModel *AppController::detailSimilarItems()
{
    return m_content->detailSimilarItems();
}

QVariantMap AppController::detailItem() const
{
    return m_content->detailItem();
}

MovieGridModel *AppController::personItems()
{
    return m_content->personItems();
}

bool AppController::detailRowsBusy() const
{
    return m_content->detailRowsBusy();
}

bool AppController::personItemsBusy() const
{
    return m_content->personItemsBusy();
}


SearchController *AppController::searchController()
{
    return m_search;
}

SyncPlayController *AppController::syncPlay() { return m_syncPlay; }

PlayerController *AppController::player()
{
    return m_player;
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

void AppController::useDefaultProfile()
{
    setErrorText({});
    m_quickConnect->cancel();

    if (!m_session->authenticated()) {
        const AuthSession session = m_database->loadAuthSession();
        if (session.accessToken.isEmpty() || m_session->serverUrl().isEmpty()) {
            setErrorText(QStringLiteral("This saved profile needs to sign in again."));
            return;
        }
        m_session->acceptSession(session);
        return;
    }

    if (m_libraries.rowCount() <= 0) {
        loadLibraries();
        return;
    }

    setBusy(false);
    setPage(QStringLiteral("libraries"));
    refreshHomeRows();
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
    setPage(QStringLiteral("login"));
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
    if (m_artwork)
        m_artwork->cancelPrefetches();
    if (m_database)
        m_database->invalidateHomePayloads();
    m_libraries.clear();
    m_currentItems->clear();
    m_home->reset();
    m_content->reset();
    m_search->reset();
    m_activePlaybackItem = {};
    m_libraryLoadGeneration.invalidate();
    const bool pageWasLogin = page() == QStringLiteral("login");
    m_navigation.reset();
    m_currentItems->resetPaging();
    setBusy(false);
    setErrorText({});
    if (m_hasDefaultProfile) {
        m_hasDefaultProfile = false;
        emit defaultProfileChanged();
    }
    emit currentLibraryNameChanged();
    emit libraryQueryChanged();
    emit libraryFilterOptionsChanged();
    if (!pageWasLogin)
        emit pageChanged();
    applyDiscoveredServersCache();
    m_discovery->start();
}


void AppController::goHome()
{
    if (page() == QStringLiteral("login"))
        return;

    qInfo() << "app: go home from page=" << page() << "viewKind=" << currentViewKind();
    m_libraryLoadGeneration.invalidate();
    setBusy(false);
    setPage(QStringLiteral("libraries"));
    refreshHomeRows();
}

void AppController::openLibrary(int index)
{
    const auto library = m_libraries.libraryAt(index);
    if (library.id.isEmpty())
        return;

    const RequestGeneration::Token loadGeneration = m_libraryLoadGeneration.next();
    m_navigation.enterLibrary(library, libraryContentLabel(library), defaultLibraryQuery(library));
    emit libraryQueryChanged();
    emit libraryFilterOptionsChanged();
    loadLibraryFilterOptions(loadGeneration, library);
    const QString cacheKey = libraryCacheKey(library, libraryQuery());
    m_currentItems->resetPaging(cacheKey);
    m_home->recordLibraryUse(library);
    emit currentLibraryNameChanged();
    const int cachedCount = m_currentItems->applyCachedPage(cacheKey);
    const bool hasWarmCache = cachedCount > 0;
    m_currentItems->setWarmCachePaging(cachedCount, kLibraryPageSize);
    m_prefetch->stop();
    if (m_artwork)
        m_artwork->cancelPrefetches();
    if (hasWarmCache) {
        setBusy(false);
        setPage(QStringLiteral("movies"));
        qInfo() << "library open: showing cached page while refreshing" << library.name << cachedCount;
    } else {
        setBusy(true, QStringLiteral("Loading %1…").arg(currentContentLabel().toLower()));
    }

    Async::runLatest(this,
        m_api->fetchLibraryPage(library.id, library.collectionType, 0, kLibraryPageSize, libraryQuery()),
        m_libraryLoadGeneration, loadGeneration,
        [this, cacheKey](const PagedMovieItems &page) {
            showCurrentItemsPage(page, cacheKey, false);
        },
        [this, hasWarmCache](const std::exception_ptr &error) {
            setBusy(false);
            m_currentItems->setLoadingMore(false);
            const QString message = exceptionMessage(error);
            if (hasWarmCache)
                qWarning() << "library open: background refresh failed" << message;
            else
                setErrorText(message);
            if (page() != QStringLiteral("movies"))
                setPage(QStringLiteral("movies"));
        });
}

void AppController::playOrOpen(const MovieItem &item, bool fromStart)
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
    playMediaItem(item, fromStart);
}

void AppController::playMovie(int index, bool fromStart)
{
    playOrOpen(m_currentItems->itemAt(index), fromStart);
}

void AppController::playResumeItem(int index, bool fromStart)
{
    const auto item = m_home->resumeItemAt(index);
    qInfo() << "app: play resume item index=" << index
            << "type=" << item.itemType << "title=" << item.title
            << "resumeTicks=" << item.resumeTicks << "fromStart=" << fromStart;
    if (item.id.isEmpty())
        return;
    playMediaItem(item, fromStart);
}

void AppController::playNextUpItem(int index, bool fromStart)
{
    const auto item = m_home->nextUpItemAt(index);
    qInfo() << "app: play next-up item index=" << index
            << "type=" << item.itemType << "title=" << item.title
            << "resumeTicks=" << item.resumeTicks << "fromStart=" << fromStart;
    if (item.id.isEmpty())
        return;
    playMediaItem(item, fromStart);
}

QObject *AppController::latestLibraryItems(int rowIndex)
{
    return m_home->latestLibraryItems(rowIndex);
}

void AppController::playLatestLibraryItem(int rowIndex, int itemIndex, bool fromStart)
{
    playOrOpen(m_home->latestLibraryItemAt(rowIndex, itemIndex), fromStart);
}

void AppController::openSeriesById(const QString &seriesId, const QString &seriesName)
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

void AppController::openSeasonById(const QString &seriesId, const QString &seasonId, const QString &seasonName)
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


void AppController::playSuggestionItem(int index, bool fromStart)
{
    playOrOpen(m_search->suggestionItemAt(index), fromStart);
}

void AppController::playSearchResult(int index, bool fromStart)
{
    playOrOpen(m_search->resultItemAt(index), fromStart);
}

void AppController::maybeLoadMoreCurrentItems(int visibleIndex)
{
    if (visibleIndex < 0)
        return;
    if (visibleIndex + kLibraryPrefetchDistance < m_currentItems->rowCount())
        return;
    loadMoreCurrentItems();
}

void AppController::loadMoreCurrentItems()
{
    if (m_currentItems->loadingMore() || !m_currentItems->hasMore())
        return;
    if (!m_api || m_api->session().accessToken.isEmpty())
        return;
    if (currentViewKind() != QStringLiteral("library"))
        return;

    const int startIndex = std::max(m_currentItems->nextStartIndex(),
                                    m_currentItems->rowCount());
    const RequestGeneration::Token loadGeneration = m_libraryLoadGeneration.current();
    const QString libraryId = currentLibraryId();
    const QString collectionType = currentLibraryCollectionType();
    const QString cacheKey = m_currentItems->cacheKey();
    const QVariantMap query = libraryQuery();
    if (m_artwork)
        m_artwork->cancelPrefetches();
    m_currentItems->setLoadingMore(true);

    const auto onDone = [this, cacheKey](const PagedMovieItems &page) {
        showCurrentItemsPage(page, cacheKey, true);
    };
    const auto onError = [this](const std::exception_ptr &error) {
        m_currentItems->setLoadingMore(false);
        setErrorText(exceptionMessage(error));
    };

    Async::runLatest(this,
                     m_api->fetchLibraryPage(libraryId, collectionType, startIndex,
                                             kLibraryPageSize, query),
                     m_libraryLoadGeneration, loadGeneration, onDone, onError);
}

void AppController::prefetchCurrentItems(int firstIndex, int lastIndex)
{
    m_currentItems->prefetchVisibleRange(firstIndex, lastIndex);
}

void AppController::setLibraryQuery(const QVariantMap &query)
{
    if (!m_navigation.setQuery(query))
        return;
    emit libraryQueryChanged();
}

void AppController::setLibrarySort(const QString &sortBy, const QString &sortOrder)
{
    QVariantMap query = libraryQuery();
    query.insert(QStringLiteral("sortBy"), sortBy.isEmpty() ? QStringLiteral("SortName") : sortBy);
    query.insert(QStringLiteral("sortOrder"), sortOrder == QStringLiteral("Descending")
                                      ? QStringLiteral("Descending")
                                      : QStringLiteral("Ascending"));
    setLibraryQuery(query);
    refreshCurrentLibrary();
}

void AppController::setLibraryQueryListValue(const QString &key, const QString &value, bool enabled)
{
    if (key.isEmpty() || value.isEmpty())
        return;

    QVariantMap query = libraryQuery();
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

void AppController::setLibraryQueryBoolValue(const QString &key, bool enabled)
{
    if (key.isEmpty())
        return;

    QVariantMap query = libraryQuery();
    if (enabled)
        query.insert(key, true);
    else
        query.remove(key);
    setLibraryQuery(query);
    refreshCurrentLibrary();
}

void AppController::setLibraryQueryNullableBoolValue(const QString &key, const QVariant &value)
{
    if (key.isEmpty())
        return;

    QVariantMap query = libraryQuery();
    if (!value.isValid() || value.isNull())
        query.remove(key);
    else
        query.insert(key, value.toBool());
    setLibraryQuery(query);
    refreshCurrentLibrary();
}

void AppController::clearLibraryFilters()
{
    if (currentLibraryId().isEmpty())
        return;

    QVariantMap query;
    query.insert(QStringLiteral("sortBy"),
                 libraryQuery().value(QStringLiteral("sortBy"), QStringLiteral("SortName")).toString());
    query.insert(QStringLiteral("sortOrder"),
                 libraryQuery().value(QStringLiteral("sortOrder"), QStringLiteral("Ascending")).toString());
    setLibraryQuery(query);
    refreshCurrentLibrary();
}

void AppController::refreshCurrentLibrary()
{
    if (currentViewKind() != QStringLiteral("library") || currentLibraryId().isEmpty())
        return;
    if (!m_api || m_api->session().accessToken.isEmpty())
        return;

    const RequestGeneration::Token loadGeneration = m_libraryLoadGeneration.next();
    const QString libraryId = currentLibraryId();
    const QString collectionType = currentLibraryCollectionType();
    const QVariantMap query = libraryQuery();

    LibraryItem library;
    library.id = libraryId;
    library.collectionType = collectionType;
    const QString cacheKey = libraryCacheKey(library, query);
    m_currentItems->resetPaging(cacheKey);
    m_currentItems->clear();
    m_prefetch->stop();
    if (m_artwork)
        m_artwork->cancelPrefetches();
    setBusy(true, QStringLiteral("Loading %1…").arg(currentContentLabel().toLower()));

    Async::runLatest(this,
        m_api->fetchLibraryPage(libraryId, collectionType, 0, kLibraryPageSize, query),
        m_libraryLoadGeneration, loadGeneration,
        [this, cacheKey](const PagedMovieItems &page) {
            showCurrentItemsPage(page, cacheKey, false);
        },
        [this](const std::exception_ptr &error) {
            setBusy(false);
            m_currentItems->setLoadingMore(false);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::loadDetailRows(const QString &itemId, const QString &itemType,
                                   const QString &seriesId, const QString &seasonId)
{
    m_content->loadDetailRows(itemId, itemType, seriesId, seasonId);
}

void AppController::loadItemDetail(const QString &itemId)
{
    m_content->loadItemDetail(itemId);
}

void AppController::openDetailSeason(int index)
{
    const auto item = m_content->detailSeasonAt(index);
    if (item.id.isEmpty())
        return;
    openSeason(item);
}

void AppController::playDetailSeasonItem(int index, bool fromStart)
{
    playOrOpen(m_content->detailSeasonAt(index), fromStart);
}

void AppController::playDetailSimilarItem(int index, bool fromStart)
{
    playOrOpen(m_content->detailSimilarItemAt(index), fromStart);
}

void AppController::loadPersonItems(const QString &personId)
{
    m_content->loadPersonItems(personId);
}

void AppController::playPersonItem(int index, bool fromStart)
{
    playOrOpen(m_content->personItemAt(index), fromStart);
}

void AppController::setFavorite(const QString &itemId, bool favorite)
{
    if (itemId.isEmpty() || !m_api || m_api->session().accessToken.isEmpty())
        return;

    m_itemState->applyFavorite(itemId, favorite);
    Async::runScoped(this,
        m_api->setItemFavorite(itemId, favorite),
        []() {},
        [this, itemId, favorite](const std::exception_ptr &error) {
            m_itemState->applyFavorite(itemId, !favorite);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::setPlayed(const QString &itemId, bool played)
{
    if (itemId.isEmpty() || !m_api || m_api->session().accessToken.isEmpty())
        return;

    m_itemState->applyPlayed(itemId, played);
    Async::runScoped(this,
        m_api->setItemPlayed(itemId, played),
        []() {},
        [this, itemId, played](const std::exception_ptr &error) {
            m_itemState->applyPlayed(itemId, !played);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::clearProgress(const QString &itemId)
{
    if (itemId.isEmpty() || !m_api || m_api->session().accessToken.isEmpty())
        return;

    m_itemState->applyResumeTicks(itemId, 0);
    m_itemState->applyPlayed(itemId, false);
    Async::runScoped(this,
        m_api->setItemPlaybackPosition(itemId, 0),
        []() {},
        [this](const std::exception_ptr &error) {
            setErrorText(exceptionMessage(error));
        });
}

void AppController::playMediaItem(const MovieItem &item, bool fromStart)
{
    Diagnostics::Task task(QStringLiteral("playback_negotiate"), {{QStringLiteral("itemId"), item.id}, {QStringLiteral("title"), item.title}, {QStringLiteral("type"), item.itemType}});
    setBusy(true, QStringLiteral("Negotiating playback…"));
    MovieItem playItem = item;
    if (fromStart || !isMeaningfulResumePosition(playItem.resumeTicks, playItem.runtimeTicks))
        playItem.resumeTicks = 0;
    m_activePlaybackItem = playItem;
    const QString itemId = playItem.id;
    Async::runScoped(this,
        m_api->negotiatePlayback(playItem),
        [this, itemId](const PlaybackSession &session) {
            // Enrich the negotiated session with media segments (skip-intro)
            // and trickplay (scrubber thumbnails). Both are advisory — if the
            // server doesn't expose them, playback still starts normally.
            JellyfinApiFacade *api = m_api;
            PlayerController *player = m_player;
            const QString mediaSourceId = session.mediaSourceId;
            auto sharedSession = std::make_shared<PlaybackSession>(session);
            auto pending = std::make_shared<int>(2);
            auto kickoff = [player, sharedSession, pending]() {
                if (--(*pending) == 0)
                    player->play(*sharedSession);
            };
            Async::runScoped(this,
                api->fetchMediaSegments(itemId),
                [sharedSession, kickoff](const std::vector<MediaSegment> &segments) {
                    sharedSession->segments = segments;
                    kickoff();
                },
                [kickoff](const std::exception_ptr &) { kickoff(); });
            Async::runScoped(this,
                api->fetchTrickplay(itemId, mediaSourceId),
                [sharedSession, kickoff](const TrickplayInfo &info) {
                    sharedSession->trickplay = info;
                    kickoff();
                },
                [kickoff](const std::exception_ptr &) { kickoff(); });
            setBusy(false);
        },
        [this](const std::exception_ptr &error) {
            setBusy(false);
            setErrorText(exceptionMessage(error));
        });
}


void AppController::onMemoryPressure(const QString &level)
{
    const QString normalized = level.trimmed().toLower();
    if (normalized != QStringLiteral("low") &&
        normalized != QStringLiteral("critical")) {
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

void AppController::setPage(const QString &page)
{
    if (!m_navigation.setPage(page))
        return;

    emit pageChanged();

    if (page == QStringLiteral("login"))
        m_discovery->start();
    else
        m_discovery->stop();
}

void AppController::setBusy(bool busy, const QString &busyText)
{
    if (m_busy == busy && m_busyText == busyText)
        return;
    m_busy = busy;
    m_busyText = busyText;
    emit busyChanged();
}

void AppController::setErrorText(const QString &errorText)
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
    for (const auto &value : servers)
        parsed.push_back(discoveredServerFromJson(value.toObject()));
    m_discoveredServers.setServers(parsed);
}

void AppController::loadLibraries()
{
    m_prefetch->stop();
    setBusy(true, QStringLiteral("Loading libraries…"));
    Async::runScoped(this,
        m_api->fetchLibraries(),
        [this](const std::vector<LibraryItem> &libraries) {
            m_libraries.setLibraries(libraries);
            setBusy(false);
            setPage(QStringLiteral("libraries"));
            refreshHomeRows();
        },
        [this](const std::exception_ptr &error) {
            setBusy(false);
            if (!m_session->handleUnauthorized(error))
                setErrorText(exceptionMessage(error));
        });
}

void AppController::showCurrentItems(const std::vector<MovieItem> &items, const QString &cacheKey)
{
    m_currentItems->setItems(items, cacheKey);
    setBusy(false);
    setPage(QStringLiteral("movies"));
}

void AppController::showCurrentItemsPage(const PagedMovieItems &page, const QString &cacheKey, bool append)
{
    m_currentItems->setPage(page, cacheKey, append);
    setBusy(false);
    if (!append)
        setPage(QStringLiteral("movies"));
}

void AppController::loadLibraryFilterOptions(RequestGeneration::Token generation,
                                             const LibraryItem &library)
{
    if (!m_api || m_api->session().accessToken.isEmpty() || library.id.isEmpty())
        return;

    Async::runLatest(this,
        m_api->fetchLibraryFilterOptions(library.id, library.collectionType),
        m_libraryLoadGeneration, generation,
        [this, library](const QVariantMap &options) {
            if (library.id != currentLibraryId())
                return;
            m_navigation.setFilterOptions(options);
            emit libraryFilterOptionsChanged();
        },
        [this, library](const std::exception_ptr &error) {
            if (library.id != currentLibraryId())
                return;
            qWarning() << "library filters: failed" << library.name << exceptionMessage(error);
            m_navigation.clearFilterOptions();
            emit libraryFilterOptionsChanged();
        });
}

void AppController::openSeries(const MovieItem &series)
{
    if (series.id.isEmpty())
        return;

    const RequestGeneration::Token loadGeneration = m_libraryLoadGeneration.next();
    m_navigation.enterSeries(series);
    emit currentLibraryNameChanged();
    m_currentItems->resetPaging(QStringLiteral("seasons/%1").arg(series.id));
    m_currentItems->clear();
    setBusy(true, QStringLiteral("Loading seasons…"));

    Async::runLatest(this,
        m_api->fetchSeasons(series.id),
        m_libraryLoadGeneration, loadGeneration,
        [this, series](const std::vector<MovieItem> &seasons) {
            if (seasons.empty()) {
                MovieItem fallback;
                fallback.id = series.id;
                fallback.title = series.title;
                fallback.itemType = QStringLiteral("Series");
                fallback.seriesId = series.id;
                fallback.playable = false;
                openSeason(fallback);
                return;
            }
            showCurrentItems(seasons, QStringLiteral("seasons/%1").arg(series.id));
        },
        [this](const std::exception_ptr &error) {
            setBusy(false);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::openSeason(const MovieItem &season)
{
    const QString seriesId = !season.seriesId.isEmpty() ? season.seriesId : m_navigation.seriesId();
    if (seriesId.isEmpty()) {
        qWarning() << "season open: missing series id" << season.id << season.title << season.itemType;
        return;
    }

    const RequestGeneration::Token loadGeneration = m_libraryLoadGeneration.next();
    qInfo() << "season open: loading episodes"
            << "series=" << seriesId
            << "season=" << season.id
            << "type=" << season.itemType
            << "title=" << season.title;
    m_navigation.enterSeason(seriesId, season);
    emit currentLibraryNameChanged();
    m_currentItems->resetPaging(QStringLiteral("episodes/%1/%2").arg(seriesId, season.id));
    m_currentItems->clear();
    setBusy(true, QStringLiteral("Loading episodes…"));

    Async::runLatest(this,
        m_api->fetchEpisodes(seriesId, season.itemType == QStringLiteral("Season") ? season.id : QString()),
        m_libraryLoadGeneration, loadGeneration,
        [this, seriesId, season](const std::vector<MovieItem> &episodes) {
            qInfo() << "season open: episodes loaded"
                    << "series=" << seriesId
                    << "season=" << season.id
                    << "count=" << episodes.size();
            showCurrentItems(episodes, QStringLiteral("episodes/%1/%2").arg(seriesId, season.id));
        },
        [this](const std::exception_ptr &error) {
            qWarning() << "season open: episodes fetch failed" << exceptionMessage(error);
            setBusy(false);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::openGenre(const QString &genre)
{
    const QString name = genre.trimmed();
    if (name.isEmpty() || !m_api || m_api->session().accessToken.isEmpty())
        return;

    const RequestGeneration::Token loadGeneration = m_libraryLoadGeneration.next();
    m_navigation.enterNamedCollection(QStringLiteral("genre"), name);
    emit libraryQueryChanged();
    emit libraryFilterOptionsChanged();
    emit currentLibraryNameChanged();
    const QString cacheKey = QStringLiteral("genre/%1").arg(name);
    m_currentItems->resetPaging(cacheKey);
    m_currentItems->clear();
    setBusy(true, QStringLiteral("Loading %1…").arg(name));

    Async::runLatest(this,
        m_api->fetchItemsByGenre(name),
        m_libraryLoadGeneration, loadGeneration,
        [this, cacheKey](const std::vector<MovieItem> &items) {
            showCurrentItems(items, cacheKey);
        },
        [this](const std::exception_ptr &error) {
            setBusy(false);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::openStudio(const QString &studio)
{
    const QString name = studio.trimmed();
    if (name.isEmpty() || !m_api || m_api->session().accessToken.isEmpty())
        return;

    const RequestGeneration::Token loadGeneration = m_libraryLoadGeneration.next();
    m_navigation.enterNamedCollection(QStringLiteral("studio"), name);
    emit libraryQueryChanged();
    emit libraryFilterOptionsChanged();
    emit currentLibraryNameChanged();
    const QString cacheKey = QStringLiteral("studio/%1").arg(name);
    m_currentItems->resetPaging(cacheKey);
    m_currentItems->clear();
    setBusy(true, QStringLiteral("Loading %1…").arg(name));

    Async::runLatest(this,
        m_api->fetchItemsByStudio(name),
        m_libraryLoadGeneration, loadGeneration,
        [this, cacheKey](const std::vector<MovieItem> &items) {
            showCurrentItems(items, cacheKey);
        },
        [this](const std::exception_ptr &error) {
            setBusy(false);
            setErrorText(exceptionMessage(error));
        });
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
    const QJsonObject payload =
        m_database->loadHomePayload(key, kHomePayloadSchemaVersion);
    if (m_home->applyCachedPayload(payload))
        qInfo() << "home: warm payload cache applied" << key;
}

void AppController::saveHomePayload(const QJsonObject &payload)
{
    if (!m_database || payload.isEmpty())
        return;
    const QString key = homePayloadCacheKey();
    if (key.isEmpty())
        return;
    m_database->saveHomePayload(key, kHomePayloadSchemaVersion, payload);
}

void AppController::handlePlaybackStopped(const QString &itemId,
                                          qint64 positionTicks,
                                          bool completed)
{
    qInfo() << "app: playbackStopped page=" << page()
            << "itemId=" << itemId
            << "positionTicks=" << positionTicks
            << "completed=" << completed;

    if (completed) {
        m_itemState->applyPlayed(itemId, true);
        if (m_api && !m_api->session().accessToken.isEmpty()) {
            Async::runScoped(this,
                m_api->setItemPlayed(itemId, true),
                []() {},
                [](const std::exception_ptr &error) {
                    qWarning() << "app: completed playback mark-watched failed"
                               << exceptionMessage(error);
                });
        }
        if (m_activePlaybackItem.id == itemId)
            playNextEpisodeAfter(m_activePlaybackItem);
    } else {
        m_itemState->applyResumeTicks(itemId, positionTicks);
        if (m_activePlaybackItem.id == itemId)
            m_home->upsertResumeItem(m_activePlaybackItem, positionTicks);
    }
}

void AppController::playNextEpisodeAfter(const MovieItem &episode)
{
    if (episode.itemType != QStringLiteral("Episode") || episode.seriesId.isEmpty())
        return;
    if (!m_api || m_api->session().accessToken.isEmpty())
        return;

    Async::runScoped(this,
        m_api->fetchEpisodes(episode.seriesId),
        [this, episode](const std::vector<MovieItem> &episodes) {
            auto current = std::find_if(episodes.begin(), episodes.end(),
                                        [&episode](const MovieItem &candidate) {
                                            return candidate.id == episode.id;
                                        });
            if (current == episodes.end())
                return;

            for (++current; current != episodes.end(); ++current) {
                if (current->id.isEmpty() || !current->playable)
                    continue;

                qInfo() << "app: auto-playing next episode"
                        << current->seriesName << current->title;
                playMediaItem(*current, false);
                return;
            }
        },
        [](const std::exception_ptr &error) {
            qWarning() << "app: next episode lookup failed"
                       << exceptionMessage(error);
        });
}

} // namespace JellyfinNative
