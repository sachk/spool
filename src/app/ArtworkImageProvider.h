#pragma once

#include <QCache>
#include <QByteArray>
#include <QMutex>
#include <QQuickImageProvider>
#include <QThreadPool>

#include <memory>

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

class ArtworkImageProvider final : public QQuickAsyncImageProvider
{
public:
    ArtworkImageProvider(QString cacheDirectory, qint64 networkCacheBytes,
                         int byteCacheBytes, int decodeThreads);
    ~ArtworkImageProvider() override;

    QQuickImageResponse *requestImageResponse(const QString &id,
                                              const QSize &requestedSize) override;

private:
    QString m_cacheDirectory;
    qint64 m_networkCacheBytes = 0;
    std::shared_ptr<ArtworkByteCache> m_byteCache;
    QThreadPool m_decodePool;
};

} // namespace JellyfinNative
