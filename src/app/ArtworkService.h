#pragma once

#include "../common/JellyfinTypes.h"
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
#include <QTimer>
#include <QUrl>
#include <QVariant>

#include <functional>
#include <memory>

class QQuickImageResponse;

namespace JellyfinNative {

class ArtworkByteCache final {
public:
    explicit ArtworkByteCache(int maximumBytes);

    QByteArray get(const QString& key);
    void insert(const QString& key, QByteArray bytes);
    void clear();

private:
    QMutex m_mutex;
    QCache<QString, QByteArray> m_cache;
};

class ArtworkFetchWorker;
class ArtworkImageResponse;

class ArtworkService final : public QObject, public ArtworkPrefetcher {
    Q_OBJECT
public:
    ArtworkService(QString cacheDirectory, qint64 networkCacheBytes, int byteCacheBytes, int decodeThreads,
        QObject *parent = nullptr);
    ~ArtworkService() override;

    Q_INVOKABLE QString url(const QVariant& item, const QString& kind, int width = 0) const;
    QString itemUrl(const MovieItem& item, bool landscape, int width = 0) const override;
    void setServerUrl(QString serverUrl);
    void setUiWidth(int width);
    QQuickImageResponse *requestImageResponse(const QString& id, const QSize& requestedSize);
    void prefetch(const QStringList& urls) override;
    void cancelPrefetches() override;
    void configurePrefetch(int maxConcurrent) override;
    void releaseMemory(bool aggressive);
    void setAuthorizationHeader(QString header);

    int requestImage(QUrl url, QSize requestedSize, ArtworkImageResponse *response);
    void cancelRequest(int requestId);
    void handleRenderFetched(
        int requestId, QString key, QByteArray bytes, QString error, bool diskCache, qint64 queueNs, qint64 fetchNs);
    void handlePrefetched(QString key, QByteArray bytes);

private:
    struct Timing {
        bool memoryCache = false;
        bool diskCache = false;
        bool network = false;
        bool cancelled = false;
        qint64 queueNs = 0;
        qint64 fetchNs = 0;
        qint64 decodeQueueNs = 0;
        qint64 decodeNs = 0;
        qint64 totalNs = 0;
        QSize requestedSize;
        QSize resultSize;
    };
    void startDecode(ArtworkImageResponse *response, QByteArray bytes, Timing timing);
    void finishTiming(Timing timing);
    void flushTimingBatch();
    void invokeWorker(std::function<void(ArtworkFetchWorker *)> call);
    QString movieUrl(const MovieItem& item, const QString& kind, int width) const;
    QString buildUrl(const QString& itemId, const QString& tag, const QString& imageType, int maxWidth, int quality,
        const QString& format = QStringLiteral("webp"), int fillWidth = 0, int fillHeight = 0) const;

    QString m_cacheDirectory;
    QString m_serverUrl;
    int m_uiWidth = 1920;
    qint64 m_networkCacheBytes = 0;
    std::shared_ptr<ArtworkByteCache> m_byteCache;
    QThreadPool m_decodePool;
    QThread m_workerThread;
    ArtworkFetchWorker *m_worker = nullptr;
    QHash<int, QPointer<ArtworkImageResponse>> m_responses;
    QHash<int, qint64> m_requestStarts;
    QList<Timing> m_timingBatch;
    QTimer m_timingBatchTimer;
    int m_nextRequestId = 1;
};

} // namespace JellyfinNative
