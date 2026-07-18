#include "SessionController.h"

#include "../api/JellyfinApiFacade.h"
#include "../cache/DatabaseManager.h"
#include "../common/AsyncTask.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUrl>

#include <algorithm>

namespace JellyfinNative {

namespace {
    constexpr auto kLikelyAuthenticatedSetting = "session/likelyAuthenticated";
    constexpr auto kProfilesSetting = "profiles/accounts-v1";

    QString normalizedServerUrl(const QString& input)
    {
        QString value = input.trimmed();
        while (value.endsWith(QLatin1Char('/')))
            value.chop(1);
        return value;
    }

    QString profileId(const QString& serverId, const QString& serverUrl, const QString& userId)
    {
        const QByteArray identity = (serverId.isEmpty() ? normalizedServerUrl(serverUrl).toLower() : serverId).toUtf8()
            + '\0' + userId.toUtf8();
        return QString::fromLatin1(QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex().left(24));
    }
}

SessionController::SessionController(DatabaseManager *database, JellyfinApiFacade *api, QObject *parent)
    : QObject(parent)
    , m_database(database)
    , m_api(api)
    , m_likelyAuthenticated(false)
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

bool SessionController::likelyAuthenticated() const
{
    return m_likelyAuthenticated;
}

void SessionController::stampLikelyAuthenticated(bool value)
{
    m_likelyAuthenticated = value;
    QSettings().setValue(QLatin1String(kLikelyAuthenticatedSetting), value);
}

QString SessionController::serverUrl() const
{
    return m_serverUrl;
}

QString SessionController::serverName() const
{
    return m_serverName;
}

QVariantList SessionController::accountProfiles() const
{
    QVariantList result;
    result.reserve(static_cast<qsizetype>(m_profiles.size()));
    for (const AccountProfile& profile : m_profiles) {
        result.push_back(
            QVariantMap { { QStringLiteral("id"), profile.id }, { QStringLiteral("serverName"), profile.serverName },
                { QStringLiteral("serverUrl"), profile.serverUrl }, { QStringLiteral("userName"), profile.userName },
                { QStringLiteral("needsSignIn"), profile.accessToken.isEmpty() } });
    }
    return result;
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
    const QVariantMap values = co_await m_database->loadValuesAsync({ QStringLiteral("login/serverUrl"),
        QStringLiteral("login/username"), QStringLiteral("login/accessToken"), QStringLiteral("login/userId"),
        QStringLiteral("login/userName"), QStringLiteral("login/serverId"), QLatin1String(kProfilesSetting) });
    m_serverUrl = values.value(QStringLiteral("login/serverUrl")).toString();
    m_username = values.value(QStringLiteral("login/username")).toString();
    const AuthSession session { values.value(QStringLiteral("login/userId")).toString(),
        values.value(QStringLiteral("login/userName")).toString(),
        values.value(QStringLiteral("login/accessToken")).toString(),
        values.value(QStringLiteral("login/serverId")).toString() };
    if (m_username.isEmpty() && !session.userName.isEmpty())
        m_username = session.userName;

    const QJsonArray encodedProfiles
        = QJsonDocument::fromJson(values.value(QLatin1String(kProfilesSetting)).toByteArray()).array();
    for (const QJsonValue& value : encodedProfiles) {
        const QJsonObject object = value.toObject();
        AccountProfile profile { object.value(QStringLiteral("id")).toString(),
            object.value(QStringLiteral("serverId")).toString(), object.value(QStringLiteral("serverName")).toString(),
            normalizedServerUrl(object.value(QStringLiteral("serverUrl")).toString()),
            object.value(QStringLiteral("userId")).toString(), object.value(QStringLiteral("userName")).toString(),
            object.value(QStringLiteral("accessToken")).toString(),
            static_cast<qint64>(object.value(QStringLiteral("lastUsedAt")).toDouble()) };
        if (!profile.serverUrl.isEmpty() && !profile.userId.isEmpty()) {
            if (profile.id.isEmpty())
                profile.id = profileId(profile.serverId, profile.serverUrl, profile.userId);
            if (profile.serverName.isEmpty())
                profile.serverName = QStringLiteral("Jellyfin Server");
            m_profiles.push_back(std::move(profile));
        }
    }

    // Adopt the pre-profile session once so an installed development build does
    // not lose its account. All future reads use the paired profile document.
    if (m_profiles.empty() && !session.accessToken.isEmpty() && !m_serverUrl.isEmpty() && !session.userId.isEmpty()) {
        m_serverName = QStringLiteral("Jellyfin Server");
        upsertActiveProfile(session);
    }

    stampLikelyAuthenticated(false);
    emit serverUrlChanged();
    emit serverNameChanged();
    emit usernameChanged();
    emit accountProfilesChanged();
    if (m_profiles.size() == 1 && !m_profiles.front().accessToken.isEmpty())
        activateProfile(m_profiles.front().id);
    co_return !m_profiles.empty();
}

void SessionController::setServerUrl(const QString& serverUrl)
{
    const QString normalized = normalizedServerUrl(serverUrl);
    if (m_serverUrl == normalized)
        return;
    m_serverUrl = normalized;
    emit serverUrlChanged();
}

void SessionController::setServerName(const QString& serverName)
{
    const QString normalized = serverName.trimmed();
    if (m_serverName == normalized)
        return;
    m_serverName = normalized;
    emit serverNameChanged();
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

bool SessionController::activateProfile(const QString& profileIdValue)
{
    const auto it = std::find_if(m_profiles.cbegin(), m_profiles.cend(),
        [&profileIdValue](const AccountProfile& profile) { return profile.id == profileIdValue; });
    if (it == m_profiles.cend()) {
        emit errorOccurred(QStringLiteral("That saved account is no longer available."));
        return false;
    }
    if (it->accessToken.isEmpty()) {
        setServerUrl(it->serverUrl);
        setServerName(it->serverName);
        setUsername(it->userName);
        emit errorOccurred(QStringLiteral("This account needs to sign in again."));
        return false;
    }

    setServerUrl(it->serverUrl);
    setServerName(it->serverName);
    setUsername(it->userName);
    m_activeProfileId = it->id;
    activateSession({ it->userId, it->userName, it->accessToken, it->serverId }, false);
    return true;
}

void SessionController::deactivate()
{
    stampLikelyAuthenticated(false);
    m_activeProfileId.clear();
    m_api->setSession({});
    if (!m_password.isEmpty()) {
        m_password.clear();
        emit passwordChanged();
    }
    emit authenticatedStateChanged();
    emit loggedOut();
}

void SessionController::acceptSession(const AuthSession& session)
{
    activateSession(session, true);
}

void SessionController::logout()
{
    stampLikelyAuthenticated(false);
    if (!m_activeProfileId.isEmpty()) {
        std::erase_if(m_profiles, [this](const AccountProfile& profile) { return profile.id == m_activeProfileId; });
        saveProfiles();
        emit accountProfilesChanged();
    }
    m_activeProfileId.clear();
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
    stampLikelyAuthenticated(true);
    if (persist) {
        m_database->saveLoginHints(m_serverUrl, m_username);
        m_database->saveAuthSession(activeSession);
        upsertActiveProfile(activeSession);
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
                upsertActiveProfile(refreshed);
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

void SessionController::upsertActiveProfile(const AuthSession& session)
{
    const QString id = profileId(session.serverId, m_serverUrl, session.userId);
    AccountProfile profile { id, session.serverId,
        m_serverName.isEmpty() ? QStringLiteral("Jellyfin Server") : m_serverName, normalizedServerUrl(m_serverUrl),
        session.userId, session.userName.isEmpty() ? m_username : session.userName, session.accessToken,
        QDateTime::currentMSecsSinceEpoch() };
    const auto it = std::find_if(
        m_profiles.begin(), m_profiles.end(), [&id](const AccountProfile& candidate) { return candidate.id == id; });
    if (it == m_profiles.end())
        m_profiles.push_back(std::move(profile));
    else
        *it = std::move(profile);
    std::sort(m_profiles.begin(), m_profiles.end(),
        [](const AccountProfile& left, const AccountProfile& right) { return left.lastUsedAt > right.lastUsedAt; });
    m_activeProfileId = id;
    saveProfiles();
    emit accountProfilesChanged();
}

void SessionController::saveProfiles()
{
    QJsonArray encoded;
    for (const AccountProfile& profile : m_profiles) {
        encoded.push_back(QJsonObject { { QStringLiteral("id"), profile.id },
            { QStringLiteral("serverId"), profile.serverId }, { QStringLiteral("serverName"), profile.serverName },
            { QStringLiteral("serverUrl"), profile.serverUrl }, { QStringLiteral("userId"), profile.userId },
            { QStringLiteral("userName"), profile.userName }, { QStringLiteral("accessToken"), profile.accessToken },
            { QStringLiteral("lastUsedAt"), profile.lastUsedAt } });
    }
    m_database->saveSetting(
        QLatin1String(kProfilesSetting), QString::fromUtf8(QJsonDocument(encoded).toJson(QJsonDocument::Compact)));
}

} // namespace JellyfinNative
