#pragma once

#include <QJsonArray>
#include <QObject>
#include <QString>
#include <QThread>
#include <QVariant>

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

    QJsonArray loadDiscoveredServers();
    void saveDiscoveredServers(const QJsonArray &servers);

    QJsonArray loadLibraries();
    void saveLibraries(const QJsonArray &libraries);

    QJsonArray loadMovies(const QString &libraryId);
    void saveMovies(const QString &libraryId, const QJsonArray &movies);

private:
    QVariant invokeOnWorker(const std::function<QVariant()> &callback);
    void invokeOnWorkerAsync(const std::function<void()> &callback);

    QThread m_thread;
    DatabaseWorker *m_worker = nullptr;
};

} // namespace JellyfinNative
