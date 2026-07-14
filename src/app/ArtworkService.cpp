#include "ArtworkService.h"
#include "ArtworkImageProvider.h"

#include <QBuffer>
#include <QDebug>
#include <QDir>
#include <QImageReader>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQueue>
#include <QQuickTextureFactory>
#include <QRunnable>
#include <QSet>
#include <QUrlQuery>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <utility>

namespace JellyfinNative {

namespace {

    constexpr int kRenderConcurrency = 6;

    QSize decodeSizeForRequest(const QSize& sourceSize, const QSize& requestedSize)
    {
        if (!sourceSize.isValid() || !requestedSize.isValid() || requestedSize.width() <= 0
            || requestedSize.height() <= 0)
            return {};
        return sourceSize.scaled(requestedSize, Qt::KeepAspectRatioByExpanding);
    }

#if defined(JELLYFIN_ARTWORK_ASPECT_DIAGNOSTICS)
    bool shouldLogAspectDiagnostic(const QSize& source, const QSize& requested, const QSize& decoded)
    {
        if (!source.isValid() || !requested.isValid() || !decoded.isValid() || source.width() <= 0
            || source.height() <= 0 || requested.width() <= 0 || requested.height() <= 0)
            return false;

        const double sourceAspect = static_cast<double>(source.width()) / source.height();
        const double requestedAspect = static_cast<double>(requested.width()) / requested.height();
        if (std::abs(sourceAspect - requestedAspect) <= 0.02)
            return false;

        if (requested.width() <= 4 || requested.height() <= 4)
            return true;

        const double horizontalOverscan = static_cast<double>(decoded.width()) / requested.width();
        const double verticalOverscan = static_cast<double>(decoded.height()) / requested.height();
        return std::max(horizontalOverscan, verticalOverscan) > 1.35;
    }
#endif

    QImage decodeArtwork(const QByteArray& bytes, const QSize& requestedSize, QString *error)
    {
        QBuffer buffer;
        buffer.setData(bytes);
        if (!buffer.open(QIODevice::ReadOnly)) {
            *error = QStringLiteral("Could not open artwork buffer");
            return {};
        }

        QImageReader reader(&buffer);
        reader.setAutoTransform(false);
        const QSize scaledSize = decodeSizeForRequest(reader.size(), requestedSize);
        if (scaledSize.isValid()) {
#if defined(JELLYFIN_ARTWORK_ASPECT_DIAGNOSTICS)
            if (shouldLogAspectDiagnostic(reader.size(), requestedSize, scaledSize)) {
                qWarning() << "artwork: aspect-preserving decode"
                           << "source=" << reader.size() << "requested=" << requestedSize << "decode=" << scaledSize;
            }
#endif
            reader.setScaledSize(scaledSize);
        }
        QImage image = reader.read();
        if (image.isNull())
            *error = reader.errorString();
        return image;
    }

    QString cacheKeyForUrl(const QUrl& url)
    {
        return url.toString(QUrl::FullyEncoded);
    }

} // namespace

class ArtworkFetchWorker final : public QObject {
public:
    ArtworkFetchWorker(QString cacheDirectory, qint64 networkCacheBytes, ArtworkService *service)
        : m_cacheDirectory(std::move(cacheDirectory))
        , m_networkCacheBytes(networkCacheBytes)
        , m_service(service)
    {
    }

    void fetchRender(int requestId, QUrl url)
    {
        ensureNetwork();
        if (!m_network || !url.isValid() || url.scheme().isEmpty()) {
            deliverRender(requestId, cacheKeyForUrl(url), {}, QStringLiteral("Invalid artwork URL"));
            return;
        }

        m_renderQueue.enqueue({ requestId, std::move(url) });
        drainRender();
    }

    void cancelRender(int requestId)
    {
        for (auto it = m_renderQueue.begin(); it != m_renderQueue.end();) {
            if (it->requestId == requestId)
                it = m_renderQueue.erase(it);
            else
                ++it;
        }

        QNetworkReply *reply = m_renderReplies.take(requestId);
        if (reply)
            reply->abort();
        drainPrefetch();
    }

    void prefetch(QStringList urls)
    {
        ensureNetwork();
        for (const QString& urlString : urls) {
            const QUrl url(urlString);
            const QString key = cacheKeyForUrl(url);
            if (key.isEmpty() || m_prefetchSeen.contains(key))
                continue;
            m_prefetchSeen.insert(key);
            m_prefetchQueue.enqueue(url);
        }
        drainPrefetch();
    }

    void cancelPrefetches()
    {
        m_prefetchQueue.clear();
        m_prefetchSeen.clear();
        const auto replies = m_prefetchReplies;
        for (QNetworkReply *reply : replies) {
            if (reply)
                reply->abort();
        }
    }

    void setPrefetchConcurrency(int maxConcurrent)
    {
        m_prefetchMaxConcurrent = std::max(1, maxConcurrent);
        drainPrefetch();
    }

    void setAuthorizationHeader(QString header)
    {
        m_authorizationHeader = header.toUtf8();
    }

    void cancelAll()
    {
        m_renderQueue.clear();
        const auto renderReplies = m_renderReplies;
        for (QNetworkReply *reply : renderReplies) {
            if (reply)
                reply->abort();
        }
        cancelPrefetches();
    }

private:
    struct RenderRequest {
        int requestId = 0;
        QUrl url;
    };

    void ensureNetwork()
    {
        if (m_network)
            return;
        m_network = new QNetworkAccessManager(this);
        if (!m_cacheDirectory.isEmpty() && m_networkCacheBytes > 0) {
            QDir().mkpath(m_cacheDirectory);
            auto *diskCache = new QNetworkDiskCache(m_network);
            diskCache->setCacheDirectory(m_cacheDirectory);
            diskCache->setMaximumCacheSize(m_networkCacheBytes);
            m_network->setCache(diskCache);
        }
    }

    QNetworkRequest cachedRequest(const QUrl& url) const
    {
        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
        if (!m_authorizationHeader.isEmpty())
            request.setRawHeader("Authorization", m_authorizationHeader);
        return request;
    }

    void drainRender()
    {
        ensureNetwork();
        while (m_network && m_renderReplies.size() < kRenderConcurrency && !m_renderQueue.isEmpty()) {
            const RenderRequest request = m_renderQueue.dequeue();
            QNetworkReply *reply = m_network->get(cachedRequest(request.url));
            m_renderReplies.insert(request.requestId, reply);
            const QString key = cacheKeyForUrl(request.url);
            connect(reply, &QNetworkReply::finished, this, [this, reply, requestId = request.requestId, key]() {
                m_renderReplies.remove(requestId);
                finishReply(reply, [this, requestId, key](QByteArray bytes, QString error) {
                    deliverRender(requestId, key, std::move(bytes), std::move(error));
                });
                drainRender();
                drainPrefetch();
            });
        }
    }

    void drainPrefetch()
    {
        ensureNetwork();
        if (!m_network || !m_renderQueue.isEmpty() || !m_renderReplies.isEmpty())
            return;
        while (m_prefetchReplies.size() < m_prefetchMaxConcurrent && !m_prefetchQueue.isEmpty()) {
            const QUrl url = m_prefetchQueue.dequeue();
            const QString key = cacheKeyForUrl(url);
            QNetworkReply *reply = m_network->get(cachedRequest(url));
            m_prefetchReplies.insert(reply);
            connect(reply, &QNetworkReply::finished, this, [this, reply, key]() {
                m_prefetchReplies.remove(reply);
                m_prefetchSeen.remove(key);
                finishReply(reply, [this, key](QByteArray bytes, QString error) {
                    if (error.isEmpty() && !bytes.isEmpty())
                        deliverPrefetch(key, std::move(bytes));
                });
                drainPrefetch();
            });
        }
    }

    template <typename Callback> void finishReply(QNetworkReply *reply, Callback callback)
    {
        if (!reply) {
            callback({}, QStringLiteral("Network reply disappeared"));
            return;
        }

        const bool ok = reply->error() == QNetworkReply::NoError;
        QByteArray bytes = ok && reply->isReadable() ? reply->readAll() : QByteArray();
        const QString error = ok ? QString() : reply->errorString();
        reply->deleteLater();
        if (ok && bytes.isEmpty())
            callback({}, QStringLiteral("Artwork response was empty"));
        else
            callback(std::move(bytes), error);
    }

    void deliverRender(int requestId, QString key, QByteArray bytes, QString error)
    {
        QPointer<ArtworkService> service = m_service;
        QMetaObject::invokeMethod(
            m_service,
            [service, requestId, key = std::move(key), bytes = std::move(bytes), error = std::move(error)]() mutable {
                if (service)
                    service->handleRenderFetched(requestId, std::move(key), std::move(bytes), std::move(error));
            },
            Qt::QueuedConnection);
    }

    void deliverPrefetch(QString key, QByteArray bytes)
    {
        QPointer<ArtworkService> service = m_service;
        QMetaObject::invokeMethod(
            m_service,
            [service, key = std::move(key), bytes = std::move(bytes)]() mutable {
                if (service)
                    service->handlePrefetched(std::move(key), std::move(bytes));
            },
            Qt::QueuedConnection);
    }

    QString m_cacheDirectory;
    qint64 m_networkCacheBytes = 0;
    QPointer<ArtworkService> m_service;
    QByteArray m_authorizationHeader;
    QNetworkAccessManager *m_network = nullptr;
    QQueue<RenderRequest> m_renderQueue;
    QHash<int, QNetworkReply *> m_renderReplies;
    QQueue<QUrl> m_prefetchQueue;
    QSet<QString> m_prefetchSeen;
    QSet<QNetworkReply *> m_prefetchReplies;
    int m_prefetchMaxConcurrent = 3;
};

class ArtworkImageResponse final : public QQuickImageResponse {
public:
    ArtworkImageResponse(ArtworkService *service, QUrl url, QSize requestedSize)
        : m_service(service)
        , m_requestedSize(std::move(requestedSize))
    {
        if (!m_service) {
            finish({}, QStringLiteral("Artwork service unavailable"));
            return;
        }
        m_requestId = m_service->requestImage(std::move(url), m_requestedSize, this);
    }

    ~ArtworkImageResponse() override = default;

    QQuickTextureFactory *textureFactory() const override
    {
        return m_image.isNull() ? nullptr : QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    QString errorString() const override
    {
        return m_error;
    }

    void cancel() override
    {
        m_cancelled.store(true);
        if (m_service && m_requestId > 0)
            m_service->cancelRequest(m_requestId);
    }

    bool cancelled() const
    {
        return m_cancelled.load();
    }

    QSize requestedSize() const
    {
        return m_requestedSize;
    }

    void finish(QImage image, QString error)
    {
        if (m_finished.exchange(true))
            return;
        if (m_cancelled.load() && error.isEmpty()) {
            m_error = QStringLiteral("Cancelled");
        } else if (image.isNull() && !error.isEmpty()) {
            qInfo() << "artwork: using fallback:" << error;
            m_image = QImage(1, 1, QImage::Format_ARGB32_Premultiplied);
            m_image.fill(Qt::transparent);
        } else {
            m_image = std::move(image);
            m_error = std::move(error);
        }
        emit finished();
    }

private:
    QPointer<ArtworkService> m_service;
    QSize m_requestedSize;
    int m_requestId = 0;
    QImage m_image;
    QString m_error;
    std::atomic_bool m_cancelled = false;
    std::atomic_bool m_finished = false;
};

ArtworkByteCache::ArtworkByteCache(int maximumBytes)
{
    m_cache.setMaxCost(std::max(1, maximumBytes));
}

QByteArray ArtworkByteCache::get(const QString& key)
{
    QMutexLocker locker(&m_mutex);
    const QByteArray *bytes = m_cache.object(key);
    return bytes ? *bytes : QByteArray();
}

void ArtworkByteCache::insert(const QString& key, QByteArray bytes)
{
    if (bytes.isEmpty())
        return;
    auto *stored = new QByteArray(std::move(bytes));
    const int cost = stored->size();
    QMutexLocker locker(&m_mutex);
    m_cache.insert(key, stored, cost);
}

void ArtworkByteCache::clear()
{
    QMutexLocker locker(&m_mutex);
    m_cache.clear();
}

ArtworkService::ArtworkService(
    QString cacheDirectory, qint64 networkCacheBytes, int byteCacheBytes, int decodeThreads, QObject *parent)
    : QObject(parent)
    , m_cacheDirectory(std::move(cacheDirectory))
    , m_networkCacheBytes(networkCacheBytes)
    , m_byteCache(std::make_shared<ArtworkByteCache>(byteCacheBytes))
{
    m_decodePool.setMaxThreadCount(std::max(1, decodeThreads));
    m_decodePool.setExpiryTimeout(30000);

    m_worker = new ArtworkFetchWorker(m_cacheDirectory, m_networkCacheBytes, this);
    m_worker->moveToThread(&m_workerThread);
    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_workerThread.start();
}

ArtworkService::~ArtworkService()
{
    invokeWorker([](ArtworkFetchWorker *worker) { worker->cancelAll(); });
    m_workerThread.quit();
    m_workerThread.wait();
    m_decodePool.waitForDone();
}

QString ArtworkService::url(const QVariant& value, const QString& kind, int width) const
{
    if (value.canConvert<MovieItem>())
        return movieUrl(value.value<MovieItem>(), kind, width);
    if (value.canConvert<LibraryItem>()) {
        const LibraryItem item = value.value<LibraryItem>();
        return buildUrl(item.id, item.imageTag, QStringLiteral("Primary"), width > 0 ? width : 280, 75);
    }
    if (value.canConvert<PersonItem>()) {
        const PersonItem item = value.value<PersonItem>();
        return buildUrl(item.id, item.imageTag, QStringLiteral("Primary"), width > 0 ? width : 360, 80);
    }
    return {};
}

QString ArtworkService::itemUrl(const MovieItem& item, bool landscape, int width) const
{
    return movieUrl(item, landscape ? QStringLiteral("landscape") : QStringLiteral("poster"), width);
}

void ArtworkService::setServerUrl(QString serverUrl)
{
    while (serverUrl.endsWith(QLatin1Char('/')))
        serverUrl.chop(1);
    m_serverUrl = std::move(serverUrl);
}

void ArtworkService::setUiWidth(int width)
{
    if (width > 0)
        m_uiWidth = width;
}

QString ArtworkService::movieUrl(const MovieItem& item, const QString& kind, int width) const
{
    QString itemId = item.id;
    QString tag = item.posterTag;
    QString imageType = QStringLiteral("Primary");
    QString format = QStringLiteral("webp");
    int maxWidth = width > 0 ? width : 280;
    int quality = 75;
    int fillWidth = 0;
    int fillHeight = 0;

    if (kind == QStringLiteral("seriesPoster")) {
        if (!item.seriesId.isEmpty() && !item.seriesPrimaryImageTag.isEmpty()) {
            itemId = item.seriesId;
            tag = item.seriesPrimaryImageTag;
        }
        maxWidth = width > 0 ? width : 360;
        quality = 80;
    } else if (kind == QStringLiteral("landscape")) {
        if (!item.thumbTag.isEmpty()) {
            tag = item.thumbTag;
            imageType = QStringLiteral("Thumb");
        } else if (!item.backdropTag.isEmpty()) {
            tag = item.backdropTag;
            imageType = QStringLiteral("Backdrop");
        } else if (tag.isEmpty() && !item.seriesId.isEmpty()) {
            itemId = item.seriesId;
            tag = item.seriesPrimaryImageTag;
        }
        maxWidth = width > 0 ? width : (m_uiWidth >= 3000 ? 640 : 400);
        quality = m_uiWidth >= 3000 ? 70 : 68;
        if (imageType != QStringLiteral("Primary")) {
            fillWidth = maxWidth;
            fillHeight = maxWidth * 9 / 16;
        }
    } else if (kind == QStringLiteral("backdrop")) {
        if (!item.backdropTag.isEmpty()) {
            tag = item.backdropTag;
            imageType = QStringLiteral("Backdrop");
        } else if (!item.thumbTag.isEmpty()) {
            tag = item.thumbTag;
            imageType = QStringLiteral("Thumb");
        }
        maxWidth = width > 0 ? width : 1920;
        quality = 82;
    } else if (kind == QStringLiteral("logo")) {
        tag = item.logoTag;
        imageType = QStringLiteral("Logo");
        format = QStringLiteral("png");
        maxWidth = width > 0 ? width : 720;
        quality = 90;
    } else if (kind == QStringLiteral("banner")) {
        tag = item.bannerTag;
        imageType = QStringLiteral("Banner");
        maxWidth = width > 0 ? width : 1000;
        quality = 86;
    } else if (kind == QStringLiteral("thumb")) {
        tag = item.thumbTag;
        imageType = QStringLiteral("Thumb");
        maxWidth = width > 0 ? width : 720;
        quality = 82;
    }
    return buildUrl(itemId, tag, imageType, maxWidth, quality, format, fillWidth, fillHeight);
}

QString ArtworkService::buildUrl(const QString& itemId, const QString& tag, const QString& imageType, int maxWidth,
    int quality, const QString& format, int fillWidth, int fillHeight) const
{
    if (m_serverUrl.isEmpty() || itemId.isEmpty() || tag.isEmpty() || imageType.isEmpty())
        return {};
    QUrl url = serverUrlWithPath(m_serverUrl, { QStringLiteral("Items"), itemId, QStringLiteral("Images"), imageType });
    QUrlQuery query;
    if (fillWidth > 0 && fillHeight > 0) {
        query.addQueryItem(QStringLiteral("fillWidth"), QString::number(fillWidth));
        query.addQueryItem(QStringLiteral("fillHeight"), QString::number(fillHeight));
    } else {
        query.addQueryItem(QStringLiteral("maxWidth"), QString::number(maxWidth));
    }
    query.addQueryItem(QStringLiteral("quality"), QString::number(quality));
    query.addQueryItem(QStringLiteral("format"), format);
    query.addQueryItem(QStringLiteral("tag"), tag);
    url.setQuery(query);
    return url.toString(QUrl::FullyEncoded);
}

QQuickImageResponse *ArtworkService::requestImageResponse(const QString& id, const QSize& requestedSize)
{
    const QString urlString = QUrl::fromPercentEncoding(id.toUtf8());
    return new ArtworkImageResponse(this, QUrl(urlString), requestedSize);
}

void ArtworkService::prefetch(const QStringList& urls)
{
    QStringList uncached;
    uncached.reserve(urls.size());
    for (const QString& url : urls) {
        const QString key = cacheKeyForUrl(QUrl(url));
        if (key.isEmpty() || !m_byteCache || !m_byteCache->get(key).isEmpty())
            continue;
        uncached.push_back(url);
    }
    if (uncached.isEmpty())
        return;
    invokeWorker(
        [urls = std::move(uncached)](ArtworkFetchWorker *worker) mutable { worker->prefetch(std::move(urls)); });
}

void ArtworkService::setAuthorizationHeader(QString header)
{
    invokeWorker([header = std::move(header)](
                     ArtworkFetchWorker *worker) mutable { worker->setAuthorizationHeader(std::move(header)); });
}

void ArtworkService::cancelPrefetches()
{
    invokeWorker([](ArtworkFetchWorker *worker) { worker->cancelPrefetches(); });
}

void ArtworkService::configurePrefetch(int maxConcurrent)
{
    invokeWorker([maxConcurrent](ArtworkFetchWorker *worker) { worker->setPrefetchConcurrency(maxConcurrent); });
}

void ArtworkService::releaseMemory(bool aggressive)
{
    cancelPrefetches();
    if (m_byteCache)
        m_byteCache->clear();
    if (aggressive)
        m_decodePool.clear();
}

int ArtworkService::requestImage(QUrl url, QSize requestedSize, ArtworkImageResponse *response)
{
    if (!url.isValid() || url.scheme().isEmpty()) {
        if (response)
            response->finish({}, QStringLiteral("Invalid artwork URL"));
        return 0;
    }

    const QString key = cacheKeyForUrl(url);
    const QByteArray cached = m_byteCache ? m_byteCache->get(key) : QByteArray();
    if (!cached.isEmpty()) {
        startDecode(response, cached, requestedSize);
        return 0;
    }

    const int requestId = m_nextRequestId++;
    m_responses.insert(requestId, response);
    invokeWorker([requestId, url = std::move(url)](
                     ArtworkFetchWorker *worker) mutable { worker->fetchRender(requestId, std::move(url)); });
    return requestId;
}

void ArtworkService::cancelRequest(int requestId)
{
    if (requestId <= 0)
        return;
    m_responses.remove(requestId);
    invokeWorker([requestId](ArtworkFetchWorker *worker) { worker->cancelRender(requestId); });
}

void ArtworkService::handleRenderFetched(int requestId, QString key, QByteArray bytes, QString error)
{
    const QPointer<ArtworkImageResponse> response = m_responses.take(requestId);
    if (!response)
        return;
    if (!error.isEmpty() || bytes.isEmpty()) {
        response->finish({}, std::move(error));
        return;
    }
    if (m_byteCache)
        m_byteCache->insert(key, bytes);
    startDecode(response, std::move(bytes), response->requestedSize());
}

void ArtworkService::handlePrefetched(QString key, QByteArray bytes)
{
    if (m_byteCache)
        m_byteCache->insert(key, std::move(bytes));
}

void ArtworkService::startDecode(ArtworkImageResponse *response, QByteArray bytes, QSize requestedSize)
{
    if (!response) {
        return;
    }
    const QPointer<ArtworkImageResponse> self(response);
    m_decodePool.start(QRunnable::create([self, bytes = std::move(bytes), requestedSize]() {
        if (!self || self->cancelled())
            return;
        QString error;
        QImage image = decodeArtwork(bytes, requestedSize, &error);
        if (!self || self->cancelled())
            return;
        QMetaObject::invokeMethod(
            self,
            [self, image = std::move(image), error = std::move(error)]() mutable {
                if (self)
                    self->finish(std::move(image), std::move(error));
            },
            Qt::QueuedConnection);
    }));
}

void ArtworkService::invokeWorker(std::function<void(ArtworkFetchWorker *)> call)
{
    if (!m_worker)
        return;
    QPointer<ArtworkFetchWorker> worker = m_worker;
    QMetaObject::invokeMethod(
        m_worker,
        [worker, call = std::move(call)]() mutable {
            if (worker)
                call(worker);
        },
        Qt::QueuedConnection);
}

ArtworkImageProvider::ArtworkImageProvider(ArtworkService *service)
    : m_service(service)
{
}

QQuickImageResponse *ArtworkImageProvider::requestImageResponse(const QString& id, const QSize& requestedSize)
{
    return m_service ? m_service->requestImageResponse(id, requestedSize)
                     : new ArtworkImageResponse(nullptr, {}, requestedSize);
}

} // namespace JellyfinNative
