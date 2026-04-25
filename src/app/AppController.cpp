#include "AppController.h"

#include "../api/JellyfinApiFacade.h"
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

    connect(m_player, &PlayerController::playbackStopped, this, [this]() {
        if (m_page != QStringLiteral("movies"))
            setPage(QStringLiteral("movies"));
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

PlayerController *AppController::player()
{
    return m_player;
}

void AppController::initialize()
{
    m_serverUrl = m_database->loadLastServerUrl();
    m_username = m_database->loadLastUsername();
    emit serverUrlChanged();
    emit usernameChanged();

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

void AppController::openLibrary(int index)
{
    const auto library = m_libraries.libraryAt(index);
    if (library.id.isEmpty())
        return;

    m_currentLibraryId = library.id;
    m_currentLibraryName = library.name;
    emit currentLibraryNameChanged();
    applyMoviesCache(library.id);
    setBusy(true, QStringLiteral("Loading movies…"));

    QCoro::runDetached(
        m_api->fetchMovies(library.id),
        [this, library](const std::vector<MovieItem> &movies) {
            m_movies.setMovies(movies);
            m_database->saveMovies(library.id, toJsonArray(movies));
            prefetchMoviePosters(movies);
            setBusy(false);
            setPage(QStringLiteral("movies"));
        },
        [this](const std::exception_ptr &error) {
            setBusy(false);
            setErrorText(exceptionMessage(error));
            if (m_page != QStringLiteral("movies"))
                setPage(QStringLiteral("movies"));
        });
}

void AppController::playMovie(int index)
{
    const auto movie = m_movies.movieAt(index);
    if (movie.id.isEmpty())
        return;

    setBusy(true, QStringLiteral("Negotiating direct play…"));
    QCoro::runDetached(
        m_api->negotiateDirectPlay(movie),
        [this](const PlaybackSession &session) {
            setBusy(false);
            m_player->play(session);
        },
        [this](const std::exception_ptr &error) {
            setBusy(false);
            setErrorText(exceptionMessage(error));
        });
}

void AppController::back()
{
    if (m_player->visible()) {
        m_player->stop();
        return;
    }

    if (m_page == QStringLiteral("movies")) {
        setPage(QStringLiteral("libraries"));
        return;
    }

    if (m_page == QStringLiteral("libraries")) {
        setPage(QStringLiteral("login"));
        m_discovery->start();
        return;
    }

    QCoreApplication::quit();
}

void AppController::clearError()
{
    setErrorText({});
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
    setBusy(true, QStringLiteral("Loading libraries…"));
    QCoro::runDetached(
        m_api->fetchLibraries(),
        [this](const std::vector<LibraryItem> &libraries) {
            m_libraries.setLibraries(libraries);
            m_database->saveLibraries(toJsonArray(libraries));
            setBusy(false);
            setPage(QStringLiteral("libraries"));
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
