#pragma once

#include "../common/JellyfinTypes.h"

#include <QObject>

#include <exception>

namespace JellyfinNative {

class DatabaseManager;
class JellyfinApiFacade;

class SessionController final : public QObject {
  Q_OBJECT

public:
  SessionController(DatabaseManager *database, JellyfinApiFacade *api,
                    QObject *parent = nullptr);

  QString serverUrl() const;
  QString username() const;
  QString password() const;
  bool authenticated() const;

  bool initialize();
  void setServerUrl(const QString &serverUrl);
  void setUsername(const QString &username);
  void setPassword(const QString &password);
  void login();
  void acceptSession(const AuthSession &session);
  void logout();
  bool handleUnauthorized(const std::exception_ptr &error);

signals:
  void serverUrlChanged();
  void usernameChanged();
  void passwordChanged();
  void busyChanged(bool busy, const QString &text);
  void errorOccurred(const QString &message);
  void authenticatedChanged(const JellyfinNative::AuthSession &session);
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
