#include "DatabaseManager.h"

#include "../diagnostics/Diagnostics.h"

#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMetaObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QVector>

#include <algorithm>

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
        if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next())
            return false;
        const int existingVersion = query.value(0).toInt();
        if (existingVersion > 3) {
            qWarning() << "database: unsupported schema version"
                       << existingVersion;
            return false;
        }
        if (!query.exec(QStringLiteral("PRAGMA journal_mode = WAL")) ||
            !query.exec(QStringLiteral("PRAGMA busy_timeout = 5000")) ||
            !query.exec(QStringLiteral(
                "CREATE TABLE IF NOT EXISTS kv ("
                "key TEXT PRIMARY KEY,"
                "value TEXT NOT NULL"
                ")")) ||
            !query.exec(QStringLiteral(
                "CREATE TABLE IF NOT EXISTS cache_entries ("
                "namespace TEXT NOT NULL,"
                "key TEXT NOT NULL,"
                "value BLOB NOT NULL,"
                "updated_at INTEGER NOT NULL,"
                "accessed_at INTEGER NOT NULL,"
                "expires_at INTEGER,"
                "PRIMARY KEY(namespace, key)"
                ")")) ||
            !query.exec(QStringLiteral(
                "CREATE INDEX IF NOT EXISTS cache_entries_expiry "
                "ON cache_entries(expires_at)")) ||
            !query.exec(QStringLiteral(
                "CREATE INDEX IF NOT EXISTS cache_entries_access "
                "ON cache_entries(accessed_at)")) ||
            !query.exec(QStringLiteral(
                "CREATE TABLE IF NOT EXISTS home_payload ("
                "key TEXT PRIMARY KEY,"
                "schema_version INTEGER NOT NULL,"
                "payload BLOB NOT NULL,"
                "saved_at INTEGER NOT NULL"
                ")")) ||
            !query.exec(QStringLiteral("PRAGMA user_version = 3"))) {
            qWarning() << "database: schema migration failed"
                       << query.lastError().text();
            return false;
        }
        return true;
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

    QJsonObject homePayload(const QString &key, int schemaVersion)
    {
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral(
            "SELECT payload FROM home_payload "
            "WHERE key = ? AND schema_version = ?"));
        query.addBindValue(key);
        query.addBindValue(schemaVersion);
        if (!query.exec() || !query.next())
            return {};
        return QJsonDocument::fromJson(query.value(0).toByteArray()).object();
    }

    void setHomePayload(const QString &key, int schemaVersion,
                        const QJsonObject &payload)
    {
        const QByteArray encoded = QJsonDocument(payload).toJson(QJsonDocument::Compact);
        if (encoded.isEmpty() || key.isEmpty())
            return;
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral(
            "INSERT INTO home_payload(key, schema_version, payload, saved_at) "
            "VALUES(?, ?, ?, ?) "
            "ON CONFLICT(key) DO UPDATE SET "
            "schema_version = excluded.schema_version, "
            "payload = excluded.payload, saved_at = excluded.saved_at"));
        query.addBindValue(key);
        query.addBindValue(schemaVersion);
        query.addBindValue(encoded);
        query.addBindValue(QDateTime::currentMSecsSinceEpoch());
        if (!query.exec())
            qWarning() << "database: home payload write failed" << query.lastError().text();
    }

    void clearHomePayloads()
    {
        QSqlQuery query(m_database);
        query.exec(QStringLiteral("DELETE FROM home_payload"));
    }

    int schemaVersion()
    {
        QSqlQuery query(m_database);
        if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next())
            return 0;
        return query.value(0).toInt();
    }

    QByteArray cacheValue(const QString &nameSpace, const QString &key,
                          qint64 maxAgeMs)
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral(
            "SELECT value, updated_at, expires_at FROM cache_entries "
            "WHERE namespace = ? AND key = ?"));
        query.addBindValue(nameSpace);
        query.addBindValue(key);
        if (!query.exec() || !query.next())
            return {};

        const qint64 updatedAt = query.value(1).toLongLong();
        const qint64 expiresAt = query.value(2).toLongLong();
        const bool expired = expiresAt > 0 && expiresAt <= now;
        const bool stale = maxAgeMs >= 0 && updatedAt < now - maxAgeMs;
        if (expired || stale) {
            removeCacheValue(nameSpace, key);
            return {};
        }

        const QByteArray result = query.value(0).toByteArray();
        QSqlQuery touch(m_database);
        touch.prepare(QStringLiteral(
            "UPDATE cache_entries SET accessed_at = ? "
            "WHERE namespace = ? AND key = ?"));
        touch.addBindValue(now);
        touch.addBindValue(nameSpace);
        touch.addBindValue(key);
        touch.exec();
        return result;
    }

    void setCacheValue(const QString &nameSpace, const QString &key,
                       const QByteArray &value, qint64 ttlMs)
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        QVariant expiresAt;
        if (ttlMs > 0)
            expiresAt = now + ttlMs;
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral(
            "INSERT INTO cache_entries(namespace, key, value, updated_at, "
            "accessed_at, expires_at) VALUES(?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(namespace, key) DO UPDATE SET "
            "value = excluded.value, updated_at = excluded.updated_at, "
            "accessed_at = excluded.accessed_at, expires_at = excluded.expires_at"));
        query.addBindValue(nameSpace);
        query.addBindValue(key);
        query.addBindValue(value);
        query.addBindValue(now);
        query.addBindValue(now);
        query.addBindValue(expiresAt);
        if (!query.exec())
            qWarning() << "database: cache write failed" << query.lastError().text();
    }

    void removeCacheValue(const QString &nameSpace, const QString &key)
    {
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral(
            "DELETE FROM cache_entries WHERE namespace = ? AND key = ?"));
        query.addBindValue(nameSpace);
        query.addBindValue(key);
        query.exec();
    }

    void invalidateCacheNamespace(const QString &nameSpace)
    {
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral(
            "DELETE FROM cache_entries WHERE namespace = ?"));
        query.addBindValue(nameSpace);
        query.exec();
    }

    void evictCacheEntries(int maximumEntries)
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        QSqlQuery expired(m_database);
        expired.prepare(QStringLiteral(
            "DELETE FROM cache_entries WHERE expires_at IS NOT NULL "
            "AND expires_at <= ?"));
        expired.addBindValue(now);
        expired.exec();

        const int limit = std::max(0, maximumEntries);
        QSqlQuery countQuery(m_database);
        if (!countQuery.exec(QStringLiteral("SELECT COUNT(*) FROM cache_entries")) ||
            !countQuery.next()) {
            return;
        }
        const int removeCount = countQuery.value(0).toInt() - limit;
        if (removeCount <= 0)
            return;

        QSqlQuery victims(m_database);
        victims.prepare(QStringLiteral(
            "SELECT namespace, key FROM cache_entries "
            "ORDER BY accessed_at ASC LIMIT ?"));
        victims.addBindValue(removeCount);
        if (!victims.exec())
            return;

        QVector<QPair<QString, QString>> keys;
        while (victims.next())
            keys.push_back({victims.value(0).toString(), victims.value(1).toString()});
        for (const auto &[nameSpace, key] : keys)
            removeCacheValue(nameSpace, key);
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
    Diagnostics::Phase phase(QStringLiteral("database"), QStringLiteral("initialize"));
    if (m_worker)
        return true;

    QDir().mkpath(QFileInfo(databasePath).absolutePath());

    m_worker = new DatabaseWorker();
    m_worker->moveToThread(&m_thread);
    m_thread.setObjectName(QStringLiteral("database-worker"));
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
    Diagnostics::Phase phase(QStringLiteral("shutdown"), QStringLiteral("database_shutdown"));
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
    session.userName = invokeOnWorker([this]() { return m_worker->value(QStringLiteral("login/userName")); }).toString();
    session.serverId = invokeOnWorker([this]() { return m_worker->value(QStringLiteral("login/serverId")); }).toString();
    return session;
}

void DatabaseManager::saveAuthSession(const AuthSession &session)
{
    invokeOnWorkerAsync([this, session]() {
        m_worker->setValue(QStringLiteral("login/accessToken"), session.accessToken);
        m_worker->setValue(QStringLiteral("login/userId"), session.userId);
        m_worker->setValue(QStringLiteral("login/userName"), session.userName);
        m_worker->setValue(QStringLiteral("login/serverId"), session.serverId);
    });
}

void DatabaseManager::clearAuthSession()
{
    invokeOnWorkerAsync([this]() {
        m_worker->setValue(QStringLiteral("login/accessToken"), QString());
        m_worker->setValue(QStringLiteral("login/userId"), QString());
        m_worker->setValue(QStringLiteral("login/userName"), QString());
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
    constexpr qint64 maxAgeMs = 7LL * 24 * 60 * 60 * 1000;
    QByteArray encoded =
        loadCacheEntry(QStringLiteral("discovery"), QStringLiteral("servers"),
                       maxAgeMs);
    if (encoded.isEmpty()) {
        encoded = invokeOnWorker([this]() {
            return m_worker->value(QStringLiteral("cache/discoveredServers"));
        }).toByteArray();
        if (!encoded.isEmpty())
            saveCacheEntry(QStringLiteral("discovery"), QStringLiteral("servers"),
                           encoded, maxAgeMs);
    }
    return QJsonDocument::fromJson(encoded).array();
}

void DatabaseManager::saveDiscoveredServers(const QJsonArray &servers)
{
    const QByteArray encoded = QJsonDocument(servers).toJson(QJsonDocument::Compact);
    constexpr qint64 ttlMs = 7LL * 24 * 60 * 60 * 1000;
    saveCacheEntry(QStringLiteral("discovery"), QStringLiteral("servers"),
                   encoded, ttlMs);
    evictCacheEntries(512);
}

QJsonObject DatabaseManager::loadHomePayload(const QString &key,
                                             int schemaVersion)
{
    if (key.isEmpty())
        return {};
    return invokeOnWorker([this, key, schemaVersion]() {
        return m_worker->homePayload(key, schemaVersion);
    }).toJsonObject();
}

void DatabaseManager::saveHomePayload(const QString &key, int schemaVersion,
                                      const QJsonObject &payload)
{
    if (key.isEmpty() || payload.isEmpty())
        return;
    invokeOnWorkerAsync([this, key, schemaVersion, payload]() {
        m_worker->setHomePayload(key, schemaVersion, payload);
    });
}

void DatabaseManager::invalidateHomePayloads()
{
    invokeOnWorkerAsync([this]() { m_worker->clearHomePayloads(); });
}


QString DatabaseManager::loadSetting(const QString &key, const QString &defaultValue)
{
    const QVariant value = invokeOnWorker([this, key]() { return m_worker->value(key); });
    if (!value.isValid() || value.toString().isEmpty())
        return defaultValue;
    return value.toString();
}

void DatabaseManager::saveSetting(const QString &key, const QString &value)
{
    invokeOnWorkerAsync([this, key, value]() { m_worker->setValue(key, value); });
}

int DatabaseManager::schemaVersion()
{
    return invokeOnWorker([this]() { return m_worker->schemaVersion(); }).toInt();
}

QByteArray DatabaseManager::loadCacheEntry(const QString &nameSpace,
                                           const QString &key,
                                           qint64 maxAgeMs)
{
    return invokeOnWorker([this, nameSpace, key, maxAgeMs]() {
        return m_worker->cacheValue(nameSpace, key, maxAgeMs);
    }).toByteArray();
}

void DatabaseManager::saveCacheEntry(const QString &nameSpace,
                                     const QString &key,
                                     const QByteArray &value, qint64 ttlMs)
{
    invokeOnWorkerAsync([this, nameSpace, key, value, ttlMs]() {
        m_worker->setCacheValue(nameSpace, key, value, ttlMs);
    });
}

void DatabaseManager::invalidateCacheNamespace(const QString &nameSpace)
{
    invokeOnWorkerAsync([this, nameSpace]() {
        m_worker->invalidateCacheNamespace(nameSpace);
    });
}

void DatabaseManager::evictCacheEntries(int maximumEntries)
{
    invokeOnWorkerAsync([this, maximumEntries]() {
        m_worker->evictCacheEntries(maximumEntries);
    });
}

} // namespace JellyfinNative

#include "DatabaseManager.moc"
