#pragma once

#include "../common/JellyfinTypes.h"

#include <QCoroTask>
#include <QObject>
#include <QVariantList>

#include <vector>

#include <exception>

namespace JellyfinNative {

class DatabaseManager;
class JellyfinApiFacade;

class SessionController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(QString serverName READ serverName WRITE setServerName NOTIFY serverNameChanged)
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY usernameChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
    Q_PROPERTY(bool authenticated READ authenticated NOTIFY authenticatedStateChanged)
    // Synchronous launch hint: did the previous run end signed in? Lets
    // startup construct home optimistically before the async session restore
    // confirms; a wrong guess is corrected by the route reset that runs when
    // initialization completes.
    Q_PROPERTY(bool likelyAuthenticated READ likelyAuthenticated CONSTANT)
    Q_PROPERTY(QVariantList accountProfiles READ accountProfiles NOTIFY accountProfilesChanged)

public:
    SessionController(DatabaseManager *database, JellyfinApiFacade *api, QObject *parent = nullptr);

    QString serverUrl() const;
    QString serverName() const;
    QString username() const;
    QString password() const;
    bool authenticated() const;
    bool likelyAuthenticated() const;
    QVariantList accountProfiles() const;

    QCoro::Task<bool> initializeAsync();
    Q_INVOKABLE void setServerUrl(const QString& serverUrl);
    Q_INVOKABLE void setServerName(const QString& serverName);
    Q_INVOKABLE void setUsername(const QString& username);
    Q_INVOKABLE void setPassword(const QString& password);
    Q_INVOKABLE void login();
    Q_INVOKABLE bool activateProfile(const QString& profileId);
    Q_INVOKABLE void deactivate();
    void acceptSession(const AuthSession& session);
    Q_INVOKABLE void logout();
    void expireSession(const QString& message);
    bool handleUnauthorized(const std::exception_ptr& error);

signals:
    void serverUrlChanged();
    void serverNameChanged();
    void usernameChanged();
    void passwordChanged();
    void busyChanged(bool busy, const QString& text);
    void errorOccurred(const QString& message);
    void authenticatedChanged(const JellyfinNative::AuthSession& session);
    void authenticatedStateChanged();
    void accountProfilesChanged();
    void loggedOut();

private:
    struct AccountProfile {
        QString id;
        QString serverId;
        QString serverName;
        QString serverUrl;
        QString userId;
        QString userName;
        QString accessToken;
        qint64 lastUsedAt = 0;
    };

    void activateSession(const AuthSession& session, bool persist);
    void upsertActiveProfile(const AuthSession& session);
    void saveProfiles();
    void stampLikelyAuthenticated(bool value);

    DatabaseManager *m_database = nullptr;
    JellyfinApiFacade *m_api = nullptr;
    QString m_serverUrl;
    QString m_serverName;
    QString m_username;
    QString m_password;
    QString m_activeProfileId;
    std::vector<AccountProfile> m_profiles;
    bool m_likelyAuthenticated = false;
};

} // namespace JellyfinNative
