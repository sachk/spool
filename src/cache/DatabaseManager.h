#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QByteArray>
#include <QObject>
#include <QString>
#include <QThread>
#include <QVariant>

#include "../common/JellyfinTypes.h"

#include <functional>

namespace JellyfinNative {

class DatabaseWorker;

class DatabaseManager final : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager() override;

    bool initialize(const QString &databasePath);
    void shutdown();

    QString loadLastServerUrl();
    QString loadLastUsername();
    void saveLoginHints(const QString &serverUrl, const QString &username);
    
    AuthSession loadAuthSession();
    void saveAuthSession(const AuthSession &session);
    void clearAuthSession();

    QString loadDeviceId();
    void saveDeviceId(const QString &deviceId);

    QJsonArray loadDiscoveredServers();
    void saveDiscoveredServers(const QJsonArray &servers);
    QJsonObject loadHomePayload(const QString &key, int schemaVersion);
    void saveHomePayload(const QString &key, int schemaVersion,
                         const QJsonObject &payload);
    void invalidateHomePayloads();

    QString loadSetting(const QString &key, const QString &defaultValue = {});
    void saveSetting(const QString &key, const QString &value);

    int schemaVersion();
    QByteArray loadCacheEntry(const QString &nameSpace, const QString &key,
                              qint64 maxAgeMs = -1);
    void saveCacheEntry(const QString &nameSpace, const QString &key,
                        const QByteArray &value, qint64 ttlMs = 0);
    void invalidateCacheNamespace(const QString &nameSpace);
    void evictCacheEntries(int maximumEntries);

private:
    QVariant invokeOnWorker(const std::function<QVariant()> &callback);
    void invokeOnWorkerAsync(const std::function<void()> &callback);

    QThread m_thread;
    DatabaseWorker *m_worker = nullptr;
};

} // namespace JellyfinNative
