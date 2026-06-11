#include "SessionController.h"

#include "../api/JellyfinApiFacade.h"
#include "../cache/DatabaseManager.h"
#include "../common/AsyncTask.h"

#include <QDebug>

namespace JellyfinNative {

SessionController::SessionController(DatabaseManager *database,
                                     JellyfinApiFacade *api, QObject *parent)
    : QObject(parent), m_database(database), m_api(api) {}

QString SessionController::serverUrl() const { return m_serverUrl; }

QString SessionController::username() const { return m_username; }

QString SessionController::password() const { return m_password; }

bool SessionController::authenticated() const {
  return !m_api->session().accessToken.isEmpty();
}

bool SessionController::initialize() {
  m_serverUrl = m_database->loadLastServerUrl();
  m_username = m_database->loadLastUsername();
  emit serverUrlChanged();
  emit usernameChanged();

  const AuthSession session = m_database->loadAuthSession();
  if (session.accessToken.isEmpty() || m_serverUrl.isEmpty())
    return false;

  activateSession(session, false);
  return true;
}

void SessionController::setServerUrl(const QString &serverUrl) {
  const QString normalized = serverUrl.trimmed();
  if (m_serverUrl == normalized)
    return;
  m_serverUrl = normalized;
  emit serverUrlChanged();
}

void SessionController::setUsername(const QString &username) {
  if (m_username == username)
    return;
  m_username = username;
  emit usernameChanged();
}

void SessionController::setPassword(const QString &password) {
  if (m_password == password)
    return;
  m_password = password;
  emit passwordChanged();
}

void SessionController::login() {
  if (m_serverUrl.isEmpty() || m_username.isEmpty()) {
    emit errorOccurred(QStringLiteral("Server and username are required."));
    return;
  }

  emit busyChanged(true, QStringLiteral("Signing in…"));
  m_api->setServerUrl(m_serverUrl);
  Async::runScoped(
      this, m_api->authenticateByName(m_username, m_password),
      [this](const AuthSession &session) {
        emit busyChanged(false, {});
        activateSession(session, true);
      },
      [this](const std::exception_ptr &error) {
        emit busyChanged(false, {});
        emit errorOccurred(exceptionMessage(error));
      });
}

void SessionController::acceptSession(const AuthSession &session) {
  activateSession(session, true);
}

void SessionController::logout() {
  m_database->clearAuthSession();
  m_api->setSession({});
  if (!m_password.isEmpty()) {
    m_password.clear();
    emit passwordChanged();
  }
  emit loggedOut();
}

void SessionController::expireSession(const QString &message) {
  if (m_api->session().accessToken.isEmpty())
    return;
  qWarning() << "session: authentication expired";
  logout();
  emit errorOccurred(message);
}

bool SessionController::handleUnauthorized(const std::exception_ptr &error) {
  const QString message = exceptionMessage(error);
  if (!message.contains(QStringLiteral("(401)")) &&
      !message.contains(QStringLiteral("Unauthorized"), Qt::CaseInsensitive)) {
    return false;
  }

  if (!m_api->session().accessToken.isEmpty())
    expireSession(QStringLiteral("Your Jellyfin session has expired. Sign in again."));
  return true;
}

void SessionController::activateSession(const AuthSession &session,
                                        bool persist) {
  if (session.accessToken.isEmpty()) {
    emit errorOccurred(QStringLiteral("The server returned an empty session."));
    return;
  }

  m_api->setServerUrl(m_serverUrl);
  m_api->setSession(session);
  if (persist) {
    m_database->saveLoginHints(m_serverUrl, m_username);
    m_database->saveAuthSession(session);
  }

  emit authenticatedChanged(session);
  Async::runScoped(
      this, m_api->postCapabilities(), []() {},
      [](const std::exception_ptr &error) {
        qWarning() << "session: capability report failed"
                   << exceptionMessage(error);
      });
}

} // namespace JellyfinNative
