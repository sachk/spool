#include "QuickConnectController.h"

#include "../api/JellyfinApiFacade.h"
#include "../common/AsyncTask.h"

#include <QDebug>
#include <QJsonObject>

namespace JellyfinNative {

namespace {

    constexpr int kPollIntervalMs = 5000;
    constexpr int kMaxPollAttempts = 36;
    constexpr int kMaxPollErrors = 6;

} // namespace

QuickConnectController::QuickConnectController(JellyfinApiFacade *api, QObject *parent)
    : QObject(parent)
    , m_api(api)
{
    m_pollTimer.setInterval(kPollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &QuickConnectController::poll);
}

QString QuickConnectController::code() const
{
    return m_code;
}

QString QuickConnectController::status() const
{
    return m_status;
}

bool QuickConnectController::active() const
{
    return !m_secret.isEmpty();
}

void QuickConnectController::start(const QString& serverUrl)
{
    if (serverUrl.isEmpty()) {
        emit errorOccurred(QStringLiteral("Enter a Jellyfin server URL first."));
        return;
    }

    cancel();
    const quint64 generation = m_generation;
    emit busyChanged(true, QStringLiteral("Starting Quick Connect…"));
    m_api->setServerUrl(serverUrl);
    qInfo() << "quick connect: starting for" << serverUrl;

    Async::runScoped(
        this, m_api->quickConnectEnabled(),
        [this, generation](bool enabled) {
            if (generation != m_generation)
                return;
            if (!enabled) {
                emit busyChanged(false, {});
                emit errorOccurred(QStringLiteral("Quick Connect is disabled on this server."));
                return;
            }

            Async::runScoped(
                this, m_api->initiateQuickConnect(),
                [this, generation](const QJsonObject& result) {
                    if (generation != m_generation)
                        return;
                    emit busyChanged(false, {});
                    m_code = result.value(QStringLiteral("Code")).toString();
                    m_secret = result.value(QStringLiteral("Secret")).toString();
                    m_status = QStringLiteral("Waiting for authorization…");
                    m_pollAttempts = 0;
                    m_pollErrors = 0;
                    qInfo() << "quick connect: initiated code" << m_code << "deviceId"
                            << result.value(QStringLiteral("DeviceId")).toString();
                    emit changed();
                    poll();
                    m_pollTimer.start();
                },
                [this, generation](const std::exception_ptr& error) {
                    if (generation != m_generation)
                        return;
                    emit busyChanged(false, {});
                    const QString message = exceptionMessage(error);
                    qWarning() << "quick connect: initiate failed" << message;
                    emit errorOccurred(message);
                });
        },
        [this, generation](const std::exception_ptr& error) {
            if (generation != m_generation)
                return;
            emit busyChanged(false, {});
            const QString message = exceptionMessage(error);
            qWarning() << "quick connect: enabled check failed" << message;
            emit errorOccurred(message);
        });
}

void QuickConnectController::cancel()
{
    ++m_generation;
    m_pollTimer.stop();
    m_code.clear();
    m_status.clear();
    m_secret.clear();
    m_pollAttempts = 0;
    m_pollErrors = 0;
    emit changed();
}

void QuickConnectController::poll()
{
    if (m_secret.isEmpty())
        return;

    ++m_pollAttempts;
    if (m_pollAttempts > kMaxPollAttempts) {
        qWarning() << "quick connect: timed out after polls" << m_pollAttempts;
        cancel();
        emit errorOccurred(QStringLiteral("Quick Connect timed out."));
        return;
    }

    const QString secret = m_secret;
    Async::runScoped(
        this, m_api->pollQuickConnect(secret),
        [this, secret](const QJsonObject& result) {
            if (secret != m_secret)
                return;

            m_pollErrors = 0;
            const bool isAuthenticated = result.value(QStringLiteral("Authenticated")).toBool();
            qInfo() << "quick connect: poll" << m_pollAttempts << "authenticated" << isAuthenticated;
            if (!isAuthenticated)
                return;

            m_pollTimer.stop();
            m_status = QStringLiteral("Authorized. Signing in…");
            emit changed();

            Async::runScoped(
                this, m_api->authenticateWithQuickConnect(secret),
                [this, secret](const AuthSession& session) {
                    if (secret != m_secret)
                        return;
                    qInfo() << "quick connect: authenticated successfully";
                    cancel();
                    emit authenticated(session);
                },
                [this, secret](const std::exception_ptr& error) {
                    if (secret != m_secret)
                        return;
                    const QString message = exceptionMessage(error);
                    qWarning() << "quick connect: token exchange failed" << message;
                    cancel();
                    emit errorOccurred(message);
                });
        },
        [this, secret](const std::exception_ptr& error) {
            if (secret != m_secret)
                return;

            const QString message = exceptionMessage(error);
            ++m_pollErrors;
            qWarning() << "quick connect: poll failed" << m_pollErrors << message;

            if (message.contains(QStringLiteral("(401)")) || message.contains(QStringLiteral("(404)"))
                || m_pollErrors >= kMaxPollErrors) {
                cancel();
                emit errorOccurred(message);
                return;
            }

            if (!m_secret.isEmpty()) {
                m_status = QStringLiteral("Waiting for authorization…");
                emit changed();
            }
        });
}

} // namespace JellyfinNative
