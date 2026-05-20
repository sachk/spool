#pragma once

#include <QJsonArray>
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

    QJsonArray loadLibraries();
    void saveLibraries(const QJsonArray &libraries);

    QJsonArray loadMovies(const QString &libraryId);
    void saveMovies(const QString &libraryId, const QJsonArray &movies);

    bool loadNightModeEnabled();
    void saveNightModeEnabled(bool enabled);
    int loadAudioDelayMs();
    void saveAudioDelayMs(int delayMs);
    QString loadAudioOutputMode();
    void saveAudioOutputMode(const QString &mode);
    QString loadSetting(const QString &key, const QString &defaultValue = {});
    void saveSetting(const QString &key, const QString &value);

private:
    QVariant invokeOnWorker(const std::function<QVariant()> &callback);
    void invokeOnWorkerAsync(const std::function<void()> &callback);

    QThread m_thread;
    DatabaseWorker *m_worker = nullptr;
};

} // namespace JellyfinNative
