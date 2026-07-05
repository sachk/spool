#include "ArtworkImageProvider.h"

#include <QBuffer>
#include <QDir>
#include <QImageReader>
#include <QDebug>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRunnable>
#include <QQuickTextureFactory>
#include <QUrl>

#include <atomic>
#include <cmath>
#include <algorithm>
#include <utility>

namespace JellyfinNative {

namespace {
QSize decodeSizeForRequest(const QSize &sourceSize, const QSize &requestedSize)
{
    if (!sourceSize.isValid() || !requestedSize.isValid() ||
        requestedSize.width() <= 0 || requestedSize.height() <= 0)
        return {};
    return sourceSize.scaled(requestedSize, Qt::KeepAspectRatioByExpanding);
}

#if defined(JELLYFIN_ARTWORK_ASPECT_DIAGNOSTICS)
bool shouldLogAspectDiagnostic(const QSize &source, const QSize &requested, const QSize &decoded)
{
    if (!source.isValid() || !requested.isValid() || !decoded.isValid() ||
        source.width() <= 0 || source.height() <= 0 ||
        requested.width() <= 0 || requested.height() <= 0)
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


class ArtworkImageResponse final : public QQuickImageResponse
{
public:
    ArtworkImageResponse(QUrl url, QSize requestedSize, QString cacheDirectory,
                         qint64 networkCacheBytes,
                         std::shared_ptr<ArtworkByteCache> byteCache,
                         QThreadPool *decodePool)
        : m_url(std::move(url))
        , m_requestedSize(requestedSize)
        , m_byteCache(std::move(byteCache))
        , m_decodePool(decodePool)
    {
        if (!m_url.isValid() || m_url.scheme().isEmpty()) {
            finish({}, QStringLiteral("Invalid artwork URL"));
            return;
        }

        const QString key = m_url.toString(QUrl::FullyEncoded);
        const QByteArray cached = m_byteCache ? m_byteCache->get(key) : QByteArray();
        if (!cached.isEmpty()) {
            startDecode(cached);
            return;
        }

        m_network = new QNetworkAccessManager(this);
        if (!cacheDirectory.isEmpty() && networkCacheBytes > 0) {
            QDir().mkpath(cacheDirectory);
            auto *diskCache = new QNetworkDiskCache(m_network);
            diskCache->setCacheDirectory(cacheDirectory);
            diskCache->setMaximumCacheSize(networkCacheBytes);
            m_network->setCache(diskCache);
        }

        QNetworkRequest request(m_url);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                             QNetworkRequest::PreferCache);
        m_reply = m_network->get(request);
        connect(m_reply, &QNetworkReply::finished, this, [this, key]() {
            if (m_cancelled.load()) {
                finish({}, {});
                return;
            }
            const QPointer<QNetworkReply> reply = m_reply;
            if (!reply) {
                finish({}, QStringLiteral("Artwork request disappeared"));
                return;
            }
            if (reply->error() != QNetworkReply::NoError) {
                finish({}, reply->errorString());
                return;
            }
            QByteArray bytes = reply->readAll();
            if (bytes.isEmpty()) {
                finish({}, QStringLiteral("Artwork response was empty"));
                return;
            }
            if (m_byteCache)
                m_byteCache->insert(key, bytes);
            startDecode(bytes);
        });
    }

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
        if (m_reply)
            m_reply->abort();
    }

private:
    static QImage decodeWebp(const QByteArray &bytes, const QSize &requestedSize,
                             QString *error)
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
                           << "source=" << reader.size()
                           << "requested=" << requestedSize
                           << "decode=" << scaledSize;
            }
#endif
            reader.setScaledSize(scaledSize);
        }
        QImage image = reader.read();
        if (image.isNull())
            *error = reader.errorString();
        return image;
    }

    void startDecode(QByteArray bytes)
    {
        if (!m_decodePool) {
            finish({}, QStringLiteral("Artwork decode pool unavailable"));
            return;
        }

        const QSize requestedSize = m_requestedSize;
        const QPointer<ArtworkImageResponse> self(this);
        m_decodePool->start(QRunnable::create([self, bytes = std::move(bytes), requestedSize]() {
            if (!self || self->m_cancelled.load())
                return;
            QString error;
            QImage image = decodeWebp(bytes, requestedSize, &error);
            if (!self || self->m_cancelled.load())
                return;
            QMetaObject::invokeMethod(self, [self, image = std::move(image), error = std::move(error)]() mutable {
                if (!self)
                    return;
                self->finish(std::move(image), std::move(error));
            }, Qt::QueuedConnection);
        }));
    }

    void finish(QImage image, QString error)
    {
        if (m_finished.exchange(true))
            return;
        if (m_cancelled.load() && error.isEmpty()) {
            m_error = QStringLiteral("Cancelled");
        } else {
            m_image = std::move(image);
            m_error = std::move(error);
        }
        emit finished();
    }

    QUrl m_url;
    QSize m_requestedSize;
    std::shared_ptr<ArtworkByteCache> m_byteCache;
    QThreadPool *m_decodePool = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QPointer<QNetworkReply> m_reply;
    QImage m_image;
    QString m_error;
    std::atomic_bool m_cancelled = false;
    std::atomic_bool m_finished = false;
};

} // namespace

ArtworkByteCache::ArtworkByteCache(int maximumBytes)
{
    m_cache.setMaxCost(std::max(1, maximumBytes));
}

QByteArray ArtworkByteCache::get(const QString &key)
{
    QMutexLocker locker(&m_mutex);
    const QByteArray *bytes = m_cache.object(key);
    return bytes ? *bytes : QByteArray();
}

void ArtworkByteCache::insert(const QString &key, QByteArray bytes)
{
    if (bytes.isEmpty())
        return;
    auto *stored = new QByteArray(std::move(bytes));
    const int cost = stored->size();
    QMutexLocker locker(&m_mutex);
    m_cache.insert(key, stored, cost);
}

ArtworkImageProvider::ArtworkImageProvider(QString cacheDirectory,
                                           qint64 networkCacheBytes,
                                           int byteCacheBytes,
                                           int decodeThreads)
    : m_cacheDirectory(std::move(cacheDirectory))
    , m_networkCacheBytes(networkCacheBytes)
    , m_byteCache(std::make_shared<ArtworkByteCache>(byteCacheBytes))
{
    m_decodePool.setMaxThreadCount(std::max(1, decodeThreads));
    m_decodePool.setExpiryTimeout(30000);
}

ArtworkImageProvider::~ArtworkImageProvider()
{
    m_decodePool.waitForDone();
}

QQuickImageResponse *ArtworkImageProvider::requestImageResponse(const QString &id,
                                                                const QSize &requestedSize)
{
    const QString urlString = QUrl::fromPercentEncoding(id.toUtf8());
    return new ArtworkImageResponse(QUrl(urlString), requestedSize, m_cacheDirectory,
                                    m_networkCacheBytes, m_byteCache, &m_decodePool);
}

} // namespace JellyfinNative
