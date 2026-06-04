#include "AppController.h"

#include "../api/JellyfinApiFacade.h"
#include "../diagnostics/Diagnostics.h"
#include "../player/PlayerController.h"

#include <QCoroTask>

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

QString normalizedChoice(const QString &value, const QStringList &allowed, const QString &fallback)
{
    return allowed.contains(value) ? value : fallback;
}

QString normalizedSubtitleMode(const QString &mode)
{
    return normalizedChoice(mode,
                            {QStringLiteral("Default"),
                             QStringLiteral("Smart"),
                             QStringLiteral("OnlyForced"),
                             QStringLiteral("Always"),
                             QStringLiteral("None")},
                            QStringLiteral("Default"));
}

QString normalizedSubtitleBurnIn(const QString &mode)
{
    return normalizedChoice(mode,
                            {QString(), QStringLiteral("onlyimageformats"),
                             QStringLiteral("allcomplexformats"), QStringLiteral("all")},
                            QString());
}

QString normalizedSubtitleStyling(const QString &styling)
{
    return normalizedChoice(styling,
                            {QStringLiteral("Auto"), QStringLiteral("Custom"), QStringLiteral("Native")},
                            QStringLiteral("Auto"));
}

QString normalizedSubtitleTextSize(const QString &size)
{
    return normalizedChoice(size,
                            {QStringLiteral("smaller"), QStringLiteral("small"), QString(),
                             QStringLiteral("large"), QStringLiteral("larger"), QStringLiteral("extralarge")},
                            QString());
}

QString normalizedSubtitleTextWeight(const QString &weight)
{
    return normalizedChoice(weight,
                            {QStringLiteral("normal"), QStringLiteral("bold")},
                            QStringLiteral("normal"));
}

QString normalizedSubtitleFont(const QString &font)
{
    return normalizedChoice(font,
                            {QString(), QStringLiteral("typewriter"), QStringLiteral("print"),
                             QStringLiteral("console"), QStringLiteral("cursive"), QStringLiteral("casual"),
                             QStringLiteral("smallcaps")},
                            QString());
}

QString normalizedSubtitleDropShadow(const QString &shadow)
{
    return normalizedChoice(shadow,
                            {QStringLiteral("none"), QStringLiteral("raised"), QStringLiteral("depressed"),
                             QStringLiteral("uniform"), QString()},
                            QString());
}

QString normalizedSubtitleColor(QString color)
{
    color = color.trimmed().toLower();
    if (!color.startsWith(QLatin1Char('#')) || color.size() != 7)
        return QStringLiteral("#ffffff");
    for (int i = 1; i < color.size(); ++i) {
        const QChar ch = color.at(i);
        if (!ch.isDigit() && (ch < QLatin1Char('a') || ch > QLatin1Char('f')))
            return QStringLiteral("#ffffff");
    }
    return color;
}

bool loadBoolSetting(DatabaseManager *database, const QString &key, bool fallback)
{
    const QString value = database->loadSetting(key, fallback ? QStringLiteral("true")
                                                             : QStringLiteral("false"))
                              .trimmed()
                              .toLower();
    return value == QStringLiteral("true") ||
           value == QStringLiteral("1") ||
           value == QStringLiteral("yes");
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
    connect(m_syncPlay, &SyncPlayController::errorText, this, &AppController::setErrorText);
    connect(m_discovery, &DiscoveryController::serverDiscovered, this, [this](const DiscoveredServer &server) {
        m_discoveredServers.upsertServer(server);
        QJsonArray cache;
        for (const auto &entry : m_discoveredServers.servers())
            cache.push_back(toJson(entry));
        m_database->saveDiscoveredServers(cache);
    });

    connect(&m_quickConnectTimer, &QTimer::timeout, this, &AppController::pollQuickConnect);
    m_quickConnectTimer.setInterval(5000);
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
    return m_quickConnectCode;
}

QString AppController::quickConnectStatus() const
{
    return m_quickConnectStatus;
}

bool AppController::quickConnectActive() const
{
    return !m_quickConnectSecret.isEmpty();
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
    return m_settingsVisible;
}

bool AppController::nightModeEnabled() const
{
    return m_nightModeEnabled;
}

int AppController::audioDelayMs() const
{
    return m_audioDelayMs;
}

QString AppController::audioOutputMode() const
{
    return m_audioOutputMode;
}

QStringList AppController::subtitleLanguageOptions() const
{
    return m_subtitleLanguageLabels;
}

int AppController::subtitleLanguageIndex() const
{
    const int index = m_subtitleLanguageCodes.indexOf(m_subtitlePreferences.language);
    return index >= 0 ? index : 0;
}

QString AppController::subtitleMode() const
{
    return m_subtitlePreferences.mode;
}

QString AppController::subtitleBurnIn() const
{
    return m_subtitlePreferences.burnInMode;
}

bool AppController::subtitleRenderPgs() const
{
    return m_subtitlePreferences.renderPgs;
}

bool AppController::subtitleAlwaysBurnIn() const
{
    return m_subtitlePreferences.alwaysBurnInWhenTranscoding;
}

QString AppController::subtitleStyling() const
{
    return m_subtitlePreferences.styling;
}

QString AppController::subtitleTextSize() const
{
    return m_subtitlePreferences.textSize;
}

QString AppController::subtitleTextWeight() const
{
    return m_subtitlePreferences.textWeight;
}

QString AppController::subtitleFont() const
{
    return m_subtitlePreferences.font;
}

QString AppController::subtitleTextColor() const
{
    return m_subtitlePreferences.textColor;
}

QString AppController::subtitleDropShadow() const
{
    return m_subtitlePreferences.dropShadow;
}

int AppController::subtitleVerticalPosition() const
{
    return m_subtitlePreferences.verticalPosition;
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
    m_nightModeEnabled = m_database->loadNightModeEnabled();
    m_audioDelayMs = m_database->loadAudioDelayMs();
    const QString storedAudioOutputMode = m_database->loadAudioOutputMode();
    m_audioOutputMode = (storedAudioOutputMode == QStringLiteral("starfish") ||
                         storedAudioOutputMode == QStringLiteral("starfish-pcm"))
                            ? QStringLiteral("starfish-pcm")
                            : QStringLiteral("alsa");
    if (m_audioOutputMode != storedAudioOutputMode)
        m_database->saveAudioOutputMode(m_audioOutputMode);
    m_redButtonAction = m_database->loadSetting(QStringLiteral("input/redButton"), QStringLiteral("none"));
    m_greenButtonAction = m_database->loadSetting(QStringLiteral("input/greenButton"), QStringLiteral("skipBackAndEnableSubs"));
    m_yellowButtonAction = m_database->loadSetting(QStringLiteral("input/yellowButton"), QStringLiteral("none"));
    m_blueButtonAction = m_database->loadSetting(QStringLiteral("input/blueButton"), QStringLiteral("none"));
    loadSubtitlePreferences();
    m_player->setNightModeEnabled(m_nightModeEnabled);
    m_player->setAudioDelayMs(m_audioDelayMs);
    m_player->setAudioOutputMode(m_audioOutputMode);
    applySubtitlePreferencesToPlayer();
    emit serverUrlChanged();
    emit usernameChanged();
    emit nightModeEnabledChanged();
    emit audioDelayMsChanged();
    emit audioOutputModeChanged();
    emit subtitleSettingsChanged();
    emit buttonRemapChanged();

    AuthSession session = m_database->loadAuthSession();
    if (!session.accessToken.isEmpty() && !m_serverUrl.isEmpty()) {
        m_api->setServerUrl(m_serverUrl);
        m_api->setSession(session);
        loadSubtitleRemoteSettings();
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
    
    QCoro::runDetached(
        m_api->authenticateByName(m_username, m_password),
        [this](const AuthSession &session) {
            m_database->saveLoginHints(m_serverUrl, m_username);
            m_database->saveAuthSession(session);
            loadSubtitleRemoteSettings();
            m_syncPlay->connectSocket();
            setBusy(false);
            loadLibraries();
            QCoro::runDetached(m_api->postCapabilities(), []() {}, [](const std::exception_ptr &) {});
        },
        [this](const std::exception_ptr &error) {
            setBusy(false);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::logout()
{
    qInfo() << "app: logout requested";
    m_quickConnectTimer.stop();
    m_quickConnectCode.clear();
    m_quickConnectStatus.clear();
    m_quickConnectSecret.clear();
    m_quickConnectPollAttempts = 0;
    m_quickConnectPollErrors = 0;
    if (m_player->visible())
        m_player->stopWithReason(QStringLiteral("logout"));
    m_database->clearAuthSession();
    m_syncPlay->disconnectSocket();
    m_api->setSession({});
    m_userConfiguration = {};
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
    emit quickConnectChanged();
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
    if (m_serverUrl.isEmpty()) {
        setErrorText(QStringLiteral("Enter a Jellyfin server URL first."));
        return;
    }

    setErrorText({});
    setBusy(true, QStringLiteral("Starting Quick Connect…"));
    m_api->setServerUrl(m_serverUrl);
    m_quickConnectPollAttempts = 0;
    m_quickConnectPollErrors = 0;
    qInfo() << "quick connect: starting for" << m_serverUrl;

    QCoro::runDetached(
        m_api->quickConnectEnabled(),
        [this](bool enabled) {
            if (!enabled) {
                setBusy(false);
                setErrorText(QStringLiteral("Quick Connect is disabled on this server."));
                return;
            }

            QCoro::runDetached(
                m_api->initiateQuickConnect(),
                [this](const QJsonObject &result) {
                    setBusy(false);
                    m_quickConnectCode = result.value(QStringLiteral("Code")).toString();
                    m_quickConnectSecret = result.value(QStringLiteral("Secret")).toString();
                    m_quickConnectStatus = QStringLiteral("Waiting for authorization…");
                    m_quickConnectPollAttempts = 0;
                    m_quickConnectPollErrors = 0;
                    qInfo() << "quick connect: initiated code" << m_quickConnectCode
                            << "deviceId" << result.value(QStringLiteral("DeviceId")).toString();
                    emit quickConnectChanged();
                    pollQuickConnect();
                    m_quickConnectTimer.start();
                },
                [this](const std::exception_ptr &error) {
                    setBusy(false);
                    qWarning() << "quick connect: initiate failed" << exceptionMessage(error);
                    setErrorText(exceptionMessage(error));
                });
        },
        [this](const std::exception_ptr &error) {
            setBusy(false);
            qWarning() << "quick connect: enabled check failed" << exceptionMessage(error);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::cancelQuickConnect()
{
    m_quickConnectTimer.stop();
    m_quickConnectCode.clear();
    m_quickConnectStatus.clear();
    m_quickConnectSecret.clear();
    m_quickConnectPollAttempts = 0;
    m_quickConnectPollErrors = 0;
    emit quickConnectChanged();
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

    QCoro::runDetached(
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

void AppController::playMovie(int index, bool fromStart)
{
    const auto item = m_movies.movieAt(index);
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
    const auto item = m_latestItems.movieAt(index);
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

    const auto item = model->movieAt(itemIndex);
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

    QCoro::runDetached(
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

    QCoro::runDetached(
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
    const auto item = m_searchSuggestions.movieAt(index);
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

void AppController::playSearchResult(int index, bool fromStart)
{
    const auto item = m_searchResults.movieAt(index);
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

    QCoro::runDetached(m_api->fetchLibraryPage(libraryId, collectionType, startIndex, kLibraryPageSize, query),
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

    QCoro::runDetached(
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
        QCoro::runDetached(
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

    QCoro::runDetached(
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
    const auto item = m_detailSimilarItems.movieAt(index);
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

    QCoro::runDetached(
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
    const auto item = m_personItems.movieAt(index);
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

void AppController::setFavorite(const QString &itemId, bool favorite)
{
    if (itemId.isEmpty() || !m_api || m_api->session().accessToken.isEmpty())
        return;

    applyFavoriteState(itemId, favorite);
    QCoro::runDetached(
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
    QCoro::runDetached(
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
    QCoro::runDetached(
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
            QCoro::runDetached(
                api->fetchMediaSegments(itemId),
                [sharedSession, kickoff](const std::vector<MediaSegment> &segments) {
                    sharedSession->segments = segments;
                    kickoff();
                },
                [kickoff](const std::exception_ptr &) { kickoff(); });
            QCoro::runDetached(
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
    qInfo() << "app: back pressed, page=" << m_page << "settingsVisible=" << m_settingsVisible;
    if (m_settingsVisible) {
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
    m_quickConnectTimer.stop();
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
    if (m_settingsVisible)
        return;
    m_settingsVisible = true;
    emit settingsVisibleChanged();
}

void AppController::closeSettings()
{
    if (!m_settingsVisible)
        return;
    m_settingsVisible = false;
    emit settingsVisibleChanged();
}

void AppController::toggleNightMode()
{
    setNightModeEnabled(!m_nightModeEnabled);
}

void AppController::setNightModeEnabled(bool enabled)
{
    if (m_nightModeEnabled == enabled)
        return;
    m_nightModeEnabled = enabled;
    m_database->saveNightModeEnabled(enabled);
    m_player->setNightModeEnabled(enabled);
    emit nightModeEnabledChanged();
}

void AppController::setAudioDelayMs(int delayMs)
{
    const int clampedDelayMs = std::clamp(delayMs, -2000, 2000);
    if (m_audioDelayMs == clampedDelayMs) {
        qInfo() << "app: audio delay unchanged" << clampedDelayMs << "ms";
        return;
    }
    qInfo() << "app: audio delay changed" << m_audioDelayMs << "->" << clampedDelayMs << "ms";
    m_audioDelayMs = clampedDelayMs;
    m_database->saveAudioDelayMs(clampedDelayMs);
    m_player->setAudioDelayMs(clampedDelayMs);
    emit audioDelayMsChanged();
}

void AppController::setAudioOutputMode(const QString &mode)
{
    const QString normalized =
        (mode == QStringLiteral("starfish") || mode == QStringLiteral("starfish-pcm"))
            ? QStringLiteral("starfish-pcm")
            : QStringLiteral("alsa");
    if (m_audioOutputMode == normalized)
        return;
    qInfo() << "app: audio output mode changed" << m_audioOutputMode << "->" << normalized;
    m_audioOutputMode = normalized;
    m_database->saveAudioOutputMode(normalized);
    m_player->setAudioOutputMode(normalized);
    emit audioOutputModeChanged();
}

void AppController::setSubtitleLanguageIndex(int index)
{
    if (index < 0 || index >= m_subtitleLanguageCodes.size())
        return;
    const QString language = m_subtitleLanguageCodes.at(index);
    if (m_subtitlePreferences.language == language)
        return;
    m_subtitlePreferences.language = language;
    saveSubtitlePreferences();
    saveSubtitleUserConfiguration();
    applySubtitlePreferencesToPlayer();
    emit subtitleSettingsChanged();
}

void AppController::setSubtitleMode(const QString &mode)
{
    const QString normalized = normalizedSubtitleMode(mode);
    if (m_subtitlePreferences.mode == normalized)
        return;
    m_subtitlePreferences.mode = normalized;
    saveSubtitlePreferences();
    saveSubtitleUserConfiguration();
    applySubtitlePreferencesToPlayer();
    emit subtitleSettingsChanged();
}

void AppController::setSubtitleBurnIn(const QString &mode)
{
    const QString normalized = normalizedSubtitleBurnIn(mode);
    if (m_subtitlePreferences.burnInMode == normalized)
        return;
    m_subtitlePreferences.burnInMode = normalized;
    saveSubtitlePreferences();
    emit subtitleSettingsChanged();
}

void AppController::setSubtitleRenderPgs(bool enabled)
{
    if (m_subtitlePreferences.renderPgs == enabled)
        return;
    m_subtitlePreferences.renderPgs = enabled;
    saveSubtitlePreferences();
    emit subtitleSettingsChanged();
}

void AppController::setSubtitleAlwaysBurnIn(bool enabled)
{
    if (m_subtitlePreferences.alwaysBurnInWhenTranscoding == enabled)
        return;
    m_subtitlePreferences.alwaysBurnInWhenTranscoding = enabled;
    saveSubtitlePreferences();
    emit subtitleSettingsChanged();
}

void AppController::setSubtitleStyling(const QString &styling)
{
    const QString normalized = normalizedSubtitleStyling(styling);
    if (m_subtitlePreferences.styling == normalized)
        return;
    m_subtitlePreferences.styling = normalized;
    saveSubtitlePreferences();
    applySubtitlePreferencesToPlayer();
    emit subtitleSettingsChanged();
}

void AppController::setSubtitleTextSize(const QString &size)
{
    const QString normalized = normalizedSubtitleTextSize(size);
    if (m_subtitlePreferences.textSize == normalized)
        return;
    m_subtitlePreferences.textSize = normalized;
    saveSubtitlePreferences();
    applySubtitlePreferencesToPlayer();
    emit subtitleSettingsChanged();
}

void AppController::setSubtitleTextWeight(const QString &weight)
{
    const QString normalized = normalizedSubtitleTextWeight(weight);
    if (m_subtitlePreferences.textWeight == normalized)
        return;
    m_subtitlePreferences.textWeight = normalized;
    saveSubtitlePreferences();
    applySubtitlePreferencesToPlayer();
    emit subtitleSettingsChanged();
}

void AppController::setSubtitleFont(const QString &font)
{
    const QString normalized = normalizedSubtitleFont(font);
    if (m_subtitlePreferences.font == normalized)
        return;
    m_subtitlePreferences.font = normalized;
    saveSubtitlePreferences();
    applySubtitlePreferencesToPlayer();
    emit subtitleSettingsChanged();
}

void AppController::setSubtitleTextColor(const QString &color)
{
    const QString normalized = normalizedSubtitleColor(color);
    if (m_subtitlePreferences.textColor == normalized)
        return;
    m_subtitlePreferences.textColor = normalized;
    saveSubtitlePreferences();
    applySubtitlePreferencesToPlayer();
    emit subtitleSettingsChanged();
}

void AppController::setSubtitleDropShadow(const QString &shadow)
{
    const QString normalized = normalizedSubtitleDropShadow(shadow);
    if (m_subtitlePreferences.dropShadow == normalized)
        return;
    m_subtitlePreferences.dropShadow = normalized;
    saveSubtitlePreferences();
    applySubtitlePreferencesToPlayer();
    emit subtitleSettingsChanged();
}

void AppController::setSubtitleVerticalPosition(int position)
{
    const int clamped = std::clamp(position, -16, 16);
    if (m_subtitlePreferences.verticalPosition == clamped)
        return;
    m_subtitlePreferences.verticalPosition = clamped;
    saveSubtitlePreferences();
    applySubtitlePreferencesToPlayer();
    emit subtitleSettingsChanged();
}

QString AppController::redButtonAction() const { return m_redButtonAction; }
QString AppController::greenButtonAction() const { return m_greenButtonAction; }
QString AppController::yellowButtonAction() const { return m_yellowButtonAction; }
QString AppController::blueButtonAction() const { return m_blueButtonAction; }

QStringList AppController::availableButtonActions() const
{
    return {
        QStringLiteral("none"),
        QStringLiteral("togglePause"),
        QStringLiteral("toggleSubs"),
        QStringLiteral("cycleSubs"),
        QStringLiteral("cycleAudio"),
        QStringLiteral("skipBack10"),
        QStringLiteral("skipForward10"),
        QStringLiteral("skipBack30"),
        QStringLiteral("skipForward30"),
        QStringLiteral("skipBack90"),
        QStringLiteral("skipForward90"),
        QStringLiteral("skipBackAndEnableSubs"),
        QStringLiteral("skipSegment"),
        QStringLiteral("showInfo"),
        QStringLiteral("stop")
    };
}

QString AppController::buttonActionLabel(const QString &action) const
{
    if (action == QStringLiteral("none")) return QStringLiteral("No action");
    if (action == QStringLiteral("togglePause")) return QStringLiteral("Play / Pause");
    if (action == QStringLiteral("toggleSubs")) return QStringLiteral("Toggle subtitles");
    if (action == QStringLiteral("cycleSubs")) return QStringLiteral("Cycle subtitles");
    if (action == QStringLiteral("cycleAudio")) return QStringLiteral("Cycle audio track");
    if (action == QStringLiteral("skipBack10")) return QStringLiteral("Skip back 10 s");
    if (action == QStringLiteral("skipForward10")) return QStringLiteral("Skip forward 10 s");
    if (action == QStringLiteral("skipBack30")) return QStringLiteral("Skip back 30 s");
    if (action == QStringLiteral("skipForward30")) return QStringLiteral("Skip forward 30 s");
    if (action == QStringLiteral("skipBack90")) return QStringLiteral("Skip back 90 s");
    if (action == QStringLiteral("skipForward90")) return QStringLiteral("Skip forward 90 s");
    if (action == QStringLiteral("skipBackAndEnableSubs")) return QStringLiteral("Skip back 10 s + enable subs");
    if (action == QStringLiteral("skipSegment")) return QStringLiteral("Skip intro / outro");
    if (action == QStringLiteral("showInfo")) return QStringLiteral("Show info");
    if (action == QStringLiteral("stop")) return QStringLiteral("Stop playback");
    return action;
}

void AppController::setRedButtonAction(const QString &action)
{
    if (m_redButtonAction == action) return;
    m_redButtonAction = action;
    m_database->saveSetting(QStringLiteral("input/redButton"), action);
    emit buttonRemapChanged();
}

void AppController::setGreenButtonAction(const QString &action)
{
    if (m_greenButtonAction == action) return;
    m_greenButtonAction = action;
    m_database->saveSetting(QStringLiteral("input/greenButton"), action);
    emit buttonRemapChanged();
}

void AppController::setYellowButtonAction(const QString &action)
{
    if (m_yellowButtonAction == action) return;
    m_yellowButtonAction = action;
    m_database->saveSetting(QStringLiteral("input/yellowButton"), action);
    emit buttonRemapChanged();
}

void AppController::setBlueButtonAction(const QString &action)
{
    if (m_blueButtonAction == action) return;
    m_blueButtonAction = action;
    m_database->saveSetting(QStringLiteral("input/blueButton"), action);
    emit buttonRemapChanged();
}

void AppController::loadSubtitlePreferences()
{
    SubtitlePreferences preferences;
    preferences.language = m_database->loadSetting(QStringLiteral("subtitles/language"), QString());
    preferences.mode = normalizedSubtitleMode(
        m_database->loadSetting(QStringLiteral("subtitles/mode"), QStringLiteral("Default")));
    preferences.burnInMode = normalizedSubtitleBurnIn(
        m_database->loadSetting(QStringLiteral("subtitles/burnIn"), QString()));
    preferences.renderPgs = loadBoolSetting(m_database, QStringLiteral("subtitles/renderPgs"), false);
    preferences.alwaysBurnInWhenTranscoding =
        loadBoolSetting(m_database, QStringLiteral("subtitles/alwaysBurnInWhenTranscoding"), false);
    preferences.styling = normalizedSubtitleStyling(
        m_database->loadSetting(QStringLiteral("subtitles/styling"), QStringLiteral("Auto")));
    preferences.textSize = normalizedSubtitleTextSize(
        m_database->loadSetting(QStringLiteral("subtitles/textSize"), QString()));
    preferences.textWeight = normalizedSubtitleTextWeight(
        m_database->loadSetting(QStringLiteral("subtitles/textWeight"), QStringLiteral("normal")));
    preferences.font = normalizedSubtitleFont(
        m_database->loadSetting(QStringLiteral("subtitles/font"), QString()));
    preferences.textColor = normalizedSubtitleColor(
        m_database->loadSetting(QStringLiteral("subtitles/textColor"), QStringLiteral("#ffffff")));
    preferences.dropShadow = normalizedSubtitleDropShadow(
        m_database->loadSetting(QStringLiteral("subtitles/dropShadow"), QString()));
    preferences.textBackground =
        m_database->loadSetting(QStringLiteral("subtitles/textBackground"), QStringLiteral("transparent"));
    preferences.verticalPosition =
        std::clamp(m_database->loadSetting(QStringLiteral("subtitles/verticalPosition"), QStringLiteral("-3")).toInt(),
                   -16,
                   16);
    m_subtitlePreferences = preferences;
}

void AppController::saveSubtitlePreferences()
{
    m_database->saveSetting(QStringLiteral("subtitles/language"), m_subtitlePreferences.language);
    m_database->saveSetting(QStringLiteral("subtitles/mode"), m_subtitlePreferences.mode);
    m_database->saveSetting(QStringLiteral("subtitles/burnIn"), m_subtitlePreferences.burnInMode);
    m_database->saveSetting(QStringLiteral("subtitles/renderPgs"),
                            m_subtitlePreferences.renderPgs ? QStringLiteral("true") : QStringLiteral("false"));
    m_database->saveSetting(QStringLiteral("subtitles/alwaysBurnInWhenTranscoding"),
                            m_subtitlePreferences.alwaysBurnInWhenTranscoding ? QStringLiteral("true")
                                                                              : QStringLiteral("false"));
    m_database->saveSetting(QStringLiteral("subtitles/styling"), m_subtitlePreferences.styling);
    m_database->saveSetting(QStringLiteral("subtitles/textSize"), m_subtitlePreferences.textSize);
    m_database->saveSetting(QStringLiteral("subtitles/textWeight"), m_subtitlePreferences.textWeight);
    m_database->saveSetting(QStringLiteral("subtitles/font"), m_subtitlePreferences.font);
    m_database->saveSetting(QStringLiteral("subtitles/textColor"), m_subtitlePreferences.textColor);
    m_database->saveSetting(QStringLiteral("subtitles/dropShadow"), m_subtitlePreferences.dropShadow);
    m_database->saveSetting(QStringLiteral("subtitles/textBackground"), m_subtitlePreferences.textBackground);
    m_database->saveSetting(QStringLiteral("subtitles/verticalPosition"),
                            QString::number(m_subtitlePreferences.verticalPosition));
}

void AppController::loadSubtitleRemoteSettings()
{
    if (!m_api || m_api->session().accessToken.isEmpty())
        return;

    QCoro::runDetached(
        m_api->fetchCultures(),
        [this](const QJsonArray &cultures) {
            QStringList codes{QString()};
            QStringList labels{QStringLiteral("Any language")};
            QSet<QString> seen{QString()};

            for (const QJsonValue &value : cultures) {
                const QJsonObject culture = value.toObject();
                const QString code = culture.value(QStringLiteral("ThreeLetterISOLanguageName")).toString();
                if (code.isEmpty() || seen.contains(code))
                    continue;
                QString label = culture.value(QStringLiteral("DisplayName")).toString();
                if (label.isEmpty())
                    label = code.toUpper();
                seen.insert(code);
                codes.push_back(code);
                labels.push_back(label);
            }

            if (!m_subtitlePreferences.language.isEmpty() && !seen.contains(m_subtitlePreferences.language)) {
                codes.push_back(m_subtitlePreferences.language);
                labels.push_back(m_subtitlePreferences.language.toUpper());
            }

            m_subtitleLanguageCodes = codes;
            m_subtitleLanguageLabels = labels;
            emit subtitleSettingsChanged();
        },
        [this](const std::exception_ptr &error) {
            qWarning() << "subtitles: culture list failed" << exceptionMessage(error);
        });

    QCoro::runDetached(
        m_api->fetchUserConfiguration(),
        [this](const QJsonObject &configuration) {
            m_userConfiguration = configuration;
            m_subtitlePreferences.language =
                configuration.value(QStringLiteral("SubtitleLanguagePreference")).toString();
            m_subtitlePreferences.mode = normalizedSubtitleMode(
                configuration.value(QStringLiteral("SubtitleMode")).toString(QStringLiteral("Default")));
            saveSubtitlePreferences();
            applySubtitlePreferencesToPlayer();
            emit subtitleSettingsChanged();
        },
        [this](const std::exception_ptr &error) {
            qWarning() << "subtitles: user configuration failed" << exceptionMessage(error);
        });
}

void AppController::saveSubtitleUserConfiguration()
{
    if (!m_api || m_api->session().accessToken.isEmpty())
        return;

    QJsonObject configuration = m_userConfiguration;
    configuration.insert(QStringLiteral("SubtitleLanguagePreference"), m_subtitlePreferences.language);
    configuration.insert(QStringLiteral("SubtitleMode"), m_subtitlePreferences.mode);
    m_userConfiguration = configuration;

    QCoro::runDetached(
        m_api->updateUserConfiguration(configuration),
        []() {},
        [this](const std::exception_ptr &error) {
            setErrorText(exceptionMessage(error));
        });
}

void AppController::applySubtitlePreferencesToPlayer()
{
    if (m_player)
        m_player->setSubtitlePreferences(m_subtitlePreferences);
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
    QCoro::runDetached(
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

void AppController::pollQuickConnect()
{
    if (m_quickConnectSecret.isEmpty())
        return;

    m_quickConnectPollAttempts += 1;
    if (m_quickConnectPollAttempts > 36) {
        qWarning() << "quick connect: timed out after polls" << m_quickConnectPollAttempts;
        cancelQuickConnect();
        setErrorText(QStringLiteral("Quick Connect timed out."));
        return;
    }

    QCoro::runDetached(
        m_api->pollQuickConnect(m_quickConnectSecret),
        [this](const QJsonObject &result) {
            m_quickConnectPollErrors = 0;
            const bool authenticated = result.value(QStringLiteral("Authenticated")).toBool();
            qInfo() << "quick connect: poll" << m_quickConnectPollAttempts
                    << "authenticated" << authenticated;
            if (!authenticated)
                return;

            m_quickConnectTimer.stop();
            m_quickConnectStatus = QStringLiteral("Authorized. Signing in…");
            emit quickConnectChanged();

            QCoro::runDetached(
                m_api->authenticateWithQuickConnect(m_quickConnectSecret),
                [this](const AuthSession &session) {
                    qInfo() << "quick connect: authenticated successfully";
                    cancelQuickConnect();
                    m_database->saveLoginHints(m_serverUrl, m_username);
                    m_database->saveAuthSession(session);
                    loadSubtitleRemoteSettings();
                    m_syncPlay->connectSocket();
                    loadLibraries();
                    QCoro::runDetached(m_api->postCapabilities(), []() {}, [](const std::exception_ptr &) {});
                },
                [this](const std::exception_ptr &error) {
                    qWarning() << "quick connect: token exchange failed" << exceptionMessage(error);
                    cancelQuickConnect();
                    setErrorText(exceptionMessage(error));
                });
        },
        [this](const std::exception_ptr &error) {
            const QString message = exceptionMessage(error);
            m_quickConnectPollErrors += 1;
            qWarning() << "quick connect: poll failed" << m_quickConnectPollErrors << message;

            if (message.contains(QStringLiteral("(401)")) ||
                message.contains(QStringLiteral("(404)")) ||
                m_quickConnectPollErrors >= 6) {
                cancelQuickConnect();
                setErrorText(message);
                return;
            }

            if (!m_quickConnectSecret.isEmpty()) {
                m_quickConnectStatus = QStringLiteral("Waiting for authorization…");
                emit quickConnectChanged();
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

    QCoro::runDetached(
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

    QCoro::runDetached(
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

    QCoro::runDetached(
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

    QCoro::runDetached(
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

    QCoro::runDetached(
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
        QCoro::runDetached(
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
        QCoro::runDetached(
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
        QCoro::runDetached(
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

    QCoro::runDetached(m_api->fetchLibraryPage(library.id, library.collectionType, 0, kLibraryPageSize),
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
