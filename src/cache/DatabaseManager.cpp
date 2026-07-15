#include "DatabaseManager.h"

#include "../diagnostics/Diagnostics.h"
#include <QCoroFuture>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMetaObject>
#include <QPointer>
#include <QPromise>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QVector>

#include <algorithm>
#include <memory>

namespace JellyfinNative {

class DatabaseWorker final : public QObject {
    Q_OBJECT

public:
    bool initialize(const QString& databasePath)
    {
        if (!QDir().mkpath(QFileInfo(databasePath).absolutePath())) {
            qWarning() << "database: failed to create cache directory for" << databasePath;
            return false;
        }
        const QString connectionName = QStringLiteral("jellyfin_native_cache");
        if (QSqlDatabase::contains(connectionName))
            m_database = QSqlDatabase::database(connectionName);
        else
            m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);

        m_database.setDatabaseName(databasePath);
        if (!m_database.open()) {
            qWarning() << "database: failed to open" << databasePath << m_database.lastError().text();
            return false;
        }

        QSqlQuery query(m_database);
        if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next())
            return false;
        const int existingVersion = query.value(0).toInt();
        if (existingVersion > 4) {
            qWarning() << "database: unsupported schema version" << existingVersion;
            return false;
        }
        if (!query.exec(QStringLiteral("PRAGMA journal_mode = WAL"))
            || !query.exec(QStringLiteral("PRAGMA busy_timeout = 5000"))
            || !query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS kv ("
                                          "key TEXT PRIMARY KEY,"
                                          "value TEXT NOT NULL"
                                          ")"))
            || !query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS cache_entries ("
                                          "namespace TEXT NOT NULL,"
                                          "key TEXT NOT NULL,"
                                          "value BLOB NOT NULL,"
                                          "updated_at INTEGER NOT NULL,"
                                          "accessed_at INTEGER NOT NULL,"
                                          "expires_at INTEGER,"
                                          "PRIMARY KEY(namespace, key)"
                                          ")"))
            || !query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS cache_entries_expiry "
                                          "ON cache_entries(expires_at)"))
            || !query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS cache_entries_access "
                                          "ON cache_entries(accessed_at)"))
            || !query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS home_payload ("
                                          "key TEXT PRIMARY KEY,"
                                          "schema_version INTEGER NOT NULL,"
                                          "payload BLOB NOT NULL,"
                                          "saved_at INTEGER NOT NULL"
                                          ")"))
            || !query.exec(QStringLiteral("PRAGMA user_version = 4"))) {
            qWarning() << "database: schema migration failed" << query.lastError().text();
            return false;
        }
        if (existingVersion < 4) {
            if (!query.exec(
                    QStringLiteral("DELETE FROM cache_entries WHERE namespace = 'discovery' AND key = 'servers'"))
                || !query.exec(QStringLiteral("DELETE FROM home_payload"))) {
                qWarning() << "database: cache invalidation migration failed" << query.lastError().text();
            }
        }
        return true;
    }

    QVariant value(const QString& key)
    {
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral("SELECT value FROM kv WHERE key = ?"));
        query.addBindValue(key);
        if (!query.exec() || !query.next())
            return {};
        return query.value(0);
    }

    QVariantMap values(const QStringList& keys)
    {
        QVariantMap result;
        for (const QString& key : keys)
            result.insert(key, value(key));
        return result;
    }

    void setValue(const QString& key, const QVariant& value)
    {
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral("INSERT INTO kv(key, value) VALUES(?, ?) "
                                     "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
        query.addBindValue(key);
        query.addBindValue(value);
        query.exec();
    }

    QJsonObject homePayload(const QString& key, int schemaVersion)
    {
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral("SELECT payload FROM home_payload "
                                     "WHERE key = ? AND schema_version = ?"));
        query.addBindValue(key);
        query.addBindValue(schemaVersion);
        if (!query.exec() || !query.next())
            return {};
        return QJsonDocument::fromJson(query.value(0).toByteArray()).object();
    }

    void setHomePayload(const QString& key, int schemaVersion, const QJsonObject& payload)
    {
        const QByteArray encoded = QJsonDocument(payload).toJson(QJsonDocument::Compact);
        if (encoded.isEmpty() || key.isEmpty())
            return;
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral("INSERT INTO home_payload(key, schema_version, payload, saved_at) "
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

    QByteArray cacheValue(const QString& nameSpace, const QString& key, qint64 maxAgeMs)
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral("SELECT value, updated_at, expires_at FROM cache_entries "
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
        touch.prepare(QStringLiteral("UPDATE cache_entries SET accessed_at = ? "
                                     "WHERE namespace = ? AND key = ?"));
        touch.addBindValue(now);
        touch.addBindValue(nameSpace);
        touch.addBindValue(key);
        touch.exec();
        return result;
    }

    void setCacheValue(const QString& nameSpace, const QString& key, const QByteArray& value, qint64 ttlMs)
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        QVariant expiresAt;
        if (ttlMs > 0)
            expiresAt = now + ttlMs;
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral("INSERT INTO cache_entries(namespace, key, value, updated_at, "
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

    void removeCacheValue(const QString& nameSpace, const QString& key)
    {
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral("DELETE FROM cache_entries WHERE namespace = ? AND key = ?"));
        query.addBindValue(nameSpace);
        query.addBindValue(key);
        query.exec();
    }

    void invalidateCacheNamespace(const QString& nameSpace)
    {
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral("DELETE FROM cache_entries WHERE namespace = ?"));
        query.addBindValue(nameSpace);
        query.exec();
    }

    void evictCacheEntries(int maximumEntries)
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        QSqlQuery expired(m_database);
        expired.prepare(QStringLiteral("DELETE FROM cache_entries WHERE expires_at IS NOT NULL "
                                       "AND expires_at <= ?"));
        expired.addBindValue(now);
        expired.exec();

        const int limit = std::max(0, maximumEntries);
        QSqlQuery countQuery(m_database);
        if (!countQuery.exec(QStringLiteral("SELECT COUNT(*) FROM cache_entries")) || !countQuery.next()) {
            return;
        }
        const int removeCount = countQuery.value(0).toInt() - limit;
        if (removeCount <= 0)
            return;

        QSqlQuery victims(m_database);
        victims.prepare(QStringLiteral("SELECT namespace, key FROM cache_entries "
                                       "ORDER BY accessed_at ASC LIMIT ?"));
        victims.addBindValue(removeCount);
        if (!victims.exec())
            return;

        QVector<QPair<QString, QString>> keys;
        while (victims.next())
            keys.push_back({ victims.value(0).toString(), victims.value(1).toString() });
        for (const auto& [nameSpace, key] : keys)
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

namespace {

    template <typename Callback>
    QCoro::Task<std::invoke_result_t<Callback>> workerTask(DatabaseWorker *worker, Callback callback)
    {
        using Result = std::invoke_result_t<Callback>;
        auto promise = std::make_shared<QPromise<Result>>();
        promise->start();
        QFuture<Result> future = promise->future();
        auto finish = [promise, callback = std::move(callback)]() mutable {
            try {
                promise->addResult(std::invoke(callback));
            } catch (...) {
                promise->setException(std::current_exception());
            }
            promise->finish();
        };
        if (!worker || !QMetaObject::invokeMethod(worker, finish, Qt::QueuedConnection))
            finish();
        co_return co_await future;
    }

    AuthSession authSessionFromWorker(DatabaseWorker *worker)
    {
        AuthSession session;
        session.accessToken = worker->value(QStringLiteral("login/accessToken")).toString();
        session.userId = worker->value(QStringLiteral("login/userId")).toString();
        session.userName = worker->value(QStringLiteral("login/userName")).toString();
        session.serverId = worker->value(QStringLiteral("login/serverId")).toString();
        return session;
    }

    QString settingFromWorker(DatabaseWorker *worker, const QString& key, const QString& defaultValue)
    {
        const QVariant value = worker->value(key);
        if (!value.isValid() || value.toString().isEmpty())
            return defaultValue;
        return value.toString();
    }

} // namespace

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
{
}

DatabaseManager::~DatabaseManager()
{
    shutdown();
}

bool DatabaseManager::initialize(const QString& databasePath)
{
    Diagnostics::Phase phase(QStringLiteral("database"), QStringLiteral("initialize"));
    if (m_worker)
        return true;

    QDir().mkpath(QFileInfo(databasePath).absolutePath());

    m_worker = new DatabaseWorker();
    m_worker->moveToThread(&m_thread);
    m_thread.setObjectName(QStringLiteral("database-worker"));
    m_thread.start();

    auto promise = std::make_shared<QPromise<bool>>();
    promise->start();
    m_initializationFuture = promise->future();
    QPointer<DatabaseManager> manager(this);
    QMetaObject::invokeMethod(m_worker, [manager, worker = m_worker, databasePath, promise]() {
        const bool success = worker->initialize(databasePath);
        promise->addResult(success);
        promise->finish();
        if (success)
            return;
        QMetaObject::invokeMethod(
            manager,
            [manager]() {
                if (manager)
                    emit manager->initializationFailed(QStringLiteral("Could not open the application database"));
            },
            Qt::QueuedConnection);
    });
    return true;
}

QCoro::Task<bool> DatabaseManager::awaitInitialization()
{
    if (!m_initializationFuture.isValid())
        co_return false;
    co_return co_await m_initializationFuture;
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

QCoro::Task<QString> DatabaseManager::loadLastServerUrlAsync()
{
    if (!co_await awaitInitialization())
        co_return QString();
    DatabaseWorker *worker = m_worker;
    co_return co_await workerTask(worker,
        [worker]() { return worker ? worker->value(QStringLiteral("login/serverUrl")).toString() : QString(); });
}

QCoro::Task<QString> DatabaseManager::loadLastUsernameAsync()
{
    if (!co_await awaitInitialization())
        co_return QString();
    DatabaseWorker *worker = m_worker;
    co_return co_await workerTask(
        worker, [worker]() { return worker ? worker->value(QStringLiteral("login/username")).toString() : QString(); });
}

void DatabaseManager::saveLoginHints(const QString& serverUrl, const QString& username)
{
    invokeOnWorkerAsync([this, serverUrl, username]() {
        m_worker->setValue(QStringLiteral("login/serverUrl"), serverUrl);
        m_worker->setValue(QStringLiteral("login/username"), username);
    });
}

QCoro::Task<AuthSession> DatabaseManager::loadAuthSessionAsync()
{
    if (!co_await awaitInitialization())
        co_return AuthSession {};
    DatabaseWorker *worker = m_worker;
    co_return co_await workerTask(
        worker, [worker]() { return worker ? authSessionFromWorker(worker) : AuthSession(); });
}

void DatabaseManager::saveAuthSession(const AuthSession& session)
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

QCoro::Task<QString> DatabaseManager::loadDeviceIdAsync()
{
    if (!co_await awaitInitialization())
        co_return QString();
    DatabaseWorker *worker = m_worker;
    co_return co_await workerTask(worker,
        [worker]() { return worker ? worker->value(QStringLiteral("client/deviceId")).toString() : QString(); });
}

void DatabaseManager::saveDeviceId(const QString& deviceId)
{
    invokeOnWorkerAsync([this, deviceId]() { m_worker->setValue(QStringLiteral("client/deviceId"), deviceId); });
}

QCoro::Task<QJsonArray> DatabaseManager::loadDiscoveredServersAsync()
{
    if (!co_await awaitInitialization())
        co_return QJsonArray();
    DatabaseWorker *worker = m_worker;
    co_return co_await workerTask(worker, [worker]() {
        if (!worker)
            return QJsonArray();
        constexpr qint64 maxAgeMs = 7LL * 24 * 60 * 60 * 1000;
        QByteArray encoded = worker->cacheValue(QStringLiteral("discovery"), QStringLiteral("servers"), maxAgeMs);
        if (encoded.isEmpty()) {
            encoded = worker->value(QStringLiteral("cache/discoveredServers")).toByteArray();
            if (!encoded.isEmpty())
                worker->setCacheValue(QStringLiteral("discovery"), QStringLiteral("servers"), encoded, maxAgeMs);
        }
        return QJsonDocument::fromJson(encoded).array();
    });
}

void DatabaseManager::saveDiscoveredServers(const QJsonArray& servers)
{
    const QByteArray encoded = QJsonDocument(servers).toJson(QJsonDocument::Compact);
    constexpr qint64 ttlMs = 7LL * 24 * 60 * 60 * 1000;
    saveCacheEntry(QStringLiteral("discovery"), QStringLiteral("servers"), encoded, ttlMs);
    evictCacheEntries(512);
}

QCoro::Task<QJsonObject> DatabaseManager::loadHomePayloadAsync(const QString& key, int schemaVersion)
{
    if (!co_await awaitInitialization())
        co_return QJsonObject();
    if (key.isEmpty())
        co_return QJsonObject();
    DatabaseWorker *worker = m_worker;
    co_return co_await workerTask(worker,
        [worker, key, schemaVersion]() { return worker ? worker->homePayload(key, schemaVersion) : QJsonObject(); });
}

void DatabaseManager::saveHomePayload(const QString& key, int schemaVersion, const QJsonObject& payload)
{
    if (key.isEmpty() || payload.isEmpty())
        return;
    invokeOnWorkerAsync(
        [this, key, schemaVersion, payload]() { m_worker->setHomePayload(key, schemaVersion, payload); });
}

void DatabaseManager::invalidateHomePayloads()
{
    invokeOnWorkerAsync([this]() { m_worker->clearHomePayloads(); });
}

QCoro::Task<QString> DatabaseManager::loadSettingAsync(const QString& key, const QString& defaultValue)
{
    if (!co_await awaitInitialization())
        co_return defaultValue;
    DatabaseWorker *worker = m_worker;
    co_return co_await workerTask(worker,
        [worker, key, defaultValue]() { return worker ? settingFromWorker(worker, key, defaultValue) : defaultValue; });
}

QCoro::Task<QVariantMap> DatabaseManager::loadValuesAsync(const QStringList& keys)
{
    if (!co_await awaitInitialization())
        co_return QVariantMap();
    DatabaseWorker *worker = m_worker;
    co_return co_await workerTask(worker, [worker, keys]() { return worker ? worker->values(keys) : QVariantMap(); });
}

void DatabaseManager::saveSetting(const QString& key, const QString& value)
{
    invokeOnWorkerAsync([this, key, value]() { m_worker->setValue(key, value); });
}

QCoro::Task<int> DatabaseManager::schemaVersionAsync()
{
    if (!co_await awaitInitialization())
        co_return 0;
    DatabaseWorker *worker = m_worker;
    co_return co_await workerTask(worker, [worker]() { return worker ? worker->schemaVersion() : 0; });
}

QCoro::Task<QByteArray> DatabaseManager::loadCacheEntryAsync(
    const QString& nameSpace, const QString& key, qint64 maxAgeMs)
{
    if (!co_await awaitInitialization())
        co_return QByteArray();
    DatabaseWorker *worker = m_worker;
    co_return co_await workerTask(worker, [worker, nameSpace, key, maxAgeMs]() {
        return worker ? worker->cacheValue(nameSpace, key, maxAgeMs) : QByteArray();
    });
}

void DatabaseManager::saveCacheEntry(
    const QString& nameSpace, const QString& key, const QByteArray& value, qint64 ttlMs)
{
    invokeOnWorkerAsync(
        [this, nameSpace, key, value, ttlMs]() { m_worker->setCacheValue(nameSpace, key, value, ttlMs); });
}

void DatabaseManager::invalidateCacheNamespace(const QString& nameSpace)
{
    invokeOnWorkerAsync([this, nameSpace]() { m_worker->invalidateCacheNamespace(nameSpace); });
}

void DatabaseManager::evictCacheEntries(int maximumEntries)
{
    invokeOnWorkerAsync([this, maximumEntries]() { m_worker->evictCacheEntries(maximumEntries); });
}

} // namespace JellyfinNative

#include "DatabaseManager.moc"
