#include "DatabaseManager.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMetaObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace JellyfinNative {

class DatabaseWorker final : public QObject
{
    Q_OBJECT

public:
    bool initialize(const QString &databasePath)
    {
        const QString connectionName = QStringLiteral("jellyfin_native_cache");
        if (QSqlDatabase::contains(connectionName))
            m_database = QSqlDatabase::database(connectionName);
        else
            m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);

        m_database.setDatabaseName(databasePath);
        if (!m_database.open())
            return false;

        QSqlQuery query(m_database);
        return query.exec(QStringLiteral(
                   "CREATE TABLE IF NOT EXISTS kv ("
                   "key TEXT PRIMARY KEY,"
                   "value TEXT NOT NULL"
                   ")"));
    }

    QVariant value(const QString &key)
    {
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral("SELECT value FROM kv WHERE key = ?"));
        query.addBindValue(key);
        if (!query.exec() || !query.next())
            return {};
        return query.value(0);
    }

    void setValue(const QString &key, const QVariant &value)
    {
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral(
            "INSERT INTO kv(key, value) VALUES(?, ?) "
            "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
        query.addBindValue(key);
        query.addBindValue(value);
        query.exec();
    }

    void close()
    {
        if (m_database.isValid()) {
            const QString connectionName = m_database.connectionName();
            m_database.close();
            m_database = {};
            QSqlDatabase::removeDatabase(connectionName);
        }
    }

private:
    QSqlDatabase m_database;
};

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
{
}

DatabaseManager::~DatabaseManager()
{
    shutdown();
}

bool DatabaseManager::initialize(const QString &databasePath)
{
    if (m_worker)
        return true;

    QDir().mkpath(QFileInfo(databasePath).absolutePath());

    m_worker = new DatabaseWorker();
    m_worker->moveToThread(&m_thread);
    m_thread.start();

    bool success = false;
    QMetaObject::invokeMethod(
        m_worker,
        [&]() {
            success = m_worker->initialize(databasePath);
        },
        Qt::BlockingQueuedConnection);
    return success;
}

void DatabaseManager::shutdown()
{
    if (!m_worker)
        return;

    QMetaObject::invokeMethod(m_worker, [this]() { m_worker->close(); }, Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
    delete m_worker;
    m_worker = nullptr;
}

QVariant DatabaseManager::invokeOnWorker(const std::function<QVariant()> &callback)
{
    QVariant result;
    QMetaObject::invokeMethod(
        m_worker,
        [&]() {
            result = callback();
        },
        Qt::BlockingQueuedConnection);
    return result;
}

void DatabaseManager::invokeOnWorkerAsync(const std::function<void()> &callback)
{
    QMetaObject::invokeMethod(m_worker, callback, Qt::QueuedConnection);
}

QString DatabaseManager::loadLastServerUrl()
{
    return invokeOnWorker([this]() { return m_worker->value(QStringLiteral("login/serverUrl")); }).toString();
}

QString DatabaseManager::loadLastUsername()
{
    return invokeOnWorker([this]() { return m_worker->value(QStringLiteral("login/username")); }).toString();
}

void DatabaseManager::saveLoginHints(const QString &serverUrl, const QString &username)
{
    invokeOnWorkerAsync([this, serverUrl, username]() {
        m_worker->setValue(QStringLiteral("login/serverUrl"), serverUrl);
        m_worker->setValue(QStringLiteral("login/username"), username);
    });
}

AuthSession DatabaseManager::loadAuthSession()
{
    AuthSession session;
    session.accessToken = invokeOnWorker([this]() { return m_worker->value(QStringLiteral("login/accessToken")); }).toString();
    session.userId = invokeOnWorker([this]() { return m_worker->value(QStringLiteral("login/userId")); }).toString();
    session.serverId = invokeOnWorker([this]() { return m_worker->value(QStringLiteral("login/serverId")); }).toString();
    return session;
}

void DatabaseManager::saveAuthSession(const AuthSession &session)
{
    invokeOnWorkerAsync([this, session]() {
        m_worker->setValue(QStringLiteral("login/accessToken"), session.accessToken);
        m_worker->setValue(QStringLiteral("login/userId"), session.userId);
        m_worker->setValue(QStringLiteral("login/serverId"), session.serverId);
    });
}

void DatabaseManager::clearAuthSession()
{
    invokeOnWorkerAsync([this]() {
        m_worker->setValue(QStringLiteral("login/accessToken"), QString());
        m_worker->setValue(QStringLiteral("login/userId"), QString());
        m_worker->setValue(QStringLiteral("login/serverId"), QString());
    });
}

QString DatabaseManager::loadDeviceId()
{
    return invokeOnWorker([this]() { return m_worker->value(QStringLiteral("client/deviceId")); }).toString();
}

void DatabaseManager::saveDeviceId(const QString &deviceId)
{
    invokeOnWorkerAsync([this, deviceId]() {
        m_worker->setValue(QStringLiteral("client/deviceId"), deviceId);
    });
}

QJsonArray DatabaseManager::loadDiscoveredServers()
{
    const auto value = invokeOnWorker([this]() { return m_worker->value(QStringLiteral("cache/discoveredServers")); });
    return QJsonDocument::fromJson(value.toByteArray()).array();
}

void DatabaseManager::saveDiscoveredServers(const QJsonArray &servers)
{
    const QByteArray encoded = QJsonDocument(servers).toJson(QJsonDocument::Compact);
    invokeOnWorkerAsync([this, encoded]() {
        m_worker->setValue(QStringLiteral("cache/discoveredServers"), QString::fromUtf8(encoded));
    });
}

QJsonArray DatabaseManager::loadLibraries()
{
    const auto value = invokeOnWorker([this]() { return m_worker->value(QStringLiteral("cache/libraries")); });
    return QJsonDocument::fromJson(value.toByteArray()).array();
}

void DatabaseManager::saveLibraries(const QJsonArray &libraries)
{
    const QByteArray encoded = QJsonDocument(libraries).toJson(QJsonDocument::Compact);
    invokeOnWorkerAsync([this, encoded]() {
        m_worker->setValue(QStringLiteral("cache/libraries"), QString::fromUtf8(encoded));
    });
}

QJsonArray DatabaseManager::loadMovies(const QString &libraryId)
{
    const auto value =
        invokeOnWorker([this, libraryId]() { return m_worker->value(QStringLiteral("cache/movies/%1").arg(libraryId)); });
    return QJsonDocument::fromJson(value.toByteArray()).array();
}

void DatabaseManager::saveMovies(const QString &libraryId, const QJsonArray &movies)
{
    const QByteArray encoded = QJsonDocument(movies).toJson(QJsonDocument::Compact);
    invokeOnWorkerAsync([this, libraryId, encoded]() {
        m_worker->setValue(QStringLiteral("cache/movies/%1").arg(libraryId), QString::fromUtf8(encoded));
    });
}

bool DatabaseManager::loadNightModeEnabled()
{
    return invokeOnWorker([this]() { return m_worker->value(QStringLiteral("settings/nightMode")); }).toBool();
}

void DatabaseManager::saveNightModeEnabled(bool enabled)
{
    invokeOnWorkerAsync([this, enabled]() {
        m_worker->setValue(QStringLiteral("settings/nightMode"), enabled);
    });
}

} // namespace JellyfinNative

#include "DatabaseManager.moc"
