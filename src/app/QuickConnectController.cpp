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
                    emit changed();
                    poll();
                    m_pollTimer.start();
                },
                [this, generation](const std::exception_ptr& error) {
                    if (generation != m_generation)
                        return;
                    emit busyChanged(false, {});
                    const QString message = exceptionMessage(error);
                    qWarning() << "quick connect: initiate failed";
                    emit errorOccurred(message);
                });
        },
        [this, generation](const std::exception_ptr& error) {
            if (generation != m_generation)
                return;
            emit busyChanged(false, {});
            const QString message = exceptionMessage(error);
            qWarning() << "quick connect: enabled check failed";
            emit errorOccurred(message);
        });
}

void QuickConnectController::cancel()
{
    ++m_generation;
    m_pollTimer.stop();
    m_code.fill(QLatin1Char('\0'));
    m_code.clear();
    m_status.clear();
    m_secret.fill(QLatin1Char('\0'));
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

    const quint64 generation = m_generation;
    Async::runScoped(
        this, m_api->pollQuickConnect(m_secret),
        [this, generation](const QJsonObject& result) {
            if (generation != m_generation)
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
                this, m_api->authenticateWithQuickConnect(m_secret),
                [this, generation](const AuthSession& session) {
                    if (generation != m_generation)
                        return;
                    qInfo() << "quick connect: authenticated successfully";
                    cancel();
                    emit authenticated(session);
                },
                [this, generation](const std::exception_ptr& error) {
                    if (generation != m_generation)
                        return;
                    const QString message = exceptionMessage(error);
                    qWarning() << "quick connect: token exchange failed";
                    cancel();
                    emit errorOccurred(message);
                });
        },
        [this, generation](const std::exception_ptr& error) {
            if (generation != m_generation)
                return;

            const QString message = exceptionMessage(error);
            ++m_pollErrors;
            qWarning() << "quick connect: poll failed" << m_pollErrors;

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
