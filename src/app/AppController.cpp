#include "AppController.h"

#include "../api/JellyfinApiFacade.h"
#include "../common/AsyncTask.h"
#include "../diagnostics/Diagnostics.h"
#include "../player/PlayerController.h"
#include "LibraryPrefetchController.h"
#include "QuickConnectController.h"
#include "SessionController.h"
#include "SettingsController.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QQmlEngine>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

#include <algorithm>
#include <utility>

namespace JellyfinNative {

namespace {

constexpr int kLibraryPageSize = 100;
constexpr int kLibraryPrefetchDistance = 200;

QString homeItemSample(const std::vector<MovieItem> &items)
{
    QStringList sample;
    for (const auto &item : items) {
        sample.push_back(QStringLiteral("%1:%2:%3").arg(item.itemType, item.title).arg(item.resumeTicks));
        if (sample.size() >= 5)
            break;
    }
    return sample.join(QStringLiteral(" | "));
}

bool isSeriesLibrary(const LibraryItem &library)
{
    return library.collectionType == QStringLiteral("tvshows");
}

QString libraryContentLabel(const LibraryItem &library)
{
    if (library.collectionType == QStringLiteral("tvshows"))
        return QStringLiteral("TV Shows");
    if (library.collectionType == QStringLiteral("movies"))
        return QStringLiteral("Movies");
    if (library.collectionType == QStringLiteral("musicvideos"))
        return QStringLiteral("Music Videos");
    if (library.collectionType == QStringLiteral("homevideos"))
        return QStringLiteral("Home Videos");
    return library.name.isEmpty() ? QStringLiteral("Library") : library.name;
}

bool showsLatestLibraryRow(const LibraryItem &library)
{
    static const QSet<QString> excluded = {
        QStringLiteral("playlists"),
        QStringLiteral("livetv"),
        QStringLiteral("boxsets"),
        QStringLiteral("channels"),
        QStringLiteral("folders"),
    };
    return !library.id.isEmpty() && !excluded.contains(library.collectionType);
}

int latestLibraryLimit(const LibraryItem &library)
{
    if (library.collectionType == QStringLiteral("tvshows"))
        return 12;
    return 16;
}

QString libraryCacheKey(const LibraryItem &library)
{
    if (isSeriesLibrary(library))
        return QStringLiteral("series/%1").arg(library.id);
    if (library.collectionType == QStringLiteral("movies"))
        return library.id;
    return QStringLiteral("library/%1/%2").arg(library.collectionType, library.id);
}

QVariantMap defaultLibraryQuery(const LibraryItem &library)
{
    Q_UNUSED(library);
    return {
        {QStringLiteral("sortBy"), QStringLiteral("SortName")},
        {QStringLiteral("sortOrder"), QStringLiteral("Ascending")},
    };
}

QStringList queryStringList(const QVariantMap &query, const QString &key)
{
    const QVariant value = query.value(key);
    if (value.typeId() == QMetaType::QStringList)
        return value.toStringList();

    QStringList result;
    const QVariantList list = value.toList();
    result.reserve(list.size());
    for (const QVariant &item : list) {
        const QString text = item.toString();
        if (!text.isEmpty())
            result.push_back(text);
    }
    return result;
}

QVariantList queryVariantList(const QVariantMap &query, const QString &key)
{
    QVariantList result;
    for (const QString &value : queryStringList(query, key))
        result.push_back(value);
    return result;
}

bool queryHasValue(const QVariantMap &query, const QString &key)
{
    const QVariant value = query.value(key);
    if (!value.isValid() || value.isNull())
        return false;
    if (value.typeId() == QMetaType::QString || value.typeId() == QMetaType::QByteArray)
        return !value.toString().isEmpty();
    if (value.typeId() == QMetaType::QStringList)
        return !value.toStringList().isEmpty();
    if (value.typeId() == QMetaType::QVariantList)
        return !value.toList().isEmpty();
    return value.toBool();
}

int activeLibraryFilterCount(const QVariantMap &query)
{
    static const QStringList listKeys = {
        QStringLiteral("filters"),
        QStringLiteral("genres"),
        QStringLiteral("officialRatings"),
        QStringLiteral("tags"),
        QStringLiteral("years"),
        QStringLiteral("studioIds"),
        QStringLiteral("seriesStatus"),
        QStringLiteral("videoTypes"),
    };
    static const QStringList valueKeys = {
        QStringLiteral("isHd"),
        QStringLiteral("is4K"),
        QStringLiteral("is3D"),
        QStringLiteral("hasSubtitles"),
        QStringLiteral("hasTrailer"),
        QStringLiteral("hasSpecialFeature"),
        QStringLiteral("hasThemeSong"),
        QStringLiteral("hasThemeVideo"),
        QStringLiteral("specialEpisode"),
        QStringLiteral("isMissing"),
        QStringLiteral("isUnaired"),
        QStringLiteral("alphabet"),
    };

    int count = 0;
    for (const QString &key : listKeys)
        count += queryStringList(query, key).size();
    for (const QString &key : valueKeys) {
        if (queryHasValue(query, key))
            ++count;
    }
    return count;
}

QString libraryQuerySignature(const QVariantMap &query, const LibraryItem &library)
{
    const QVariantMap defaults = defaultLibraryQuery(library);
    if (activeLibraryFilterCount(query) == 0 &&
        query.value(QStringLiteral("sortBy"), defaults.value(QStringLiteral("sortBy"))).toString() ==
            defaults.value(QStringLiteral("sortBy")).toString() &&
        query.value(QStringLiteral("sortOrder"), defaults.value(QStringLiteral("sortOrder"))).toString() ==
            defaults.value(QStringLiteral("sortOrder")).toString()) {
        return {};
    }

    QStringList parts;
    const QStringList keys = query.keys();
    for (const QString &key : keys) {
        const QVariant value = query.value(key);
        if (!queryHasValue(query, key))
            continue;
        if (value.typeId() == QMetaType::QStringList || value.typeId() == QMetaType::QVariantList) {
            QStringList values = queryStringList(query, key);
            values.sort();
            parts.push_back(QStringLiteral("%1=%2").arg(key, values.join(QLatin1Char(','))));
        } else {
            parts.push_back(QStringLiteral("%1=%2").arg(key, value.toString()));
        }
    }
    parts.sort();
    return parts.join(QLatin1Char('&'));
}

QString libraryCacheKey(const LibraryItem &library, const QVariantMap &query)
{
    const QString baseKey = libraryCacheKey(library);
    const QString signature = libraryQuerySignature(query, library);
    return signature.isEmpty() ? baseKey : QStringLiteral("%1?%2").arg(baseKey, signature);
}

}

AppController::AppController(DatabaseManager *database,
                             DiscoveryController *discovery,
                             JellyfinApiFacade *api,
                             PlayerController *player,
                             QObject *parent)
    : QObject(parent)
    , m_database(database)
    , m_discovery(discovery)
    , m_api(api)
    , m_player(player)
{
    m_syncPlay = new SyncPlayController(api, player, this);
    m_quickConnect = new QuickConnectController(api, this);
    m_settings = new SettingsController(database, api, player, this);
    m_session = new SessionController(database, api, this);
    m_prefetch = new LibraryPrefetchController(api, this);
    connect(m_syncPlay, &SyncPlayController::errorText, this, &AppController::setErrorText);
    connect(m_quickConnect, &QuickConnectController::changed,
            this, &AppController::quickConnectChanged);
    connect(m_quickConnect, &QuickConnectController::busyChanged,
            this, &AppController::setBusy);
    connect(m_quickConnect, &QuickConnectController::errorOccurred,
            this, &AppController::setErrorText);
    connect(m_settings, &SettingsController::visibleChanged,
            this, &AppController::settingsVisibleChanged);
    connect(m_settings, &SettingsController::nightModeChanged,
            this, &AppController::nightModeEnabledChanged);
    connect(m_settings, &SettingsController::toneMappingVisualizationChanged,
            this, &AppController::toneMappingVisualizationEnabledChanged);
    connect(m_settings, &SettingsController::audioDelayChanged,
            this, &AppController::audioDelayMsChanged);
    connect(m_settings, &SettingsController::audioOutputModeChanged,
            this, &AppController::audioOutputModeChanged);
    connect(m_settings, &SettingsController::subtitleSettingsChanged,
            this, &AppController::subtitleSettingsChanged);
    connect(m_settings, &SettingsController::buttonRemapChanged,
            this, &AppController::buttonRemapChanged);
    connect(m_settings, &SettingsController::errorOccurred,
            this, &AppController::setErrorText);
    connect(m_session, &SessionController::serverUrlChanged,
            this, &AppController::serverUrlChanged);
    connect(m_session, &SessionController::usernameChanged,
            this, &AppController::usernameChanged);
    connect(m_session, &SessionController::passwordChanged,
            this, &AppController::passwordChanged);
    connect(m_session, &SessionController::busyChanged,
            this, &AppController::setBusy);
    connect(m_session, &SessionController::errorOccurred,
            this, &AppController::setErrorText);
    connect(m_session, &SessionController::authenticatedChanged, this,
            [this](const AuthSession &) {
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

    connect(m_player, &PlayerController::playbackStopped, this, [this](const QString &itemId, qint64 positionTicks) {
        qInfo() << "app: playbackStopped page=" << page() << "itemId=" << itemId << "positionTicks=" << positionTicks;
        applyPlaybackPosition(itemId, positionTicks);
        schedulePostPlaybackRefresh();
    });
}

QString AppController::page() const
{
    return m_navigation.page();
}

QString AppController::serverUrl() const
{
    return m_session->serverUrl();
}

QString AppController::username() const
{
    return m_session->username();
}

QString AppController::password() const
{
    return m_session->password();
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

QString AppController::quickConnectCode() const
{
    return m_quickConnect->code();
}

QString AppController::quickConnectStatus() const
{
    return m_quickConnect->status();
}

bool AppController::quickConnectActive() const
{
    return m_quickConnect->active();
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

bool AppController::settingsVisible() const
{
    return m_settings->visible();
}

bool AppController::nightModeEnabled() const
{
    return m_settings->nightModeEnabled();
}

bool AppController::toneMappingVisualizationEnabled() const
{
    return m_settings->toneMappingVisualizationEnabled();
}

int AppController::audioDelayMs() const
{
    return m_settings->audioDelayMs();
}

QString AppController::audioOutputMode() const
{
    return m_settings->audioOutputMode();
}

QStringList AppController::subtitleLanguageOptions() const
{
    return m_settings->subtitleLanguageOptions();
}

int AppController::subtitleLanguageIndex() const
{
    return m_settings->subtitleLanguageIndex();
}

QString AppController::subtitleMode() const
{
    return m_settings->subtitleMode();
}

QString AppController::subtitleBurnIn() const
{
    return m_settings->subtitleBurnIn();
}

bool AppController::subtitleRenderPgs() const
{
    return m_settings->subtitleRenderPgs();
}

bool AppController::subtitleAlwaysBurnIn() const
{
    return m_settings->subtitleAlwaysBurnIn();
}

QString AppController::subtitleStyling() const
{
    return m_settings->subtitleStyling();
}

QString AppController::subtitleTextSize() const
{
    return m_settings->subtitleTextSize();
}

QString AppController::subtitleTextWeight() const
{
    return m_settings->subtitleTextWeight();
}

QString AppController::subtitleFont() const
{
    return m_settings->subtitleFont();
}

QString AppController::subtitleTextColor() const
{
    return m_settings->subtitleTextColor();
}

QString AppController::subtitleDropShadow() const
{
    return m_settings->subtitleDropShadow();
}

int AppController::subtitleVerticalPosition() const
{
    return m_settings->subtitleVerticalPosition();
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
    return &m_movies;
}

MovieGridModel *AppController::resumeItems()
{
    return &m_resumeItems;
}

MovieGridModel *AppController::nextUpItems()
{
    return &m_nextUpItems;
}

MovieGridModel *AppController::latestItems()
{
    return &m_latestItems;
}

QVariantList AppController::latestLibraryRows() const
{
    QVariantList rows;
    rows.reserve(static_cast<qsizetype>(m_latestLibrarySections.size()));
    for (size_t row = 0; row < m_latestLibrarySections.size(); ++row) {
        const LatestLibrarySection &section = m_latestLibrarySections[row];
        if (!section.model || section.model->rowCount() <= 0)
            continue;
        rows.push_back(QVariantMap{
            {QStringLiteral("rowIndex"), static_cast<int>(row)},
            {QStringLiteral("title"), QStringLiteral("Recently Added in %1").arg(section.library.name)},
            {QStringLiteral("libraryName"), section.library.name},
            {QStringLiteral("libraryId"), section.library.id},
            {QStringLiteral("collectionType"), section.library.collectionType},
            {QStringLiteral("kind"), section.library.collectionType == QStringLiteral("tvshows")
                                         || section.library.collectionType == QStringLiteral("movies")
                                     ? QStringLiteral("poster")
                                     : QStringLiteral("landscape")},
            {QStringLiteral("count"), section.model->rowCount()},
        });
    }
    return rows;
}

MovieGridModel *AppController::searchResults()
{
    return &m_searchResults;
}

MovieGridModel *AppController::searchSuggestions()
{
    return &m_searchSuggestions;
}

bool AppController::searchSuggestionsBusy() const
{
    return m_searchSuggestionsBusy;
}

bool AppController::currentItemsLoadingMore() const
{
    return m_currentItemsLoadingMore;
}

bool AppController::currentItemsHasMore() const
{
    return m_currentItemsHasMore;
}

int AppController::currentItemsTotalCount() const
{
    return m_currentItemsTotalCount;
}

MovieGridModel *AppController::detailSeasons()
{
    return &m_detailSeasons;
}

MovieGridModel *AppController::detailSimilarItems()
{
    return &m_detailSimilarItems;
}

MovieGridModel *AppController::personItems()
{
    return &m_personItems;
}

bool AppController::detailRowsBusy() const
{
    return m_detailRowsBusy;
}

bool AppController::personItemsBusy() const
{
    return m_personItemsBusy;
}

bool AppController::searchBusy() const
{
    return m_searchBusy;
}

QString AppController::searchQuery() const
{
    return m_searchQuery;
}

SyncPlayController *AppController::syncPlay() { return m_syncPlay; }

PlayerController *AppController::player()
{
    return m_player;
}

void AppController::initialize()
{
    Diagnostics::Task task(QStringLiteral("app_initialize"));
    m_settings->loadLocal();
    if (!m_session->initialize()) {
        applyDiscoveredServersCache();
        m_discovery->start();
    }
}

void AppController::setServerUrl(const QString &serverUrl)
{
    m_session->setServerUrl(serverUrl);
}

void AppController::setUsername(const QString &username)
{
    m_session->setUsername(username);
}

void AppController::setPassword(const QString &password)
{
    m_session->setPassword(password);
}

void AppController::chooseDiscoveredServer(int index)
{
    const auto server = m_discoveredServers.serverAt(index);
    if (server.address.isEmpty())
        return;
    setServerUrl(server.address);
}

void AppController::login()
{
    setErrorText({});
    m_session->login();
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
    m_api->cancelPrefetches();
    m_libraries.clear();
    m_movies.clear();
    m_resumeItems.clear();
    m_nextUpItems.clear();
    m_latestItems.clear();
    clearLatestLibraryRows();
    m_searchGeneration.invalidate();
    m_searchSuggestionsGeneration.invalidate();
    m_searchResults.clear();
    m_searchSuggestions.clear();
    m_searchQuery.clear();
    m_searchBusy = false;
    m_searchSuggestionsBusy = false;
    m_searchSuggestionsLoaded = false;
    m_detailRowsGeneration.invalidate();
    m_detailRowsPending = 0;
    m_detailRowsBusy = false;
    m_detailSeasons.clear();
    m_detailSimilarItems.clear();
    m_personItemsGeneration.invalidate();
    m_personItems.clear();
    m_personItemsBusy = false;
    m_libraryLoadGeneration.invalidate();
    m_homeLoadGeneration.invalidate();
    m_homeLoadsPending = 0;
    const bool pageWasLogin = page() == QStringLiteral("login");
    m_navigation.reset();
    resetCurrentItemsPaging();
    setBusy(false);
    setErrorText({});
    emit currentLibraryNameChanged();
    emit libraryQueryChanged();
    emit libraryFilterOptionsChanged();
    emit searchChanged();
    emit searchSuggestionsChanged();
    emit detailRowsChanged();
    emit personItemsChanged();
    if (!pageWasLogin)
        emit pageChanged();
    applyDiscoveredServersCache();
    m_discovery->start();
}

void AppController::startQuickConnect()
{
    setErrorText({});
    m_quickConnect->start(serverUrl());
}

void AppController::cancelQuickConnect()
{
    m_quickConnect->cancel();
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
    resetCurrentItemsPaging(cacheKey);
    recordLibraryUse(library);
    emit currentLibraryNameChanged();
    const int cachedCount = applyPrefetchedLibraryPage(cacheKey);
    const bool hasWarmCache = cachedCount > 0;
    m_currentItemsNextStartIndex = cachedCount;
    m_currentItemsTotalCount = cachedCount;
    m_currentItemsHasMore = cachedCount >= kLibraryPageSize;
    m_currentItemsLoadingMore = true;
    emit currentItemsPagingChanged();
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
            setCurrentItemsPage(page, cacheKey, false);
        },
        [this, hasWarmCache](const std::exception_ptr &error) {
            setBusy(false);
            setCurrentItemsLoadingMore(false);
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
    playOrOpen(m_movies.movieAt(index), fromStart);
}

void AppController::playResumeItem(int index, bool fromStart)
{
    const auto item = m_resumeItems.movieAt(index);
    qInfo() << "app: play resume item index=" << index
            << "type=" << item.itemType << "title=" << item.title
            << "resumeTicks=" << item.resumeTicks << "fromStart=" << fromStart;
    if (item.id.isEmpty())
        return;
    playMediaItem(item, fromStart);
}

void AppController::playNextUpItem(int index, bool fromStart)
{
    const auto item = m_nextUpItems.movieAt(index);
    qInfo() << "app: play next-up item index=" << index
            << "type=" << item.itemType << "title=" << item.title
            << "resumeTicks=" << item.resumeTicks << "fromStart=" << fromStart;
    if (item.id.isEmpty())
        return;
    playMediaItem(item, fromStart);
}

void AppController::playLatestItem(int index, bool fromStart)
{
    playOrOpen(m_latestItems.movieAt(index), fromStart);
}

QObject *AppController::latestLibraryItems(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= static_cast<int>(m_latestLibrarySections.size()))
        return nullptr;
    return m_latestLibrarySections[static_cast<size_t>(rowIndex)].model.get();
}

void AppController::playLatestLibraryItem(int rowIndex, int itemIndex, bool fromStart)
{
    if (rowIndex < 0 || rowIndex >= static_cast<int>(m_latestLibrarySections.size()))
        return;

    MovieGridModel *model = m_latestLibrarySections[static_cast<size_t>(rowIndex)].model.get();
    if (!model)
        return;

    playOrOpen(model->movieAt(itemIndex), fromStart);
}

void AppController::search(const QString &query)
{
    const QString trimmed = query.trimmed();
    const RequestGeneration::Token generation = m_searchGeneration.next();
    m_searchQuery = trimmed;

    if (trimmed.size() < 2 || !m_api || m_api->session().accessToken.isEmpty()) {
        m_searchBusy = false;
        m_searchResults.clear();
        emit searchChanged();
        return;
    }

    m_searchBusy = true;
    emit searchChanged();

    Async::runLatest(this,
        m_api->searchItems(trimmed),
        m_searchGeneration, generation,
        [this](const std::vector<MovieItem> &items) {
            m_searchResults.setMovies(items);
            m_prefetch->prefetchPosters(items);
            m_searchBusy = false;
            emit searchChanged();
        },
        [this](const std::exception_ptr &error) {
            m_searchResults.clear();
            m_searchBusy = false;
            emit searchChanged();
            setErrorText(exceptionMessage(error));
        });
}

void AppController::clearSearch()
{
    m_searchGeneration.invalidate();
    m_searchQuery.clear();
    m_searchBusy = false;
    m_searchResults.clear();
    emit searchChanged();
}

void AppController::loadSearchSuggestions()
{
    if (!m_api || m_api->session().accessToken.isEmpty())
        return;
    // Suggestions are stable for a session; load them once and reuse.
    if (m_searchSuggestionsLoaded || m_searchSuggestionsBusy)
        return;

    const RequestGeneration::Token generation = m_searchSuggestionsGeneration.next();
    m_searchSuggestionsBusy = true;
    emit searchSuggestionsChanged();

    Async::runLatest(this,
        m_api->fetchSearchSuggestions(),
        m_searchSuggestionsGeneration, generation,
        [this](const std::vector<MovieItem> &items) {
            m_searchSuggestions.setMovies(items);
            m_prefetch->prefetchPosters(items);
            m_searchSuggestionsBusy = false;
            m_searchSuggestionsLoaded = true;
            emit searchSuggestionsChanged();
        },
        [this](const std::exception_ptr &error) {
            m_searchSuggestions.clear();
            m_searchSuggestionsBusy = false;
            emit searchSuggestionsChanged();
            qWarning() << "search: suggestions fetch failed" << exceptionMessage(error);
        });
}

void AppController::playSuggestionItem(int index, bool fromStart)
{
    playOrOpen(m_searchSuggestions.movieAt(index), fromStart);
}

void AppController::playSearchResult(int index, bool fromStart)
{
    playOrOpen(m_searchResults.movieAt(index), fromStart);
}

void AppController::maybeLoadMoreCurrentItems(int visibleIndex)
{
    if (visibleIndex < 0)
        return;
    if (visibleIndex + kLibraryPrefetchDistance < m_movies.rowCount())
        return;
    loadMoreCurrentItems();
}

void AppController::loadMoreCurrentItems()
{
    if (m_currentItemsLoadingMore || !m_currentItemsHasMore)
        return;
    if (!m_api || m_api->session().accessToken.isEmpty())
        return;
    if (currentViewKind() != QStringLiteral("library"))
        return;

    const int startIndex = std::max(m_currentItemsNextStartIndex, m_movies.rowCount());
    const RequestGeneration::Token loadGeneration = m_libraryLoadGeneration.current();
    const QString libraryId = currentLibraryId();
    const QString collectionType = currentLibraryCollectionType();
    const QString cacheKey = m_currentItemsCacheKey;
    const QVariantMap query = libraryQuery();
    setCurrentItemsLoadingMore(true);

    const auto onDone = [this, cacheKey](const PagedMovieItems &page) {
        setCurrentItemsPage(page, cacheKey, true);
    };
    const auto onError = [this](const std::exception_ptr &error) {
        setCurrentItemsLoadingMore(false);
        setErrorText(exceptionMessage(error));
    };

    Async::runLatest(this,
                     m_api->fetchLibraryPage(libraryId, collectionType, startIndex,
                                             kLibraryPageSize, query),
                     m_libraryLoadGeneration, loadGeneration, onDone, onError);
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
    QStringList values = queryStringList(query, key);
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
    resetCurrentItemsPaging(cacheKey);
    m_movies.clear();
    setBusy(true, QStringLiteral("Loading %1…").arg(currentContentLabel().toLower()));

    Async::runLatest(this,
        m_api->fetchLibraryPage(libraryId, collectionType, 0, kLibraryPageSize, query),
        m_libraryLoadGeneration, loadGeneration,
        [this, cacheKey](const PagedMovieItems &page) {
            setCurrentItemsPage(page, cacheKey, false);
        },
        [this](const std::exception_ptr &error) {
            setBusy(false);
            setCurrentItemsLoadingMore(false);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::loadDetailRows(const QString &itemId, const QString &itemType)
{
    const RequestGeneration::Token generation = m_detailRowsGeneration.next();
    m_detailRowsPending = 0;
    m_detailRowsBusy = false;
    m_detailSeasons.clear();
    m_detailSimilarItems.clear();

    if (itemId.isEmpty() || !m_api || m_api->session().accessToken.isEmpty()) {
        emit detailRowsChanged();
        return;
    }

    m_detailRowsBusy = true;
    emit detailRowsChanged();

    const bool loadSeasons = itemType == QStringLiteral("Series");
    m_detailRowsPending = loadSeasons ? 2 : 1;
    qInfo() << "detail rows: loading" << itemType << itemId << "seasons=" << loadSeasons;

    if (loadSeasons) {
        Async::runLatest(this,
            m_api->fetchSeasons(itemId),
            m_detailRowsGeneration, generation,
            [this, generation, itemId](const std::vector<MovieItem> &seasons) {
                qInfo() << "detail rows: seasons loaded" << itemId << seasons.size();
                m_detailSeasons.setMovies(seasons);
                m_prefetch->prefetchPosters(seasons);
                emit detailRowsChanged();
                finishDetailRowLoad(generation);
            },
            [this, generation, itemId](const std::exception_ptr &error) {
                qWarning() << "detail rows: seasons fetch failed" << itemId << exceptionMessage(error);
                finishDetailRowLoad(generation);
            });
    }

    Async::runLatest(this,
        m_api->fetchSimilarItems(itemId),
        m_detailRowsGeneration, generation,
        [this, generation, itemId](const std::vector<MovieItem> &items) {
            qInfo() << "detail rows: similar loaded" << itemId << items.size();
            m_detailSimilarItems.setMovies(items);
            m_prefetch->prefetchPosters(items);
            emit detailRowsChanged();
            finishDetailRowLoad(generation);
        },
        [this, generation, itemId](const std::exception_ptr &error) {
            qWarning() << "detail rows: similar fetch failed" << itemId << exceptionMessage(error);
            finishDetailRowLoad(generation);
        });
}

void AppController::openDetailSeason(int index)
{
    const auto item = m_detailSeasons.movieAt(index);
    if (item.id.isEmpty())
        return;
    openSeason(item);
}

void AppController::playDetailSimilarItem(int index, bool fromStart)
{
    playOrOpen(m_detailSimilarItems.movieAt(index), fromStart);
}

void AppController::loadPersonItems(const QString &personId)
{
    const RequestGeneration::Token generation = m_personItemsGeneration.next();
    m_personItems.clear();
    if (personId.isEmpty() || !m_api || m_api->session().accessToken.isEmpty()) {
        m_personItemsBusy = false;
        emit personItemsChanged();
        return;
    }

    m_personItemsBusy = true;
    emit personItemsChanged();

    Async::runLatest(this,
        m_api->fetchItemsByPerson(personId),
        m_personItemsGeneration, generation,
        [this](const std::vector<MovieItem> &items) {
            m_personItems.setMovies(items);
            m_prefetch->prefetchPosters(items);
            m_personItemsBusy = false;
            emit personItemsChanged();
        },
        [this](const std::exception_ptr &error) {
            m_personItems.clear();
            m_personItemsBusy = false;
            emit personItemsChanged();
            setErrorText(exceptionMessage(error));
        });
}

void AppController::playPersonItem(int index, bool fromStart)
{
    playOrOpen(m_personItems.movieAt(index), fromStart);
}

void AppController::setFavorite(const QString &itemId, bool favorite)
{
    if (itemId.isEmpty() || !m_api || m_api->session().accessToken.isEmpty())
        return;

    applyFavoriteState(itemId, favorite);
    Async::runScoped(this,
        m_api->setItemFavorite(itemId, favorite),
        []() {},
        [this, itemId, favorite](const std::exception_ptr &error) {
            applyFavoriteState(itemId, !favorite);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::setPlayed(const QString &itemId, bool played)
{
    if (itemId.isEmpty() || !m_api || m_api->session().accessToken.isEmpty())
        return;

    applyPlayedState(itemId, played);
    Async::runScoped(this,
        m_api->setItemPlayed(itemId, played),
        []() {},
        [this, itemId, played](const std::exception_ptr &error) {
            applyPlayedState(itemId, !played);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::playMediaItem(const MovieItem &item, bool fromStart)
{
    Diagnostics::Task task(QStringLiteral("playback_negotiate"), {{QStringLiteral("itemId"), item.id}, {QStringLiteral("title"), item.title}, {QStringLiteral("type"), item.itemType}});
    setBusy(true, QStringLiteral("Negotiating direct play…"));
    MovieItem playItem = item;
    if (fromStart)
        playItem.resumeTicks = 0;
    const QString itemId = playItem.id;
    Async::runScoped(this,
        m_api->negotiateDirectPlay(playItem),
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

void AppController::back()
{
    qInfo() << "app: back pressed, page=" << page() << "settingsVisible=" << settingsVisible();
    if (settingsVisible()) {
        closeSettings();
        return;
    }

    if (m_player->visible()) {
        m_player->stop();
        return;
    }

    if (page() == QStringLiteral("movies")) {
        if (currentViewKind() == QStringLiteral("episodes")) {
            MovieItem series;
            series.id = m_navigation.seriesId();
            series.title = m_navigation.seriesName();
            series.itemType = QStringLiteral("Series");
            series.playable = false;
            openSeries(series);
            return;
        }
        qInfo() << "app: back from movies to libraries";
        setPage(QStringLiteral("libraries"));
        return;
    }

    if (page() == QStringLiteral("libraries")) {
        qInfo() << "app: back ignored on home";
        return;
    }

    qInfo() << "app: quitting";
    QCoreApplication::quit();
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
    m_api->cancelPrefetches();
    m_discovery->stop();
    m_player->teardownMpv();
}

void AppController::clearError()
{
    setErrorText({});
}

void AppController::openSettings()
{
    m_settings->open();
}

void AppController::closeSettings()
{
    m_settings->close();
}

void AppController::toggleNightMode()
{
    m_settings->toggleNightMode();
}

void AppController::setNightModeEnabled(bool enabled)
{
    m_settings->setNightModeEnabled(enabled);
}

void AppController::setToneMappingVisualizationEnabled(bool enabled)
{
    m_settings->setToneMappingVisualizationEnabled(enabled);
}

void AppController::setAudioDelayMs(int delayMs)
{
    m_settings->setAudioDelayMs(delayMs);
}

void AppController::setAudioOutputMode(const QString &mode)
{
    m_settings->setAudioOutputMode(mode);
}

void AppController::setSubtitleLanguageIndex(int index)
{
    m_settings->setSubtitleLanguageIndex(index);
}

void AppController::setSubtitleMode(const QString &mode)
{
    m_settings->setSubtitleMode(mode);
}

void AppController::setSubtitleBurnIn(const QString &mode)
{
    m_settings->setSubtitleBurnIn(mode);
}

void AppController::setSubtitleRenderPgs(bool enabled)
{
    m_settings->setSubtitleRenderPgs(enabled);
}

void AppController::setSubtitleAlwaysBurnIn(bool enabled)
{
    m_settings->setSubtitleAlwaysBurnIn(enabled);
}

void AppController::setSubtitleStyling(const QString &styling)
{
    m_settings->setSubtitleStyling(styling);
}

void AppController::setSubtitleTextSize(const QString &size)
{
    m_settings->setSubtitleTextSize(size);
}

void AppController::setSubtitleTextWeight(const QString &weight)
{
    m_settings->setSubtitleTextWeight(weight);
}

void AppController::setSubtitleFont(const QString &font)
{
    m_settings->setSubtitleFont(font);
}

void AppController::setSubtitleTextColor(const QString &color)
{
    m_settings->setSubtitleTextColor(color);
}

void AppController::setSubtitleDropShadow(const QString &shadow)
{
    m_settings->setSubtitleDropShadow(shadow);
}

void AppController::setSubtitleVerticalPosition(int position)
{
    m_settings->setSubtitleVerticalPosition(position);
}

QString AppController::redButtonAction() const { return m_settings->redButtonAction(); }
QString AppController::greenButtonAction() const { return m_settings->greenButtonAction(); }
QString AppController::yellowButtonAction() const { return m_settings->yellowButtonAction(); }
QString AppController::blueButtonAction() const { return m_settings->blueButtonAction(); }

QStringList AppController::availableButtonActions() const
{
    return m_settings->availableButtonActions();
}

QString AppController::buttonActionLabel(const QString &action) const
{
    return m_settings->buttonActionLabel(action);
}

void AppController::setRedButtonAction(const QString &action)
{
    m_settings->setRedButtonAction(action);
}

void AppController::setGreenButtonAction(const QString &action)
{
    m_settings->setGreenButtonAction(action);
}

void AppController::setYellowButtonAction(const QString &action)
{
    m_settings->setYellowButtonAction(action);
}

void AppController::setBlueButtonAction(const QString &action)
{
    m_settings->setBlueButtonAction(action);
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

int AppController::applyPrefetchedLibraryPage(const QString &cacheKey)
{
    const auto prefetchedPage = m_prefetch->cachedPage(cacheKey);
    if (prefetchedPage) {
        m_movies.setMovies(prefetchedPage->items);
        m_prefetch->prefetchPosters(prefetchedPage->items);
        return m_movies.rowCount();
    }
    m_movies.clear();
    return 0;
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

void AppController::setCurrentItems(const std::vector<MovieItem> &items, const QString &cacheKey)
{
    resetCurrentItemsPaging(cacheKey);
    m_movies.setMovies(items);
    m_prefetch->prefetchPosters(items);
    setBusy(false);
    setPage(QStringLiteral("movies"));
}

void AppController::setCurrentItemsPage(const PagedMovieItems &page, const QString &cacheKey, bool append)
{
    if (!append) {
        m_movies.setMovies(page.items);
    } else {
        m_movies.appendMovies(page.items);
    }

    const int loadedCount = m_movies.rowCount();
    const int pageEnd = page.startIndex + static_cast<int>(page.items.size());
    const bool hasServerTotal = page.totalRecordCount > 0;
    m_currentItemsCacheKey = cacheKey;
    m_currentItemsNextStartIndex = std::max(loadedCount, pageEnd);
    m_currentItemsTotalCount = hasServerTotal ? std::max(page.totalRecordCount, m_currentItemsNextStartIndex)
                                              : m_currentItemsNextStartIndex;
    m_currentItemsHasMore = hasServerTotal
                                ? m_currentItemsNextStartIndex < m_currentItemsTotalCount
                                : page.items.size() >= static_cast<size_t>(std::max(1, page.limit));
    if (page.items.empty())
        m_currentItemsHasMore = false;
    m_currentItemsLoadingMore = false;

    m_prefetch->prefetchPosters(page.items);
    setBusy(false);
    setPage(QStringLiteral("movies"));
    emit currentItemsPagingChanged();
}

void AppController::resetCurrentItemsPaging(const QString &cacheKey)
{
    m_currentItemsCacheKey = cacheKey;
    m_currentItemsLoadingMore = false;
    m_currentItemsHasMore = false;
    m_currentItemsTotalCount = 0;
    m_currentItemsNextStartIndex = 0;
    emit currentItemsPagingChanged();
}

void AppController::setCurrentItemsLoadingMore(bool loading)
{
    if (m_currentItemsLoadingMore == loading)
        return;
    m_currentItemsLoadingMore = loading;
    emit currentItemsPagingChanged();
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

void AppController::recordLibraryUse(const LibraryItem &library)
{
    if (library.id.isEmpty())
        return;

    m_recentLibraryIds.removeAll(library.id);
    m_recentLibraryIds.prepend(library.id);
    while (m_recentLibraryIds.size() > 12)
        m_recentLibraryIds.removeLast();
}

void AppController::openSeries(const MovieItem &series)
{
    if (series.id.isEmpty())
        return;

    const RequestGeneration::Token loadGeneration = m_libraryLoadGeneration.next();
    m_navigation.enterSeries(series);
    emit currentLibraryNameChanged();
    resetCurrentItemsPaging(QStringLiteral("seasons/%1").arg(series.id));
    m_movies.clear();
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
            setCurrentItems(seasons, QStringLiteral("seasons/%1").arg(series.id));
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
    resetCurrentItemsPaging(QStringLiteral("episodes/%1/%2").arg(seriesId, season.id));
    m_movies.clear();
    setBusy(true, QStringLiteral("Loading episodes…"));

    Async::runLatest(this,
        m_api->fetchEpisodes(seriesId, season.itemType == QStringLiteral("Season") ? season.id : QString()),
        m_libraryLoadGeneration, loadGeneration,
        [this, seriesId, season](const std::vector<MovieItem> &episodes) {
            qInfo() << "season open: episodes loaded"
                    << "series=" << seriesId
                    << "season=" << season.id
                    << "count=" << episodes.size();
            setCurrentItems(episodes, QStringLiteral("episodes/%1/%2").arg(seriesId, season.id));
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
    resetCurrentItemsPaging(cacheKey);
    m_movies.clear();
    setBusy(true, QStringLiteral("Loading %1…").arg(name));

    Async::runLatest(this,
        m_api->fetchItemsByGenre(name),
        m_libraryLoadGeneration, loadGeneration,
        [this, cacheKey](const std::vector<MovieItem> &items) {
            setCurrentItems(items, cacheKey);
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
    resetCurrentItemsPaging(cacheKey);
    m_movies.clear();
    setBusy(true, QStringLiteral("Loading %1…").arg(name));

    Async::runLatest(this,
        m_api->fetchItemsByStudio(name),
        m_libraryLoadGeneration, loadGeneration,
        [this, cacheKey](const std::vector<MovieItem> &items) {
            setCurrentItems(items, cacheKey);
        },
        [this](const std::exception_ptr &error) {
            setBusy(false);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::refreshHomeRows()
{
    if (!m_api || m_api->session().accessToken.isEmpty())
        return;

    const RequestGeneration::Token generation = m_homeLoadGeneration.next();
    clearLatestLibraryRows();
    m_latestItems.clear();
    m_prefetch->stop();

    std::vector<LibraryItem> latestLibraries;
    const int libraryCount = m_libraries.rowCount();
    latestLibraries.reserve(static_cast<size_t>(libraryCount));
    for (int i = 0; i < libraryCount; ++i) {
        const LibraryItem library = m_libraries.libraryAt(i);
        if (showsLatestLibraryRow(library))
            latestLibraries.push_back(library);
    }

    m_homeLoadsPending = 2 + static_cast<int>(latestLibraries.size());

    Async::runLatest(this,
        m_api->fetchResumeItems(),
        m_homeLoadGeneration, generation,
        [this, generation](const std::vector<MovieItem> &items) {
            qInfo() << "home: resume items" << items.size() << homeItemSample(items);
            m_resumeItems.setMovies(items);
            m_prefetch->prefetchPosters(items);
            handleHomeRowLoaded(generation);
        },
        [this, generation](const std::exception_ptr &error) {
            qWarning() << "home: resume fetch failed" << exceptionMessage(error);
            handleHomeRowLoaded(generation);
        });

    Async::runLatest(this,
        m_api->fetchNextUpEpisodes(),
        m_homeLoadGeneration, generation,
        [this, generation](const std::vector<MovieItem> &items) {
            qInfo() << "home: next-up items" << items.size() << homeItemSample(items);
            m_nextUpItems.setMovies(items);
            m_prefetch->prefetchPosters(items);
            handleHomeRowLoaded(generation);
        },
        [this, generation](const std::exception_ptr &error) {
            qWarning() << "home: next-up fetch failed" << exceptionMessage(error);
            handleHomeRowLoaded(generation);
        });

    for (int order = 0; order < static_cast<int>(latestLibraries.size()); ++order) {
        const LibraryItem library = latestLibraries[static_cast<size_t>(order)];
        Async::runLatest(this,
            m_api->fetchLatestItems(library.id, latestLibraryLimit(library)),
            m_homeLoadGeneration, generation,
            [this, generation, order, library](const std::vector<MovieItem> &items) {
                qInfo() << "home: latest items" << library.name << items.size() << homeItemSample(items);
                addLatestLibraryRow(generation, order, library, items);
                handleHomeRowLoaded(generation);
            },
            [this, generation, library](const std::exception_ptr &error) {
                qWarning() << "home: latest fetch failed" << library.name << exceptionMessage(error);
                handleHomeRowLoaded(generation);
            });
    }
}

void AppController::schedulePostPlaybackRefresh()
{
    const QString viewKind = currentViewKind();
    const QString libraryId = currentLibraryId();
    const QString seriesId = m_navigation.seriesId();
    const QString seasonId = m_navigation.seasonId();
    QTimer::singleShot(1500, this, [this, viewKind, libraryId, seriesId, seasonId]() {
        if (!m_api || m_api->session().accessToken.isEmpty())
            return;
        qInfo() << "app: post-playback data refresh";
        refreshHomeRows();
        refreshCurrentItems(viewKind, libraryId, seriesId, seasonId);
    });
}

void AppController::refreshCurrentItems(const QString &viewKind,
                                        const QString &libraryId,
                                        const QString &seriesId,
                                        const QString &seasonId)
{
    if (viewKind != currentViewKind() || libraryId != currentLibraryId() ||
        seriesId != m_navigation.seriesId() || seasonId != m_navigation.seasonId())
        return;

    if (viewKind == QStringLiteral("library") && !libraryId.isEmpty()) {
        const RequestGeneration::Token loadGeneration = m_libraryLoadGeneration.next();
        const QString collectionType = currentLibraryCollectionType();
        const QString cacheKey = m_currentItemsCacheKey;
        const QVariantMap query = libraryQuery();
        Async::runLatest(this,
            m_api->fetchLibraryPage(libraryId, collectionType, 0, kLibraryPageSize, query),
            m_libraryLoadGeneration, loadGeneration,
            [this, cacheKey](const PagedMovieItems &page) {
                setCurrentItemsPage(page, cacheKey, false);
            },
            [this](const std::exception_ptr &error) {
                qWarning() << "app: post-playback library refresh failed" << exceptionMessage(error);
            });
        return;
    }

    if (viewKind == QStringLiteral("episodes") && !seriesId.isEmpty()) {
        const RequestGeneration::Token loadGeneration = m_libraryLoadGeneration.next();
        Async::runLatest(this,
            m_api->fetchEpisodes(seriesId, seasonId),
            m_libraryLoadGeneration, loadGeneration,
            [this](const std::vector<MovieItem> &episodes) {
                m_movies.setMovies(episodes);
                m_prefetch->prefetchPosters(episodes);
            },
            [this](const std::exception_ptr &error) {
                qWarning() << "app: post-playback episode refresh failed" << exceptionMessage(error);
            });
    }
}

void AppController::applyPlaybackPosition(const QString &itemId, qint64 positionTicks)
{
    if (itemId.isEmpty() || positionTicks < 0)
        return;

    m_movies.updateResumeTicks(itemId, positionTicks);
    m_resumeItems.updateResumeTicks(itemId, positionTicks);
    m_nextUpItems.updateResumeTicks(itemId, positionTicks);
    m_latestItems.updateResumeTicks(itemId, positionTicks);
    m_searchResults.updateResumeTicks(itemId, positionTicks);
    m_searchSuggestions.updateResumeTicks(itemId, positionTicks);
    m_detailSimilarItems.updateResumeTicks(itemId, positionTicks);
    m_personItems.updateResumeTicks(itemId, positionTicks);
    for (LatestLibrarySection &section : m_latestLibrarySections) {
        if (section.model)
            section.model->updateResumeTicks(itemId, positionTicks);
    }
}

void AppController::applyFavoriteState(const QString &itemId, bool favorite)
{
    if (itemId.isEmpty())
        return;

    m_movies.updateFavorite(itemId, favorite);
    m_resumeItems.updateFavorite(itemId, favorite);
    m_nextUpItems.updateFavorite(itemId, favorite);
    m_latestItems.updateFavorite(itemId, favorite);
    m_searchResults.updateFavorite(itemId, favorite);
    m_searchSuggestions.updateFavorite(itemId, favorite);
    m_detailSeasons.updateFavorite(itemId, favorite);
    m_detailSimilarItems.updateFavorite(itemId, favorite);
    m_personItems.updateFavorite(itemId, favorite);
    for (LatestLibrarySection &section : m_latestLibrarySections) {
        if (section.model)
            section.model->updateFavorite(itemId, favorite);
    }
    emit itemFavoriteChanged(itemId, favorite);
}

void AppController::applyPlayedState(const QString &itemId, bool played)
{
    if (itemId.isEmpty())
        return;

    m_movies.updatePlayed(itemId, played);
    m_resumeItems.updatePlayed(itemId, played);
    m_nextUpItems.updatePlayed(itemId, played);
    m_latestItems.updatePlayed(itemId, played);
    m_searchResults.updatePlayed(itemId, played);
    m_searchSuggestions.updatePlayed(itemId, played);
    m_detailSeasons.updatePlayed(itemId, played);
    m_detailSimilarItems.updatePlayed(itemId, played);
    m_personItems.updatePlayed(itemId, played);
    for (LatestLibrarySection &section : m_latestLibrarySections) {
        if (section.model)
            section.model->updatePlayed(itemId, played);
    }
    emit itemPlayedChanged(itemId, played);
}

void AppController::clearLatestLibraryRows()
{
    if (m_latestLibrarySections.empty())
        return;
    m_latestLibrarySections.clear();
    emit latestLibraryRowsChanged();
}

void AppController::addLatestLibraryRow(RequestGeneration::Token generation,
                                        int order,
                                        const LibraryItem &library,
                                        const std::vector<MovieItem> &items)
{
    if (!m_homeLoadGeneration.isCurrent(generation) || items.empty())
        return;

    auto model = std::make_unique<MovieGridModel>();
    QQmlEngine::setObjectOwnership(model.get(), QQmlEngine::CppOwnership);
    model->setMovies(items);

    LatestLibrarySection section;
    section.order = order;
    section.library = library;
    section.model = std::move(model);
    m_latestLibrarySections.push_back(std::move(section));

    std::sort(m_latestLibrarySections.begin(), m_latestLibrarySections.end(),
              [](const LatestLibrarySection &left, const LatestLibrarySection &right) {
                  return left.order < right.order;
              });

    m_prefetch->prefetchPosters(items);
    emit latestLibraryRowsChanged();
}

void AppController::handleHomeRowLoaded(RequestGeneration::Token generation)
{
    if (!m_homeLoadGeneration.isCurrent(generation) || m_homeLoadsPending <= 0)
        return;

    --m_homeLoadsPending;
    if (m_homeLoadsPending == 0)
        m_prefetch->schedule(m_libraries.libraries(), m_recentLibraryIds);
}

void AppController::finishDetailRowLoad(RequestGeneration::Token generation)
{
    if (!m_detailRowsGeneration.isCurrent(generation) || m_detailRowsPending <= 0)
        return;

    --m_detailRowsPending;
    if (m_detailRowsPending == 0) {
        m_detailRowsBusy = false;
        emit detailRowsChanged();
    }
}

} // namespace JellyfinNative
