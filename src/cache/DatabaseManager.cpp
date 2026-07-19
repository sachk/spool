#include "DatabaseManager.h"

#include "../diagnostics/Diagnostics.h"
#include <QCoroFuture>

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
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
    bool initialize(const QString& cachePath)
    {
        QElapsedTimer timer;
        timer.start();
        const QFileInfo cacheInfo(cachePath);
        if (!QDir().mkpath(cacheInfo.absolutePath())) {
            qWarning() << "database: failed to create data directory for" << cachePath;
            return initializeTransient();
        }

        const QString statePath = cacheInfo.dir().filePath(QStringLiteral("state.sqlite"));
        if (!openState(statePath)) {
            qWarning() << "database: invalid durable state; preserving and rebuilding" << statePath;
            if (!recoverState(statePath))
                return false;
            m_recoveryNotice = QStringLiteral(
                "Local account data could not be read and was reset. A diagnostic backup was preserved.");
        }
        qInfo() << "startup: durable database ready in" << timer.elapsed() << "ms";
        if (!openCache(cachePath)) {
            qWarning() << "database: invalid cache; preserving and rebuilding" << cachePath;
            if (!recoverCache(cachePath))
                return false;
            if (m_recoveryNotice.isEmpty())
                m_recoveryNotice = QStringLiteral("The local cache was damaged and has been rebuilt.");
        }
        qInfo() << "startup: database worker ready in" << timer.elapsed() << "ms";
        return true;
    }

    QString recoveryNotice() const
    {
        return m_recoveryNotice;
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
        if (keys.isEmpty())
            return result;

        QStringList placeholders;
        placeholders.reserve(keys.size());
        for (const QString& key : keys) {
            result.insert(key, {});
            placeholders.append(QStringLiteral("?"));
        }

        QSqlQuery query(m_database);
        query.prepare(QStringLiteral("SELECT key, value FROM kv WHERE key IN (%1)").arg(placeholders.join(',')));
        for (const QString& key : keys)
            query.addBindValue(key);
        if (!query.exec()) {
            qWarning() << "database: batch value load failed" << query.lastError().text();
            return result;
        }
        while (query.next())
            result.insert(query.value(0).toString(), query.value(1));
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

private:
    static bool quickCheck(QSqlDatabase& database)
    {
        QSqlQuery query(database);
        return query.exec(QStringLiteral("PRAGMA quick_check")) && query.next()
            && query.value(0).toString() == QStringLiteral("ok");
    }

    static bool prepareConnection(QSqlDatabase& database)
    {
        QSqlQuery query(database);
        return quickCheck(database) && query.exec(QStringLiteral("PRAGMA journal_mode = WAL"))
            && query.exec(QStringLiteral("PRAGMA busy_timeout = 5000"));
    }

    static bool ensureConnection(QSqlDatabase& database, const QString& connectionName, const QString& path)
    {
        if (!database.isValid()) {
            if (QSqlDatabase::contains(connectionName))
                database = QSqlDatabase::database(connectionName);
            else
                database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        }
        database.setDatabaseName(path);
        if (!database.open()) {
            qWarning() << "database: failed to open" << path << database.lastError().text();
            return false;
        }
        return true;
    }

    bool openState(const QString& path)
    {
        if (!ensureConnection(m_database, QStringLiteral("jellyfin_native_state"), path)
            || !prepareConnection(m_database))
            return false;
        QSqlQuery query(m_database);
        if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next())
            return false;
        const int existingVersion = query.value(0).toInt();
        if (existingVersion > 1)
            return false;
        if (!query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS kv ("
                                       "key TEXT PRIMARY KEY,"
                                       "value TEXT NOT NULL"
                                       ")"))
            || !query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS profiles ("
                                          "profile_id TEXT PRIMARY KEY,"
                                          "server_id TEXT NOT NULL,"
                                          "server_name TEXT NOT NULL,"
                                          "server_url TEXT NOT NULL,"
                                          "user_id TEXT NOT NULL,"
                                          "user_name TEXT NOT NULL,"
                                          "access_token TEXT NOT NULL,"
                                          "avatar_tag TEXT NOT NULL,"
                                          "last_used_at INTEGER NOT NULL,"
                                          "created_at INTEGER NOT NULL,"
                                          "needs_authentication INTEGER NOT NULL"
                                          ")"))
            || !query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS profiles_usage "
                                          "ON profiles(last_used_at DESC, created_at ASC)"))
            || !query.exec(QStringLiteral("PRAGMA user_version = 1"))) {
            qWarning() << "database: durable schema creation failed" << query.lastError().text();
            return false;
        }
        return true;
    }

    bool openCache(const QString& path)
    {
        if (!ensureConnection(m_cacheDatabase, QStringLiteral("jellyfin_native_cache"), path)
            || !prepareConnection(m_cacheDatabase))
            return false;
        QSqlQuery query(m_cacheDatabase);
        if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next())
            return false;
        const int existingVersion = query.value(0).toInt();
        if (existingVersion > 1)
            return false;
        if (!query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS cache_entries ("
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
            || !query.exec(QStringLiteral("PRAGMA user_version = 1"))) {
            qWarning() << "database: cache schema creation failed" << query.lastError().text();
            return false;
        }
        return true;
    }

    static void closeForRecovery(QSqlDatabase& database)
    {
        if (database.isValid())
            database.close();
    }

    static bool preserveBrokenDatabase(const QString& path)
    {
        QFile::remove(path + QStringLiteral("-wal"));
        QFile::remove(path + QStringLiteral("-shm"));
        if (!QFileInfo::exists(path))
            return true;
        const QString backup = path + QStringLiteral(".corrupt-")
            + QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd'T'HHmmsszzz'Z'"));
        if (!QFile::rename(path, backup))
            return false;

        const QFileInfo fileInfo(path);
        QDir directory = fileInfo.dir();
        const QFileInfoList backups
            = directory.entryInfoList({ fileInfo.fileName() + QStringLiteral(".corrupt-*") }, QDir::Files, QDir::Time);
        constexpr qsizetype maximumBackups = 3;
        for (qsizetype index = maximumBackups; index < backups.size(); ++index)
            QFile::remove(backups.at(index).absoluteFilePath());
        return true;
    }

    bool recoverState(const QString& path)
    {
        closeForRecovery(m_database);
        if (preserveBrokenDatabase(path) && openState(path))
            return true;
        closeForRecovery(m_database);
        return openState(QStringLiteral(":memory:"));
    }

    bool recoverCache(const QString& path)
    {
        closeForRecovery(m_cacheDatabase);
        if (preserveBrokenDatabase(path) && openCache(path))
            return true;
        closeForRecovery(m_cacheDatabase);
        return openCache(QStringLiteral(":memory:"));
    }

    bool initializeTransient()
    {
        m_recoveryNotice = QStringLiteral(
            "Local storage is unavailable. Account data and cache changes will last only until the app closes.");
        return openState(QStringLiteral(":memory:")) && openCache(QStringLiteral(":memory:"));
    }

public:
    QJsonObject homePayload(const QString& key, int schemaVersion)
    {
        QSqlQuery query(m_cacheDatabase);
        query.prepare(QStringLiteral("SELECT payload FROM home_payload "
                                     "WHERE key = ? AND schema_version = ?"));
        query.addBindValue(key);
        query.addBindValue(schemaVersion);
        if (!query.exec() || !query.next())
            return {};
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(query.value(0).toByteArray(), &error);
        if (error.error == QJsonParseError::NoError && document.isObject())
            return document.object();
        QSqlQuery remove(m_cacheDatabase);
        remove.prepare(QStringLiteral("DELETE FROM home_payload WHERE key = ?"));
        remove.addBindValue(key);
        remove.exec();
        return {};
    }

    void setHomePayload(const QString& key, int schemaVersion, const QJsonObject& payload)
    {
        const QByteArray encoded = QJsonDocument(payload).toJson(QJsonDocument::Compact);
        if (encoded.isEmpty() || key.isEmpty())
            return;
        QSqlQuery query(m_cacheDatabase);
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
        QSqlQuery query(m_cacheDatabase);
        query.exec(QStringLiteral("DELETE FROM home_payload"));
    }

    int schemaVersion()
    {
        QSqlQuery query(m_cacheDatabase);
        if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next())
            return 0;
        return query.value(0).toInt();
    }

    QByteArray cacheValue(const QString& nameSpace, const QString& key, qint64 maxAgeMs)
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        QSqlQuery query(m_cacheDatabase);
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
        QSqlQuery touch(m_cacheDatabase);
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
        QSqlQuery query(m_cacheDatabase);
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
        QSqlQuery query(m_cacheDatabase);
        query.prepare(QStringLiteral("DELETE FROM cache_entries WHERE namespace = ? AND key = ?"));
        query.addBindValue(nameSpace);
        query.addBindValue(key);
        query.exec();
    }

    void invalidateCacheNamespace(const QString& nameSpace)
    {
        QSqlQuery query(m_cacheDatabase);
        query.prepare(QStringLiteral("DELETE FROM cache_entries WHERE namespace = ?"));
        query.addBindValue(nameSpace);
        query.exec();
    }

    void evictCacheEntries(int maximumEntries)
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        QSqlQuery expired(m_cacheDatabase);
        expired.prepare(QStringLiteral("DELETE FROM cache_entries WHERE expires_at IS NOT NULL "
                                       "AND expires_at <= ?"));
        expired.addBindValue(now);
        expired.exec();

        const int limit = std::max(0, maximumEntries);
        QSqlQuery countQuery(m_cacheDatabase);
        if (!countQuery.exec(QStringLiteral("SELECT COUNT(*) FROM cache_entries")) || !countQuery.next()) {
            return;
        }
        const int removeCount = countQuery.value(0).toInt() - limit;
        if (removeCount <= 0)
            return;

        QSqlQuery victims(m_cacheDatabase);
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

    std::vector<AccountProfile> accountProfiles()
    {
        std::vector<AccountProfile> profiles;
        QSqlQuery query(m_database);
        if (!query.exec(QStringLiteral("SELECT profile_id, server_id, server_name, server_url, user_id, user_name, "
                                       "access_token, avatar_tag, last_used_at, created_at, needs_authentication "
                                       "FROM profiles ORDER BY last_used_at DESC, created_at ASC"))) {
            qWarning() << "database: profile load failed" << query.lastError().text();
            return profiles;
        }
        while (query.next())
            profiles.push_back(accountProfileFromQuery(query));
        return profiles;
    }

    std::optional<AccountProfile> activateAccountProfile(const QString& profileId)
    {
        if (profileId.isEmpty() || !m_database.transaction())
            return std::nullopt;

        QSqlQuery query(m_database);
        query.prepare(QStringLiteral("SELECT profile_id, server_id, server_name, server_url, user_id, user_name, "
                                     "access_token, avatar_tag, last_used_at, created_at, needs_authentication "
                                     "FROM profiles WHERE profile_id = ?"));
        query.addBindValue(profileId);
        if (!query.exec() || !query.next()) {
            m_database.rollback();
            return std::nullopt;
        }

        AccountProfile profile = accountProfileFromQuery(query);
        profile.lastUsedAt = QDateTime::currentMSecsSinceEpoch();
        QSqlQuery touch(m_database);
        touch.prepare(QStringLiteral("UPDATE profiles SET last_used_at = ? WHERE profile_id = ?"));
        touch.addBindValue(profile.lastUsedAt);
        touch.addBindValue(profileId);
        if (!touch.exec() || !m_database.commit()) {
            m_database.rollback();
            return std::nullopt;
        }
        return profile;
    }

    void upsertAccountProfile(const AccountProfile& profile)
    {
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral(
            "INSERT INTO profiles(profile_id, server_id, server_name, server_url, user_id, user_name, "
            "access_token, avatar_tag, last_used_at, created_at, needs_authentication) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(profile_id) DO UPDATE SET "
            "server_id = excluded.server_id, server_name = excluded.server_name, "
            "server_url = excluded.server_url, user_id = excluded.user_id, user_name = excluded.user_name, "
            "access_token = excluded.access_token, avatar_tag = excluded.avatar_tag, "
            "last_used_at = excluded.last_used_at, needs_authentication = excluded.needs_authentication"));
        const auto bindText
            = [&query](const QString& value) { query.addBindValue(value.isNull() ? QStringLiteral("") : value); };
        bindText(profile.profileId);
        bindText(profile.serverId);
        bindText(profile.serverName);
        bindText(profile.serverUrl);
        bindText(profile.userId);
        bindText(profile.userName);
        bindText(profile.accessToken);
        bindText(profile.avatarTag);
        query.addBindValue(profile.lastUsedAt);
        query.addBindValue(profile.createdAt);
        query.addBindValue(profile.needsAuthentication);
        if (!query.exec())
            qWarning() << "database: profile upsert failed" << query.lastError().text();
    }

    void expireAccountProfile(const QString& profileId)
    {
        QSqlQuery query(m_database);
        query.prepare(
            QStringLiteral("UPDATE profiles SET access_token = '', needs_authentication = 1 WHERE profile_id = ?"));
        query.addBindValue(profileId);
        if (!query.exec())
            qWarning() << "database: profile expiration failed" << query.lastError().text();
    }

    void removeAccountProfile(const QString& profileId)
    {
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral("DELETE FROM profiles WHERE profile_id = ?"));
        query.addBindValue(profileId);
        if (!query.exec())
            qWarning() << "database: profile removal failed" << query.lastError().text();
    }

    void clearAccountProfiles()
    {
        QSqlQuery query(m_database);
        if (!query.exec(QStringLiteral("DELETE FROM profiles")))
            qWarning() << "database: profile clear failed" << query.lastError().text();
    }

    void close()
    {
        closeDatabase(m_cacheDatabase);
        closeDatabase(m_database);
    }

private:
    static void closeDatabase(QSqlDatabase& database)
    {
        if (!database.isValid())
            return;
        const QString connectionName = database.connectionName();
        database.close();
        database = {};
        QSqlDatabase::removeDatabase(connectionName);
    }
    static AccountProfile accountProfileFromQuery(const QSqlQuery& query)
    {
        return { query.value(0).toString(), query.value(1).toString(), query.value(2).toString(),
            query.value(3).toString(), query.value(4).toString(), query.value(5).toString(), query.value(6).toString(),
            query.value(7).toString(), query.value(8).toLongLong(), query.value(9).toLongLong(),
            query.value(10).toBool() };
    }
    QSqlDatabase m_database;
    QSqlDatabase m_cacheDatabase;
    QString m_recoveryNotice;
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
        const QString recoveryNotice = worker->recoveryNotice();
        promise->addResult(success);
        promise->finish();
        if (success && recoveryNotice.isEmpty())
            return;
        QMetaObject::invokeMethod(
            manager,
            [manager, success, recoveryNotice]() {
                if (!manager)
                    return;
                if (success)
                    emit manager->recoveryNotice(recoveryNotice);
                else
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
QCoro::Task<std::vector<AccountProfile>> DatabaseManager::loadAccountProfilesAsync()
{
    if (!co_await awaitInitialization())
        co_return std::vector<AccountProfile> {};
    DatabaseWorker *worker = m_worker;
    co_return co_await workerTask(
        worker, [worker]() { return worker ? worker->accountProfiles() : std::vector<AccountProfile> {}; });
}

QCoro::Task<std::optional<AccountProfile>> DatabaseManager::activateAccountProfileAsync(const QString& profileId)
{
    if (!co_await awaitInitialization())
        co_return std::nullopt;
    DatabaseWorker *worker = m_worker;
    co_return co_await workerTask(
        worker, [worker, profileId]() { return worker ? worker->activateAccountProfile(profileId) : std::nullopt; });
}

void DatabaseManager::upsertAccountProfile(const AccountProfile& profile)
{
    invokeOnWorkerAsync([this, profile]() { m_worker->upsertAccountProfile(profile); });
}

void DatabaseManager::expireAccountProfile(const QString& profileId)
{
    invokeOnWorkerAsync([this, profileId]() { m_worker->expireAccountProfile(profileId); });
}

void DatabaseManager::removeAccountProfile(const QString& profileId)
{
    invokeOnWorkerAsync([this, profileId]() { m_worker->removeAccountProfile(profileId); });
}

void DatabaseManager::clearAccountProfiles()
{
    invokeOnWorkerAsync([this]() { m_worker->clearAccountProfiles(); });
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
        const QByteArray encoded = worker->cacheValue(QStringLiteral("discovery"), QStringLiteral("servers"), maxAgeMs);
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(encoded, &error);
        if (error.error == QJsonParseError::NoError && document.isArray())
            return document.array();
        if (!encoded.isEmpty())
            worker->removeCacheValue(QStringLiteral("discovery"), QStringLiteral("servers"));
        return QJsonArray();
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
QCoro::Task<StartupState> DatabaseManager::loadStartupStateAsync(const QStringList& keys)
{
    if (!m_initializationFuture.isValid())
        co_return StartupState {};

    // Queue directly behind database initialization instead of resuming on
    // the GUI thread first. The snapshot is then read while QML is compiling,
    // and its completed future can resume routing on the first event-loop turn.
    DatabaseWorker *worker = m_worker;
    const QFuture<bool> initialization = m_initializationFuture;
    co_return co_await workerTask(worker, [worker, keys, initialization]() {
        if (!worker || !initialization.result())
            return StartupState {};
        return StartupState { worker->value(QStringLiteral("client/deviceId")).toString(), worker->values(keys),
            worker->accountProfiles() };
    });
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
