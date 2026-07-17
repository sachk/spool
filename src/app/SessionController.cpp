#include "SessionController.h"

#include "../api/JellyfinApiFacade.h"
#include "../cache/DatabaseManager.h"
#include "../common/AsyncTask.h"

#include <QDebug>

namespace JellyfinNative {

SessionController::SessionController(DatabaseManager *database, JellyfinApiFacade *api, QObject *parent)
    : QObject(parent)
    , m_database(database)
    , m_api(api)
{
    connect(m_api, &JellyfinApiFacade::deviceProfileChanged, this, [this]() {
        if (!authenticated())
            return;
        Async::runScoped(
            this, m_api->postCapabilities(), []() {},
            [](const std::exception_ptr& error) {
                qWarning() << "session: updated capability report failed" << exceptionMessage(error);
            });
    });
}

QString SessionController::serverUrl() const
{
    return m_serverUrl;
}

QString SessionController::username() const
{
    return m_username;
}

QString SessionController::password() const
{
    return m_password;
}

bool SessionController::authenticated() const
{
    return !m_api->session().accessToken.isEmpty();
}

QCoro::Task<bool> SessionController::initializeAsync()
{
    const QVariantMap values = co_await m_database->loadValuesAsync(
        { QStringLiteral("login/serverUrl"), QStringLiteral("login/username"), QStringLiteral("login/accessToken"),
            QStringLiteral("login/userId"), QStringLiteral("login/userName"), QStringLiteral("login/serverId") });
    m_serverUrl = values.value(QStringLiteral("login/serverUrl")).toString();
    m_username = values.value(QStringLiteral("login/username")).toString();
    const AuthSession session { values.value(QStringLiteral("login/userId")).toString(),
        values.value(QStringLiteral("login/userName")).toString(),
        values.value(QStringLiteral("login/accessToken")).toString(),
        values.value(QStringLiteral("login/serverId")).toString() };
    if (m_username.isEmpty() && !session.userName.isEmpty())
        m_username = session.userName;

    emit serverUrlChanged();
    emit usernameChanged();

    if (session.accessToken.isEmpty() || m_serverUrl.isEmpty())
        co_return false;

    activateSession(session, false);
    co_return true;
}

void SessionController::setServerUrl(const QString& serverUrl)
{
    const QString normalized = serverUrl.trimmed();
    if (m_serverUrl == normalized)
        return;
    m_serverUrl = normalized;
    emit serverUrlChanged();
}

void SessionController::setUsername(const QString& username)
{
    if (m_username == username)
        return;
    m_username = username;
    emit usernameChanged();
}

void SessionController::setPassword(const QString& password)
{
    if (m_password == password)
        return;
    m_password = password;
    emit passwordChanged();
}

void SessionController::login()
{
    if (m_serverUrl.isEmpty() || m_username.isEmpty()) {
        emit errorOccurred(QStringLiteral("Server and username are required."));
        return;
    }

    emit busyChanged(true, QStringLiteral("Signing in…"));
    m_api->setServerUrl(m_serverUrl);
    Async::runScoped(
        this, m_api->authenticateByName(m_username, m_password),
        [this](const AuthSession& session) {
            emit busyChanged(false, {});
            activateSession(session, true);
        },
        [this](const std::exception_ptr& error) {
            emit busyChanged(false, {});
            emit errorOccurred(exceptionMessage(error));
        });
}

void SessionController::acceptSession(const AuthSession& session)
{
    activateSession(session, true);
}

void SessionController::logout()
{
    m_database->clearAuthSession();
    m_api->setSession({});
    if (!m_password.isEmpty()) {
        m_password.clear();
        emit passwordChanged();
    }
    emit authenticatedStateChanged();
    emit loggedOut();
}

void SessionController::expireSession(const QString& message)
{
    if (m_api->session().accessToken.isEmpty())
        return;
    qWarning() << "session: authentication expired";
    logout();
    emit errorOccurred(message);
}

bool SessionController::handleUnauthorized(const std::exception_ptr& error)
{
    const QString message = exceptionMessage(error);
    if (!message.contains(QStringLiteral("(401)"))
        && !message.contains(QStringLiteral("Unauthorized"), Qt::CaseInsensitive)) {
        return false;
    }

    if (!m_api->session().accessToken.isEmpty())
        expireSession(QStringLiteral("Your Jellyfin session has expired. Sign in again."));
    return true;
}

void SessionController::activateSession(const AuthSession& session, bool persist)
{
    if (session.accessToken.isEmpty()) {
        emit errorOccurred(QStringLiteral("The server returned an empty session."));
        return;
    }

    AuthSession activeSession = session;
    if (activeSession.userName.isEmpty())
        activeSession.userName = m_username;
    if (!activeSession.userName.isEmpty() && m_username != activeSession.userName) {
        m_username = activeSession.userName;
        emit usernameChanged();
    }

    m_api->setServerUrl(m_serverUrl);
    m_api->setSession(activeSession);
    if (persist) {
        m_database->saveLoginHints(m_serverUrl, m_username);
        m_database->saveAuthSession(activeSession);
    }

    if (m_username.isEmpty() && !activeSession.userId.isEmpty()) {
        Async::runScoped(
            this, m_api->fetchCurrentUserName(),
            [this](const QString& userName) {
                const QString normalized = userName.trimmed();
                if (normalized.isEmpty() || !m_username.isEmpty())
                    return;

                m_username = normalized;
                emit usernameChanged();

                AuthSession refreshed = m_api->session();
                refreshed.userName = normalized;
                m_api->setSession(refreshed);
                m_database->saveLoginHints(m_serverUrl, normalized);
                m_database->saveAuthSession(refreshed);
            },
            [](const std::exception_ptr& error) {
                qWarning() << "session: user name backfill failed" << exceptionMessage(error);
            });
    }

    emit authenticatedChanged(activeSession);
    emit authenticatedStateChanged();
    Async::runScoped(
        this, m_api->postCapabilities(), []() {},
        [](const std::exception_ptr& error) {
            qWarning() << "session: capability report failed" << exceptionMessage(error);
        });
}

} // namespace JellyfinNative
