#pragma once

#include <QByteArray>
#include <QCoroTask>
#include <QJsonArray>
#include <QJsonObject>
#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QThread>
#include <QVariant>

#include "../common/JellyfinTypes.h"

#include <utility>

namespace JellyfinNative {

class DatabaseWorker;

class DatabaseManager final : public QObject {
    Q_OBJECT

public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager() override;

    bool initialize(const QString& databasePath);
    void shutdown();

    QCoro::Task<QString> loadLastServerUrlAsync();
    QCoro::Task<QString> loadLastUsernameAsync();
    void saveLoginHints(const QString& serverUrl, const QString& username);

    QCoro::Task<AuthSession> loadAuthSessionAsync();
    void saveAuthSession(const AuthSession& session);
    void clearAuthSession();

    QCoro::Task<QString> loadDeviceIdAsync();
    void saveDeviceId(const QString& deviceId);

    QCoro::Task<QJsonArray> loadDiscoveredServersAsync();
    void saveDiscoveredServers(const QJsonArray& servers);
    QCoro::Task<QJsonObject> loadHomePayloadAsync(const QString& key, int schemaVersion);
    void saveHomePayload(const QString& key, int schemaVersion, const QJsonObject& payload);
    void invalidateHomePayloads();

    QCoro::Task<QString> loadSettingAsync(const QString& key, const QString& defaultValue = {});
    void saveSetting(const QString& key, const QString& value);

    QCoro::Task<int> schemaVersionAsync();
    QCoro::Task<QByteArray> loadCacheEntryAsync(const QString& nameSpace, const QString& key, qint64 maxAgeMs = -1);
    void saveCacheEntry(const QString& nameSpace, const QString& key, const QByteArray& value, qint64 ttlMs = 0);
    void invalidateCacheNamespace(const QString& nameSpace);
    void evictCacheEntries(int maximumEntries);

private:
    template <typename Callback> void invokeOnWorkerAsync(Callback&& callback)
    {
        QMetaObject::invokeMethod(m_worker, std::forward<Callback>(callback), Qt::QueuedConnection);
    }

    QThread m_thread;
    DatabaseWorker *m_worker = nullptr;
};

} // namespace JellyfinNative
