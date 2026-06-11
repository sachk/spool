#include "AppController.h"

#include "../api/JellyfinApiFacade.h"
#include "../common/AsyncTask.h"
#include "../diagnostics/Diagnostics.h"
#include "../player/PlayerController.h"
#include "QuickConnectController.h"
#include "SettingsController.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QQmlEngine>
#include <QStringList>
#include <QVariantMap>

#include <algorithm>
#include <utility>

namespace JellyfinNative {

namespace {

constexpr int kLibraryPageSize = 100;
constexpr int kLibraryPrefetchDistance = 200;
constexpr int kBackgroundLibraryPrefetchLimit = 3;

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
    connect(m_quickConnect, &QuickConnectController::authenticated, this,
            [this](const AuthSession &session) {
                m_database->saveLoginHints(m_serverUrl, m_username);
                m_database->saveAuthSession(session);
                m_settings->loadRemote();
                m_syncPlay->connectSocket();
                loadLibraries();
                Async::runScoped(this, m_api->postCapabilities(), []() {},
                                 [](const std::exception_ptr &) {});
            });
    connect(m_discovery, &DiscoveryController::serverDiscovered, this, [this](const DiscoveredServer &server) {
        m_discoveredServers.upsertServer(server);
        QJsonArray cache;
        for (const auto &entry : m_discoveredServers.servers())
            cache.push_back(toJson(entry));
        m_database->saveDiscoveredServers(cache);
    });

    m_libraryPrefetchTimer.setSingleShot(true);
    connect(&m_libraryPrefetchTimer, &QTimer::timeout, this, &AppController::startNextLibraryPrefetch);

    connect(m_player, &PlayerController::playbackStopped, this, [this](const QString &itemId, qint64 positionTicks) {
        qInfo() << "app: playbackStopped page=" << m_page << "itemId=" << itemId << "positionTicks=" << positionTicks;
        applyPlaybackPosition(itemId, positionTicks);
        schedulePostPlaybackRefresh();
    });
}

QString AppController::page() const
{
    return m_page;
}

QString AppController::serverUrl() const
{
    return m_serverUrl;
}

QString AppController::username() const
{
    return m_username;
}

QString AppController::password() const
{
    return m_password;
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
    return m_currentLibraryName;
}

QString AppController::currentContentLabel() const
{
    return m_currentContentLabel;
}

QString AppController::currentViewKind() const
{
    return m_currentViewKind;
}

QString AppController::currentLibraryId() const
{
    return m_currentLibraryId;
}

QString AppController::currentLibraryCollectionType() const
{
    return m_currentLibraryCollectionType;
}

QVariantMap AppController::libraryQuery() const
{
    return m_libraryQuery;
}

QVariantMap AppController::libraryFilterOptions() const
{
    return m_libraryFilterOptions;
}

int AppController::libraryFilterActiveCount() const
{
    return activeLibraryFilterCount(m_libraryQuery);
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
    m_serverUrl = m_database->loadLastServerUrl();
    m_username = m_database->loadLastUsername();
    m_settings->loadLocal();
    emit serverUrlChanged();
    emit usernameChanged();

    AuthSession session = m_database->loadAuthSession();
    if (!session.accessToken.isEmpty() && !m_serverUrl.isEmpty()) {
        m_api->setServerUrl(m_serverUrl);
        m_api->setSession(session);
        m_settings->loadRemote();
        m_syncPlay->connectSocket();
        loadLibraries();
    } else {
        applyDiscoveredServersCache();
        m_discovery->start();
    }
}

void AppController::setServerUrl(const QString &serverUrl)
{
    if (m_serverUrl == serverUrl)
        return;
    m_serverUrl = serverUrl.trimmed();
    emit serverUrlChanged();
}

void AppController::setUsername(const QString &username)
{
    if (m_username == username)
        return;
    m_username = username;
    emit usernameChanged();
}

void AppController::setPassword(const QString &password)
{
    if (m_password == password)
        return;
    m_password = password;
    emit passwordChanged();
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
    if (m_serverUrl.isEmpty() || m_username.isEmpty()) {
        setErrorText(QStringLiteral("Server and username are required."));
        return;
    }

    setErrorText({});
    setBusy(true, QStringLiteral("Signing in…"));
    m_api->setServerUrl(m_serverUrl);
    
    Async::runScoped(this,
        m_api->authenticateByName(m_username, m_password),
        [this](const AuthSession &session) {
            m_database->saveLoginHints(m_serverUrl, m_username);
            m_database->saveAuthSession(session);
            m_settings->loadRemote();
            m_syncPlay->connectSocket();
            setBusy(false);
            loadLibraries();
            Async::runScoped(this, m_api->postCapabilities(), []() {}, [](const std::exception_ptr &) {});
        },
        [this](const std::exception_ptr &error) {
            setBusy(false);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::logout()
{
    qInfo() << "app: logout requested";
    m_quickConnect->cancel();
    if (m_player->visible())
        m_player->stopWithReason(QStringLiteral("logout"));
    m_database->clearAuthSession();
    m_syncPlay->disconnectSocket();
    m_api->setSession({});
    m_settings->clearRemote();
    m_password.clear();
    m_libraries.clear();
    m_movies.clear();
    m_resumeItems.clear();
    m_nextUpItems.clear();
    m_latestItems.clear();
    ++m_searchGeneration;
    m_searchResults.clear();
    m_searchQuery.clear();
    m_searchBusy = false;
    ++m_detailRowsGeneration;
    m_detailRowsPending = 0;
    m_detailRowsBusy = false;
    m_detailSeasons.clear();
    m_detailSimilarItems.clear();
    m_libraryQueries.clear();
    m_libraryQuery.clear();
    m_libraryFilterOptions.clear();
    m_currentLibraryId.clear();
    m_currentLibraryCollectionType.clear();
    m_currentLibraryName.clear();
    m_currentContentLabel = QStringLiteral("Movies");
    m_currentViewKind.clear();
    m_currentSeriesId.clear();
    m_currentSeriesName.clear();
    m_currentSeasonId.clear();
    resetCurrentItemsPaging();
    setBusy(false);
    setErrorText({});
    emit passwordChanged();
    emit currentLibraryNameChanged();
    emit libraryQueryChanged();
    emit libraryFilterOptionsChanged();
    emit searchChanged();
    emit detailRowsChanged();
    setPage(QStringLiteral("login"));
    applyDiscoveredServersCache();
    m_discovery->start();
}

void AppController::startQuickConnect()
{
    setErrorText({});
    m_quickConnect->start(m_serverUrl);
}

void AppController::cancelQuickConnect()
{
    m_quickConnect->cancel();
}

void AppController::goHome()
{
    if (m_page == QStringLiteral("login"))
        return;

    qInfo() << "app: go home from page=" << m_page << "viewKind=" << m_currentViewKind;
    ++m_libraryLoadGeneration;
    setBusy(false);
    setPage(QStringLiteral("libraries"));
    refreshHomeRows();
}

void AppController::openLibrary(int index)
{
    const auto library = m_libraries.libraryAt(index);
    if (library.id.isEmpty())
        return;

    const int loadGeneration = ++m_libraryLoadGeneration;
    m_currentLibraryId = library.id;
    m_currentLibraryCollectionType = library.collectionType;
    m_currentLibraryName = library.name;
    m_currentSeriesId.clear();
    m_currentSeriesName.clear();
    m_currentSeasonId.clear();
    m_currentViewKind = QStringLiteral("library");
    m_currentContentLabel = libraryContentLabel(library);
    setLibraryQuery(m_libraryQueries.value(library.id, defaultLibraryQuery(library)));
    m_libraryFilterOptions.clear();
    emit libraryFilterOptionsChanged();
    loadLibraryFilterOptions(loadGeneration, library);
    const QString cacheKey = libraryCacheKey(library, m_libraryQuery);
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
        setBusy(true, QStringLiteral("Loading %1…").arg(m_currentContentLabel.toLower()));
    }

    Async::runScoped(this,
        m_api->fetchLibraryPage(library.id, library.collectionType, 0, kLibraryPageSize, m_libraryQuery),
        [this, cacheKey, loadGeneration](const PagedMovieItems &page) {
            if (loadGeneration != m_libraryLoadGeneration)
                return;
            setCurrentItemsPage(page, cacheKey, false);
        },
        [this, loadGeneration, hasWarmCache](const std::exception_ptr &error) {
            if (loadGeneration != m_libraryLoadGeneration)
                return;
            setBusy(false);
            setCurrentItemsLoadingMore(false);
            const QString message = exceptionMessage(error);
            if (hasWarmCache)
                qWarning() << "library open: background refresh failed" << message;
            else
                setErrorText(message);
            if (m_page != QStringLiteral("movies"))
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
    const int generation = ++m_searchGeneration;
    m_searchQuery = trimmed;

    if (trimmed.size() < 2 || !m_api || m_api->session().accessToken.isEmpty()) {
        m_searchBusy = false;
        m_searchResults.clear();
        emit searchChanged();
        return;
    }

    m_searchBusy = true;
    emit searchChanged();

    Async::runScoped(this,
        m_api->searchItems(trimmed),
        [this, generation](const std::vector<MovieItem> &items) {
            if (generation != m_searchGeneration)
                return;
            m_searchResults.setMovies(items);
            prefetchMoviePosters(items);
            m_searchBusy = false;
            emit searchChanged();
        },
        [this, generation](const std::exception_ptr &error) {
            if (generation != m_searchGeneration)
                return;
            m_searchResults.clear();
            m_searchBusy = false;
            emit searchChanged();
            setErrorText(exceptionMessage(error));
        });
}

void AppController::clearSearch()
{
    ++m_searchGeneration;
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

    const int generation = ++m_searchSuggestionsGeneration;
    m_searchSuggestionsBusy = true;
    emit searchSuggestionsChanged();

    Async::runScoped(this,
        m_api->fetchSearchSuggestions(),
        [this, generation](const std::vector<MovieItem> &items) {
            if (generation != m_searchSuggestionsGeneration)
                return;
            m_searchSuggestions.setMovies(items);
            prefetchMoviePosters(items);
            m_searchSuggestionsBusy = false;
            m_searchSuggestionsLoaded = true;
            emit searchSuggestionsChanged();
        },
        [this, generation](const std::exception_ptr &error) {
            if (generation != m_searchSuggestionsGeneration)
                return;
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
    if (m_currentViewKind != QStringLiteral("library"))
        return;

    const int startIndex = std::max(m_currentItemsNextStartIndex, m_movies.rowCount());
    const int loadGeneration = m_libraryLoadGeneration;
    const QString libraryId = m_currentLibraryId;
    const QString collectionType = m_currentLibraryCollectionType;
    const QString cacheKey = m_currentItemsCacheKey;
    const QVariantMap query = m_libraryQuery;
    setCurrentItemsLoadingMore(true);

    const auto onDone = [this, loadGeneration, cacheKey](const PagedMovieItems &page) {
        if (loadGeneration != m_libraryLoadGeneration)
            return;
        setCurrentItemsPage(page, cacheKey, true);
    };
    const auto onError = [this, loadGeneration](const std::exception_ptr &error) {
        if (loadGeneration != m_libraryLoadGeneration)
            return;
        setCurrentItemsLoadingMore(false);
        setErrorText(exceptionMessage(error));
    };

    Async::runScoped(this, m_api->fetchLibraryPage(libraryId, collectionType, startIndex, kLibraryPageSize, query),
                       onDone,
                       onError);
}

void AppController::setLibraryQuery(const QVariantMap &query)
{
    if (m_libraryQuery == query)
        return;
    m_libraryQuery = query;
    if (!m_currentLibraryId.isEmpty())
        m_libraryQueries.insert(m_currentLibraryId, m_libraryQuery);
    emit libraryQueryChanged();
}

void AppController::setLibrarySort(const QString &sortBy, const QString &sortOrder)
{
    QVariantMap query = m_libraryQuery;
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

    QVariantMap query = m_libraryQuery;
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

    QVariantMap query = m_libraryQuery;
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

    QVariantMap query = m_libraryQuery;
    if (!value.isValid() || value.isNull())
        query.remove(key);
    else
        query.insert(key, value.toBool());
    setLibraryQuery(query);
    refreshCurrentLibrary();
}

void AppController::clearLibraryFilters()
{
    if (m_currentLibraryId.isEmpty())
        return;

    QVariantMap query;
    query.insert(QStringLiteral("sortBy"),
                 m_libraryQuery.value(QStringLiteral("sortBy"), QStringLiteral("SortName")).toString());
    query.insert(QStringLiteral("sortOrder"),
                 m_libraryQuery.value(QStringLiteral("sortOrder"), QStringLiteral("Ascending")).toString());
    setLibraryQuery(query);
    refreshCurrentLibrary();
}

void AppController::refreshCurrentLibrary()
{
    if (m_currentViewKind != QStringLiteral("library") || m_currentLibraryId.isEmpty())
        return;
    if (!m_api || m_api->session().accessToken.isEmpty())
        return;

    const int loadGeneration = ++m_libraryLoadGeneration;
    const QString libraryId = m_currentLibraryId;
    const QString collectionType = m_currentLibraryCollectionType;
    const QVariantMap query = m_libraryQuery;

    LibraryItem library;
    library.id = libraryId;
    library.collectionType = collectionType;
    const QString cacheKey = libraryCacheKey(library, query);
    resetCurrentItemsPaging(cacheKey);
    m_movies.clear();
    setBusy(true, QStringLiteral("Loading %1…").arg(m_currentContentLabel.toLower()));

    Async::runScoped(this,
        m_api->fetchLibraryPage(libraryId, collectionType, 0, kLibraryPageSize, query),
        [this, loadGeneration, cacheKey](const PagedMovieItems &page) {
            if (loadGeneration != m_libraryLoadGeneration)
                return;
            setCurrentItemsPage(page, cacheKey, false);
        },
        [this, loadGeneration](const std::exception_ptr &error) {
            if (loadGeneration != m_libraryLoadGeneration)
                return;
            setBusy(false);
            setCurrentItemsLoadingMore(false);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::loadDetailRows(const QString &itemId, const QString &itemType)
{
    const int generation = ++m_detailRowsGeneration;
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
        Async::runScoped(this,
            m_api->fetchSeasons(itemId),
            [this, generation, itemId](const std::vector<MovieItem> &seasons) {
                if (generation != m_detailRowsGeneration)
                    return;
                qInfo() << "detail rows: seasons loaded" << itemId << seasons.size();
                m_detailSeasons.setMovies(seasons);
                prefetchMoviePosters(seasons);
                emit detailRowsChanged();
                finishDetailRowLoad(generation);
            },
            [this, generation, itemId](const std::exception_ptr &error) {
                if (generation != m_detailRowsGeneration)
                    return;
                qWarning() << "detail rows: seasons fetch failed" << itemId << exceptionMessage(error);
                finishDetailRowLoad(generation);
            });
    }

    Async::runScoped(this,
        m_api->fetchSimilarItems(itemId),
        [this, generation, itemId](const std::vector<MovieItem> &items) {
            if (generation != m_detailRowsGeneration)
                return;
            qInfo() << "detail rows: similar loaded" << itemId << items.size();
            m_detailSimilarItems.setMovies(items);
            prefetchMoviePosters(items);
            emit detailRowsChanged();
            finishDetailRowLoad(generation);
        },
        [this, generation, itemId](const std::exception_ptr &error) {
            if (generation != m_detailRowsGeneration)
                return;
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
    const int generation = ++m_personItemsGeneration;
    m_personItems.clear();
    if (personId.isEmpty() || !m_api || m_api->session().accessToken.isEmpty()) {
        m_personItemsBusy = false;
        emit personItemsChanged();
        return;
    }

    m_personItemsBusy = true;
    emit personItemsChanged();

    Async::runScoped(this,
        m_api->fetchItemsByPerson(personId),
        [this, generation](const std::vector<MovieItem> &items) {
            if (generation != m_personItemsGeneration)
                return;
            m_personItems.setMovies(items);
            prefetchMoviePosters(items);
            m_personItemsBusy = false;
            emit personItemsChanged();
        },
        [this, generation](const std::exception_ptr &error) {
            if (generation != m_personItemsGeneration)
                return;
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
    qInfo() << "app: back pressed, page=" << m_page << "settingsVisible=" << settingsVisible();
    if (settingsVisible()) {
        closeSettings();
        return;
    }

    if (m_player->visible()) {
        m_player->stop();
        return;
    }

    if (m_page == QStringLiteral("movies")) {
        if (m_currentViewKind == QStringLiteral("episodes")) {
            MovieItem series;
            series.id = m_currentSeriesId;
            series.title = m_currentSeriesName;
            series.itemType = QStringLiteral("Series");
            series.playable = false;
            openSeries(series);
            return;
        }
        qInfo() << "app: back from movies to libraries";
        setPage(QStringLiteral("libraries"));
        return;
    }

    if (m_page == QStringLiteral("libraries")) {
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
    m_libraryPrefetchTimer.stop();
    m_libraryPrefetchQueue.clear();
    m_libraryPrefetchActive = false;
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
    if (m_page == page)
        return;

    m_page = page;
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
    const auto prefetchedPage = m_prefetchedLibraryPages.constFind(cacheKey);
    if (prefetchedPage != m_prefetchedLibraryPages.constEnd()) {
        m_movies.setMovies(prefetchedPage.value().items);
        prefetchMoviePosters(prefetchedPage.value().items);
        return m_movies.rowCount();
    }
    m_movies.clear();
    return 0;
}

void AppController::loadLibraries()
{
    m_libraryPrefetchTimer.stop();
    m_libraryPrefetchQueue.clear();
    m_libraryPrefetchActive = false;
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
            const QString msg = exceptionMessage(error);
            if (msg.contains(QStringLiteral("(401)")) || msg.contains(QStringLiteral("Unauthorized"))) {
                m_database->clearAuthSession();
                m_api->setSession({});
                setPage(QStringLiteral("login"));
                m_discovery->start();
            } else {
                setErrorText(msg);
            }
        });
}

void AppController::setCurrentItems(const std::vector<MovieItem> &items, const QString &cacheKey)
{
    resetCurrentItemsPaging(cacheKey);
    m_movies.setMovies(items);
    prefetchMoviePosters(items);
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

    prefetchMoviePosters(page.items);
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

void AppController::loadLibraryFilterOptions(int generation, const LibraryItem &library)
{
    if (!m_api || m_api->session().accessToken.isEmpty() || library.id.isEmpty())
        return;

    Async::runScoped(this,
        m_api->fetchLibraryFilterOptions(library.id, library.collectionType),
        [this, generation, library](const QVariantMap &options) {
            if (generation != m_libraryLoadGeneration || library.id != m_currentLibraryId)
                return;
            m_libraryFilterOptions = options;
            emit libraryFilterOptionsChanged();
        },
        [this, generation, library](const std::exception_ptr &error) {
            if (generation != m_libraryLoadGeneration || library.id != m_currentLibraryId)
                return;
            qWarning() << "library filters: failed" << library.name << exceptionMessage(error);
            m_libraryFilterOptions.clear();
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

    const int loadGeneration = ++m_libraryLoadGeneration;
    m_currentSeriesId = series.id;
    m_currentSeriesName = series.title;
    m_currentSeasonId.clear();
    m_currentViewKind = QStringLiteral("seasons");
    m_currentLibraryName = series.title;
    m_currentContentLabel = QStringLiteral("Seasons");
    emit currentLibraryNameChanged();
    resetCurrentItemsPaging(QStringLiteral("seasons/%1").arg(series.id));
    m_movies.clear();
    setBusy(true, QStringLiteral("Loading seasons…"));

    Async::runScoped(this,
        m_api->fetchSeasons(series.id),
        [this, series, loadGeneration](const std::vector<MovieItem> &seasons) {
            if (loadGeneration != m_libraryLoadGeneration)
                return;
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
        [this, loadGeneration](const std::exception_ptr &error) {
            if (loadGeneration != m_libraryLoadGeneration)
                return;
            setBusy(false);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::openSeason(const MovieItem &season)
{
    const QString seriesId = !season.seriesId.isEmpty() ? season.seriesId : m_currentSeriesId;
    if (seriesId.isEmpty()) {
        qWarning() << "season open: missing series id" << season.id << season.title << season.itemType;
        return;
    }

    const int loadGeneration = ++m_libraryLoadGeneration;
    qInfo() << "season open: loading episodes"
            << "series=" << seriesId
            << "season=" << season.id
            << "type=" << season.itemType
            << "title=" << season.title;
    m_currentSeriesId = seriesId;
    m_currentSeasonId = season.itemType == QStringLiteral("Season") ? season.id : QString();
    m_currentViewKind = QStringLiteral("episodes");
    m_currentLibraryName = season.title;
    m_currentContentLabel = QStringLiteral("Episodes");
    emit currentLibraryNameChanged();
    resetCurrentItemsPaging(QStringLiteral("episodes/%1/%2").arg(seriesId, season.id));
    m_movies.clear();
    setBusy(true, QStringLiteral("Loading episodes…"));

    Async::runScoped(this,
        m_api->fetchEpisodes(seriesId, season.itemType == QStringLiteral("Season") ? season.id : QString()),
        [this, seriesId, season, loadGeneration](const std::vector<MovieItem> &episodes) {
            if (loadGeneration != m_libraryLoadGeneration)
                return;
            qInfo() << "season open: episodes loaded"
                    << "series=" << seriesId
                    << "season=" << season.id
                    << "count=" << episodes.size();
            setCurrentItems(episodes, QStringLiteral("episodes/%1/%2").arg(seriesId, season.id));
        },
        [this, loadGeneration](const std::exception_ptr &error) {
            if (loadGeneration != m_libraryLoadGeneration)
                return;
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

    const int loadGeneration = ++m_libraryLoadGeneration;
    m_currentSeriesId.clear();
    m_currentSeriesName.clear();
    m_currentSeasonId.clear();
    m_currentLibraryId.clear();
    m_currentLibraryCollectionType.clear();
    m_currentViewKind = QStringLiteral("genre");
    m_currentLibraryName = name;
    m_currentContentLabel = QStringLiteral("Titles");
    setLibraryQuery(QVariantMap());
    emit currentLibraryNameChanged();
    const QString cacheKey = QStringLiteral("genre/%1").arg(name);
    resetCurrentItemsPaging(cacheKey);
    m_movies.clear();
    setBusy(true, QStringLiteral("Loading %1…").arg(name));

    Async::runScoped(this,
        m_api->fetchItemsByGenre(name),
        [this, cacheKey, loadGeneration](const std::vector<MovieItem> &items) {
            if (loadGeneration != m_libraryLoadGeneration)
                return;
            setCurrentItems(items, cacheKey);
        },
        [this, loadGeneration](const std::exception_ptr &error) {
            if (loadGeneration != m_libraryLoadGeneration)
                return;
            setBusy(false);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::openStudio(const QString &studio)
{
    const QString name = studio.trimmed();
    if (name.isEmpty() || !m_api || m_api->session().accessToken.isEmpty())
        return;

    const int loadGeneration = ++m_libraryLoadGeneration;
    m_currentSeriesId.clear();
    m_currentSeriesName.clear();
    m_currentSeasonId.clear();
    m_currentLibraryId.clear();
    m_currentLibraryCollectionType.clear();
    m_currentViewKind = QStringLiteral("studio");
    m_currentLibraryName = name;
    m_currentContentLabel = QStringLiteral("Titles");
    setLibraryQuery(QVariantMap());
    emit currentLibraryNameChanged();
    const QString cacheKey = QStringLiteral("studio/%1").arg(name);
    resetCurrentItemsPaging(cacheKey);
    m_movies.clear();
    setBusy(true, QStringLiteral("Loading %1…").arg(name));

    Async::runScoped(this,
        m_api->fetchItemsByStudio(name),
        [this, cacheKey, loadGeneration](const std::vector<MovieItem> &items) {
            if (loadGeneration != m_libraryLoadGeneration)
                return;
            setCurrentItems(items, cacheKey);
        },
        [this, loadGeneration](const std::exception_ptr &error) {
            if (loadGeneration != m_libraryLoadGeneration)
                return;
            setBusy(false);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::refreshHomeRows()
{
    if (!m_api || m_api->session().accessToken.isEmpty())
        return;

    const int generation = ++m_homeLoadGeneration;
    clearLatestLibraryRows();
    m_latestItems.clear();
    m_libraryPrefetchTimer.stop();

    std::vector<LibraryItem> latestLibraries;
    const int libraryCount = m_libraries.rowCount();
    latestLibraries.reserve(static_cast<size_t>(libraryCount));
    for (int i = 0; i < libraryCount; ++i) {
        const LibraryItem library = m_libraries.libraryAt(i);
        if (showsLatestLibraryRow(library))
            latestLibraries.push_back(library);
    }

    m_homeLoadsPending = 2 + static_cast<int>(latestLibraries.size());

    Async::runScoped(this,
        m_api->fetchResumeItems(),
        [this, generation](const std::vector<MovieItem> &items) {
            if (generation != m_homeLoadGeneration)
                return;
            qInfo() << "home: resume items" << items.size() << homeItemSample(items);
            m_resumeItems.setMovies(items);
            prefetchMoviePosters(items);
            handleHomeRowLoaded(generation);
        },
        [this, generation](const std::exception_ptr &error) {
            if (generation != m_homeLoadGeneration)
                return;
            qWarning() << "home: resume fetch failed" << exceptionMessage(error);
            handleHomeRowLoaded(generation);
        });

    Async::runScoped(this,
        m_api->fetchNextUpEpisodes(),
        [this, generation](const std::vector<MovieItem> &items) {
            if (generation != m_homeLoadGeneration)
                return;
            qInfo() << "home: next-up items" << items.size() << homeItemSample(items);
            m_nextUpItems.setMovies(items);
            prefetchMoviePosters(items);
            handleHomeRowLoaded(generation);
        },
        [this, generation](const std::exception_ptr &error) {
            if (generation != m_homeLoadGeneration)
                return;
            qWarning() << "home: next-up fetch failed" << exceptionMessage(error);
            handleHomeRowLoaded(generation);
        });

    for (int order = 0; order < static_cast<int>(latestLibraries.size()); ++order) {
        const LibraryItem library = latestLibraries[static_cast<size_t>(order)];
        Async::runScoped(this,
            m_api->fetchLatestItems(library.id, latestLibraryLimit(library)),
            [this, generation, order, library](const std::vector<MovieItem> &items) {
                if (generation != m_homeLoadGeneration)
                    return;
                qInfo() << "home: latest items" << library.name << items.size() << homeItemSample(items);
                addLatestLibraryRow(generation, order, library, items);
                handleHomeRowLoaded(generation);
            },
            [this, generation, library](const std::exception_ptr &error) {
                if (generation != m_homeLoadGeneration)
                    return;
                qWarning() << "home: latest fetch failed" << library.name << exceptionMessage(error);
                handleHomeRowLoaded(generation);
            });
    }
}

void AppController::schedulePostPlaybackRefresh()
{
    const QString viewKind = m_currentViewKind;
    const QString libraryId = m_currentLibraryId;
    const QString seriesId = m_currentSeriesId;
    const QString seasonId = m_currentSeasonId;
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
    if (viewKind != m_currentViewKind || libraryId != m_currentLibraryId ||
        seriesId != m_currentSeriesId || seasonId != m_currentSeasonId)
        return;

    if (viewKind == QStringLiteral("library") && !libraryId.isEmpty()) {
        const int loadGeneration = ++m_libraryLoadGeneration;
        const QString collectionType = m_currentLibraryCollectionType;
        const QString cacheKey = m_currentItemsCacheKey;
        const QVariantMap query = m_libraryQuery;
        Async::runScoped(this,
            m_api->fetchLibraryPage(libraryId, collectionType, 0, kLibraryPageSize, query),
            [this, loadGeneration, cacheKey](const PagedMovieItems &page) {
                if (loadGeneration != m_libraryLoadGeneration)
                    return;
                setCurrentItemsPage(page, cacheKey, false);
            },
            [this, loadGeneration](const std::exception_ptr &error) {
                if (loadGeneration != m_libraryLoadGeneration)
                    return;
                qWarning() << "app: post-playback library refresh failed" << exceptionMessage(error);
            });
        return;
    }

    if (viewKind == QStringLiteral("episodes") && !seriesId.isEmpty()) {
        const int loadGeneration = ++m_libraryLoadGeneration;
        Async::runScoped(this,
            m_api->fetchEpisodes(seriesId, seasonId),
            [this, loadGeneration, seriesId, seasonId](const std::vector<MovieItem> &episodes) {
                if (loadGeneration != m_libraryLoadGeneration)
                    return;
                m_movies.setMovies(episodes);
                prefetchMoviePosters(episodes);
            },
            [this, loadGeneration](const std::exception_ptr &error) {
                if (loadGeneration != m_libraryLoadGeneration)
                    return;
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

void AppController::addLatestLibraryRow(int generation,
                                        int order,
                                        const LibraryItem &library,
                                        const std::vector<MovieItem> &items)
{
    if (generation != m_homeLoadGeneration || items.empty())
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

    prefetchMoviePosters(items);
    emit latestLibraryRowsChanged();
}

void AppController::handleHomeRowLoaded(int generation)
{
    if (generation != m_homeLoadGeneration || m_homeLoadsPending <= 0)
        return;

    --m_homeLoadsPending;
    if (m_homeLoadsPending == 0)
        scheduleLibraryPrefetch(generation);
}

void AppController::finishDetailRowLoad(int generation)
{
    if (generation != m_detailRowsGeneration || m_detailRowsPending <= 0)
        return;

    --m_detailRowsPending;
    if (m_detailRowsPending == 0) {
        m_detailRowsBusy = false;
        emit detailRowsChanged();
    }
}

void AppController::scheduleLibraryPrefetch(int generation)
{
    if (generation != m_homeLoadGeneration || !m_api || m_api->session().accessToken.isEmpty())
        return;

    m_libraryPrefetchQueue.clear();
    const int count = m_libraries.rowCount();
    m_libraryPrefetchQueue.reserve(kBackgroundLibraryPrefetchLimit);
    std::vector<LibraryItem> selectedLibraries;
    selectedLibraries.reserve(kBackgroundLibraryPrefetchLimit);
    QSet<QString> selectedIds;

    auto trySelectLibrary = [&selectedLibraries, &selectedIds](const LibraryItem &library) {
        if (selectedLibraries.size() >= static_cast<size_t>(kBackgroundLibraryPrefetchLimit))
            return;
        if (library.id.isEmpty() || selectedIds.contains(library.id))
            return;
        if (!showsLatestLibraryRow(library))
            return;
        selectedIds.insert(library.id);
        selectedLibraries.push_back(library);
    };

    for (const QString &recentId : std::as_const(m_recentLibraryIds)) {
        for (int i = 0; i < count; ++i) {
            const auto library = m_libraries.libraryAt(i);
            if (library.id == recentId) {
                trySelectLibrary(library);
                break;
            }
        }
    }

    for (int i = 0; i < count; ++i) {
        const auto library = m_libraries.libraryAt(i);
        trySelectLibrary(library);
    }

    for (const LibraryItem &library : selectedLibraries) {
        const QString cacheKey = libraryCacheKey(library);
        if (!m_prefetchedLibraryKeys.contains(cacheKey))
            m_libraryPrefetchQueue.push_back(library);
    }

    QSet<QString> selectedCacheKeys;
    selectedCacheKeys.reserve(static_cast<qsizetype>(selectedLibraries.size()));
    for (const LibraryItem &library : selectedLibraries)
        selectedCacheKeys.insert(libraryCacheKey(library));

    QSet<QString> retainedPrefetchKeys;
    retainedPrefetchKeys.reserve(m_prefetchedLibraryKeys.size());
    for (const QString &cacheKey : std::as_const(m_prefetchedLibraryKeys)) {
        if (selectedCacheKeys.contains(cacheKey))
            retainedPrefetchKeys.insert(cacheKey);
    }
    m_prefetchedLibraryKeys = retainedPrefetchKeys;

    for (auto it = m_prefetchedLibraryPages.begin(); it != m_prefetchedLibraryPages.end();) {
        if (selectedCacheKeys.contains(it.key()))
            ++it;
        else
            it = m_prefetchedLibraryPages.erase(it);
    }

    if (m_libraryPrefetchQueue.empty())
        return;

    m_libraryPrefetchGeneration = generation;
    m_libraryPrefetchIndex = 0;
    m_libraryPrefetchActive = false;
    qInfo() << "library prefetch: scheduled" << m_libraryPrefetchQueue.size()
            << "libraries after home load";
    m_libraryPrefetchTimer.start(1500);
}

void AppController::startNextLibraryPrefetch()
{
    if (m_libraryPrefetchActive || m_libraryPrefetchGeneration != m_homeLoadGeneration)
        return;
    if (!m_api || m_api->session().accessToken.isEmpty())
        return;
    if (m_libraryPrefetchIndex < 0 ||
        m_libraryPrefetchIndex >= static_cast<int>(m_libraryPrefetchQueue.size()))
        return;

    const LibraryItem library = m_libraryPrefetchQueue[static_cast<size_t>(m_libraryPrefetchIndex++)];
    const QString cacheKey = libraryCacheKey(library);
    m_libraryPrefetchActive = true;
    qInfo() << "library prefetch: fetching" << library.name << cacheKey;

    const auto onDone = [this, cacheKey, library](const PagedMovieItems &page) {
        if (m_libraryPrefetchGeneration != m_homeLoadGeneration)
            return;
        qInfo() << "library prefetch: cached" << library.name << page.items.size();
        m_prefetchedLibraryPages.insert(cacheKey, page);
        m_prefetchedLibraryKeys.insert(cacheKey);
        m_libraryPrefetchActive = false;
        startNextLibraryPrefetch();
    };

    const auto onError = [this, cacheKey, library](const std::exception_ptr &error) {
        if (m_libraryPrefetchGeneration != m_homeLoadGeneration)
            return;
        qWarning() << "library prefetch: failed" << library.name << cacheKey << exceptionMessage(error);
        m_libraryPrefetchActive = false;
        startNextLibraryPrefetch();
    };

    Async::runScoped(this, m_api->fetchLibraryPage(library.id, library.collectionType, 0, kLibraryPageSize),
                       onDone,
                       onError);
}

void AppController::prefetchMoviePosters(const std::vector<MovieItem> &movies)
{
    QStringList urls;
    constexpr qsizetype maxUrls = 24;
    urls.reserve(std::min<qsizetype>(static_cast<qsizetype>(movies.size() * 2), maxUrls));

    for (const auto &movie : movies) {
        if (!movie.posterUrl.isEmpty())
            urls.push_back(movie.posterUrl);
        if (!movie.backdropUrl.isEmpty() && urls.size() < maxUrls)
            urls.push_back(movie.backdropUrl);
        if (!movie.logoUrl.isEmpty() && urls.size() < maxUrls)
            urls.push_back(movie.logoUrl);
        if (urls.size() >= maxUrls)
            break;
    }

    if (!urls.isEmpty())
        m_api->prefetchImages(urls, 3);
}

} // namespace JellyfinNative
