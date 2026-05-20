#include "AppController.h"

#include "../api/JellyfinApiFacade.h"
#include "../diagnostics/Diagnostics.h"
#include "../player/PlayerController.h"

#include <QCoroTask>

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QStringList>

#include <algorithm>

namespace JellyfinNative {

namespace {

template<typename T>
QJsonArray toJsonArray(const std::vector<T> &items)
{
    QJsonArray array;
    for (const auto &item : items)
        array.push_back(toJson(item));
    return array;
}

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

QString libraryCacheKey(const LibraryItem &library)
{
    return isSeriesLibrary(library) ? QStringLiteral("series/%1").arg(library.id) : library.id;
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
    m_audioOutputMode = m_database->loadAudioOutputMode() == QStringLiteral("starfish") ? QStringLiteral("starfish") : QStringLiteral("alsa");
    m_redButtonAction = m_database->loadSetting(QStringLiteral("input/redButton"), QStringLiteral("none"));
    m_greenButtonAction = m_database->loadSetting(QStringLiteral("input/greenButton"), QStringLiteral("skipBackAndEnableSubs"));
    m_yellowButtonAction = m_database->loadSetting(QStringLiteral("input/yellowButton"), QStringLiteral("none"));
    m_blueButtonAction = m_database->loadSetting(QStringLiteral("input/blueButton"), QStringLiteral("none"));
    m_player->setNightModeEnabled(m_nightModeEnabled);
    m_player->setAudioDelayMs(m_audioDelayMs);
    m_player->setAudioOutputMode(m_audioOutputMode);
    emit serverUrlChanged();
    emit usernameChanged();
    emit nightModeEnabledChanged();
    emit audioDelayMsChanged();
    emit audioOutputModeChanged();
    emit buttonRemapChanged();

    AuthSession session = m_database->loadAuthSession();
    if (!session.accessToken.isEmpty() && !m_serverUrl.isEmpty()) {
        m_api->setServerUrl(m_serverUrl);
        m_api->setSession(session);
        loadLibraries();
    } else {
        applyDiscoveredServersCache();
        applyLibrariesCache();
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
    m_api->setSession({});
    m_password.clear();
    m_libraries.clear();
    m_movies.clear();
    m_resumeItems.clear();
    m_nextUpItems.clear();
    m_latestItems.clear();
    m_currentLibraryId.clear();
    m_currentLibraryName.clear();
    m_currentContentLabel = QStringLiteral("Movies");
    m_currentViewKind.clear();
    m_currentSeriesId.clear();
    m_currentSeriesName.clear();
    m_currentSeasonId.clear();
    setBusy(false);
    setErrorText({});
    emit passwordChanged();
    emit quickConnectChanged();
    emit currentLibraryNameChanged();
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
    m_currentLibraryName = library.name;
    m_currentSeriesId.clear();
    m_currentSeriesName.clear();
    m_currentSeasonId.clear();
    m_currentViewKind = library.collectionType == QStringLiteral("tvshows") ? QStringLiteral("series")
                                                                            : QStringLiteral("movies");
    m_currentContentLabel = m_currentViewKind == QStringLiteral("series") ? QStringLiteral("TV Shows")
                                                                          : QStringLiteral("Movies");
    emit currentLibraryNameChanged();
    applyMoviesCache(m_currentViewKind == QStringLiteral("series")
                         ? QStringLiteral("series/%1").arg(library.id)
                         : library.id);
    setBusy(true, m_currentViewKind == QStringLiteral("series") ? QStringLiteral("Loading shows…")
                                                                : QStringLiteral("Loading movies…"));

    if (m_currentViewKind == QStringLiteral("series")) {
        QCoro::runDetached(
            m_api->fetchSeries(library.id),
            [this, library, loadGeneration](const std::vector<MovieItem> &items) {
                if (loadGeneration != m_libraryLoadGeneration)
                    return;
                setCurrentItems(items, QStringLiteral("series/%1").arg(library.id));
            },
            [this, loadGeneration](const std::exception_ptr &error) {
                if (loadGeneration != m_libraryLoadGeneration)
                    return;
                setBusy(false);
                setErrorText(exceptionMessage(error));
                if (m_page != QStringLiteral("movies"))
                    setPage(QStringLiteral("movies"));
            });
    } else {
        QCoro::runDetached(
            m_api->fetchMovies(library.id),
            [this, library, loadGeneration](const std::vector<MovieItem> &movies) {
                if (loadGeneration != m_libraryLoadGeneration)
                    return;
                setCurrentItems(movies, library.id);
            },
            [this, loadGeneration](const std::exception_ptr &error) {
                if (loadGeneration != m_libraryLoadGeneration)
                    return;
                setBusy(false);
                setErrorText(exceptionMessage(error));
                if (m_page != QStringLiteral("movies"))
                    setPage(QStringLiteral("movies"));
            });
    }
}

void AppController::playMovie(int index)
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

    playMediaItem(item);
}

void AppController::playResumeItem(int index)
{
    const auto item = m_resumeItems.movieAt(index);
    qInfo() << "app: play resume item index=" << index
            << "type=" << item.itemType << "title=" << item.title
            << "resumeTicks=" << item.resumeTicks;
    if (item.id.isEmpty())
        return;
    playMediaItem(item);
}

void AppController::playNextUpItem(int index)
{
    const auto item = m_nextUpItems.movieAt(index);
    qInfo() << "app: play next-up item index=" << index
            << "type=" << item.itemType << "title=" << item.title
            << "resumeTicks=" << item.resumeTicks;
    if (item.id.isEmpty())
        return;
    playMediaItem(item);
}

void AppController::playLatestItem(int index)
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
    playMediaItem(item);
}

void AppController::playMediaItem(const MovieItem &item)
{
    Diagnostics::Task task(QStringLiteral("playback_negotiate"), {{QStringLiteral("itemId"), item.id}, {QStringLiteral("title"), item.title}, {QStringLiteral("type"), item.itemType}});
    setBusy(true, QStringLiteral("Negotiating direct play…"));
    const QString itemId = item.id;
    QCoro::runDetached(
        m_api->negotiateDirectPlay(item),
        [this, itemId](const PlaybackSession &session) {
            // Fire-and-forget: fetch media segments in parallel; pass to the
            // player once they arrive (skip-intro/outro support).
            PlaybackSession enriched = session;
            JellyfinApiFacade *api = m_api;
            PlayerController *player = m_player;
            QCoro::runDetached(
                api->fetchMediaSegments(itemId),
                [player, enriched](const std::vector<MediaSegment> &segments) mutable {
                    enriched.segments = segments;
                    player->play(enriched);
                },
                [player, enriched](const std::exception_ptr &) {
                    // No segments available — start playback regardless.
                    player->play(enriched);
                });
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
            openSeries({m_currentSeriesId, m_currentSeriesName, {}, {}, {}, QStringLiteral("Series"), {}, {}, {}, 0, 0, 0, 0, false});
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
    const QString normalized = mode == QStringLiteral("starfish") ? QStringLiteral("starfish") : QStringLiteral("alsa");
    if (m_audioOutputMode == normalized)
        return;
    qInfo() << "app: audio output mode changed" << m_audioOutputMode << "->" << normalized;
    m_audioOutputMode = normalized;
    m_database->saveAudioOutputMode(normalized);
    m_player->setAudioOutputMode(normalized);
    emit audioOutputModeChanged();
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

void AppController::applyLibrariesCache()
{
    const auto libraries = m_database->loadLibraries();
    std::vector<LibraryItem> parsed;
    parsed.reserve(libraries.size());
    for (const auto &value : libraries)
        parsed.push_back(libraryFromJson(value.toObject()));
    m_libraries.setLibraries(parsed);
}

void AppController::applyMoviesCache(const QString &libraryId)
{
    const auto movies = m_database->loadMovies(libraryId);
    std::vector<MovieItem> parsed;
    parsed.reserve(movies.size());
    for (const auto &value : movies)
        parsed.push_back(movieFromJson(value.toObject()));
    m_movies.setMovies(parsed);
    prefetchMoviePosters(parsed);
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
            m_database->saveLibraries(toJsonArray(libraries));
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
    m_movies.setMovies(items);
    if (!cacheKey.isEmpty())
        m_database->saveMovies(cacheKey, toJsonArray(items));
    prefetchMoviePosters(items);
    setBusy(false);
    setPage(QStringLiteral("movies"));
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
    applyMoviesCache(QStringLiteral("seasons/%1").arg(series.id));
    setBusy(true, QStringLiteral("Loading seasons…"));

    QCoro::runDetached(
        m_api->fetchSeasons(series.id),
        [this, series, loadGeneration](const std::vector<MovieItem> &seasons) {
            if (loadGeneration != m_libraryLoadGeneration)
                return;
            if (seasons.empty()) {
                openSeason({series.id, series.title, {}, {}, {}, QStringLiteral("Series"),
                            series.id, {}, 0, 0, 0, false});
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
    if (seriesId.isEmpty())
        return;

    const int loadGeneration = ++m_libraryLoadGeneration;
    m_currentSeriesId = seriesId;
    m_currentSeasonId = season.itemType == QStringLiteral("Season") ? season.id : QString();
    m_currentViewKind = QStringLiteral("episodes");
    m_currentLibraryName = season.title;
    m_currentContentLabel = QStringLiteral("Episodes");
    emit currentLibraryNameChanged();
    applyMoviesCache(QStringLiteral("episodes/%1/%2").arg(seriesId, season.id));
    setBusy(true, QStringLiteral("Loading episodes…"));

    QCoro::runDetached(
        m_api->fetchEpisodes(seriesId, season.itemType == QStringLiteral("Season") ? season.id : QString()),
        [this, seriesId, season, loadGeneration](const std::vector<MovieItem> &episodes) {
            if (loadGeneration != m_libraryLoadGeneration)
                return;
            setCurrentItems(episodes, QStringLiteral("episodes/%1/%2").arg(seriesId, season.id));
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
    m_homeLoadsPending = 3;
    m_libraryPrefetchTimer.stop();

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

    QCoro::runDetached(
        m_api->fetchLatestItems(),
        [this, generation](const std::vector<MovieItem> &items) {
            if (generation != m_homeLoadGeneration)
                return;
            qInfo() << "home: latest items" << items.size() << homeItemSample(items);
            m_latestItems.setMovies(items);
            prefetchMoviePosters(items);
            handleHomeRowLoaded(generation);
        },
        [this, generation](const std::exception_ptr &error) {
            if (generation != m_homeLoadGeneration)
                return;
            qWarning() << "home: latest fetch failed" << exceptionMessage(error);
            handleHomeRowLoaded(generation);
        });
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

    if (viewKind == QStringLiteral("movies") && !libraryId.isEmpty()) {
        const int loadGeneration = ++m_libraryLoadGeneration;
        QCoro::runDetached(
            m_api->fetchMovies(libraryId),
            [this, loadGeneration, libraryId](const std::vector<MovieItem> &movies) {
                if (loadGeneration != m_libraryLoadGeneration)
                    return;
                m_movies.setMovies(movies);
                m_database->saveMovies(libraryId, toJsonArray(movies));
                prefetchMoviePosters(movies);
            },
            [this, loadGeneration](const std::exception_ptr &error) {
                if (loadGeneration != m_libraryLoadGeneration)
                    return;
                qWarning() << "app: post-playback movie refresh failed" << exceptionMessage(error);
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
                m_database->saveMovies(QStringLiteral("episodes/%1/%2").arg(seriesId, seasonId), toJsonArray(episodes));
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
}

void AppController::handleHomeRowLoaded(int generation)
{
    if (generation != m_homeLoadGeneration || m_homeLoadsPending <= 0)
        return;

    --m_homeLoadsPending;
    if (m_homeLoadsPending == 0)
        scheduleLibraryPrefetch(generation);
}

void AppController::scheduleLibraryPrefetch(int generation)
{
    if (generation != m_homeLoadGeneration || !m_api || m_api->session().accessToken.isEmpty())
        return;

    m_libraryPrefetchQueue.clear();
    const int count = m_libraries.rowCount();
    m_libraryPrefetchQueue.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        const auto library = m_libraries.libraryAt(i);
        if (library.id.isEmpty())
            continue;
        if (m_prefetchedLibraryKeys.contains(libraryCacheKey(library)))
            continue;
        m_libraryPrefetchQueue.push_back(library);
    }

    if (m_libraryPrefetchQueue.empty())
        return;

    m_libraryPrefetchGeneration = generation;
    m_libraryPrefetchIndex = 0;
    m_libraryPrefetchActive = false;
    qInfo() << "library prefetch: scheduled" << m_libraryPrefetchQueue.size()
            << "libraries after home load";
    m_libraryPrefetchTimer.start(500);
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
    const bool seriesLibrary = isSeriesLibrary(library);
    m_libraryPrefetchActive = true;
    qInfo() << "library prefetch: fetching" << library.name << cacheKey;

    const auto onDone = [this, cacheKey, library](const std::vector<MovieItem> &items) {
        if (m_libraryPrefetchGeneration != m_homeLoadGeneration)
            return;
        qInfo() << "library prefetch: cached" << library.name << items.size();
        m_database->saveMovies(cacheKey, toJsonArray(items));
        m_prefetchedLibraryKeys.insert(cacheKey);
        prefetchMoviePosters(items);
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

    if (seriesLibrary)
        QCoro::runDetached(m_api->fetchSeries(library.id), onDone, onError);
    else
        QCoro::runDetached(m_api->fetchMovies(library.id), onDone, onError);
}

void AppController::prefetchMoviePosters(const std::vector<MovieItem> &movies)
{
    QStringList urls;
    urls.reserve(std::min<size_t>(movies.size(), 24));

    for (const auto &movie : movies) {
        if (movie.posterUrl.isEmpty())
            continue;

        urls.push_back(movie.posterUrl);
        if (urls.size() >= 24)
            break;
    }

    if (!urls.isEmpty())
        m_api->prefetchImages(urls, 6);
}

} // namespace JellyfinNative
