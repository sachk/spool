#pragma once

#include "ArtworkPrefetcher.h"

#include <QByteArray>
#include <QCache>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QThreadPool>
#include <QUrl>

#include <functional>
#include <memory>

class QQuickImageResponse;

namespace JellyfinNative {

class ArtworkByteCache final
{
public:
    explicit ArtworkByteCache(int maximumBytes);

    QByteArray get(const QString &key);
    void insert(const QString &key, QByteArray bytes);

private:
    QMutex m_mutex;
    QCache<QString, QByteArray> m_cache;
};

class ArtworkFetchWorker;
class ArtworkImageResponse;

class ArtworkService final : public QObject, public ArtworkPrefetcher
{
public:
    ArtworkService(QString cacheDirectory, qint64 networkCacheBytes,
                   int byteCacheBytes, int decodeThreads,
                   QObject *parent = nullptr);
    ~ArtworkService() override;

    QQuickImageResponse *requestImageResponse(const QString &id,
                                              const QSize &requestedSize);
    void prefetch(const QStringList &urls) override;
    void cancelPrefetches() override;
    void configurePrefetch(int maxConcurrent) override;

    int requestImage(QUrl url, QSize requestedSize,
                     ArtworkImageResponse *response);
    void cancelRequest(int requestId);
    void handleRenderFetched(int requestId, QString key, QByteArray bytes,
                             QString error);
    void handlePrefetched(QString key, QByteArray bytes);

private:
    void startDecode(ArtworkImageResponse *response, QByteArray bytes,
                     QSize requestedSize);
    void invokeWorker(std::function<void(ArtworkFetchWorker *)> call);

    QString m_cacheDirectory;
    qint64 m_networkCacheBytes = 0;
    std::shared_ptr<ArtworkByteCache> m_byteCache;
    QThreadPool m_decodePool;
    QThread m_workerThread;
    ArtworkFetchWorker *m_worker = nullptr;
    QHash<int, QPointer<ArtworkImageResponse>> m_responses;
    int m_nextRequestId = 1;
};

} // namespace JellyfinNative
