#pragma once

#include "../common/JellyfinTypes.h"

#include <QObject>

#include <exception>

namespace JellyfinNative {

class DatabaseManager;
class JellyfinApiFacade;

class SessionController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
  Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY usernameChanged)
  Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
  Q_PROPERTY(bool authenticated READ authenticated NOTIFY authenticatedStateChanged)

public:
  SessionController(DatabaseManager *database, JellyfinApiFacade *api,
                    QObject *parent = nullptr);

  QString serverUrl() const;
  QString username() const;
  QString password() const;
  bool authenticated() const;

  bool initialize();
  Q_INVOKABLE void setServerUrl(const QString &serverUrl);
  Q_INVOKABLE void setUsername(const QString &username);
  Q_INVOKABLE void setPassword(const QString &password);
  Q_INVOKABLE void login();
  void acceptSession(const AuthSession &session);
  Q_INVOKABLE void logout();
  void expireSession(const QString &message);
  bool handleUnauthorized(const std::exception_ptr &error);

signals:
  void serverUrlChanged();
  void usernameChanged();
  void passwordChanged();
  void busyChanged(bool busy, const QString &text);
  void errorOccurred(const QString &message);
  void authenticatedChanged(const JellyfinNative::AuthSession &session);
  void authenticatedStateChanged();
  void loggedOut();

private:
  void activateSession(const AuthSession &session, bool persist);

  DatabaseManager *m_database = nullptr;
  JellyfinApiFacade *m_api = nullptr;
  QString m_serverUrl;
  QString m_username;
  QString m_password;
};

} // namespace JellyfinNative
