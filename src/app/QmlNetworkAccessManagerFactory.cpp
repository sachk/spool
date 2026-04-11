#include "QmlNetworkAccessManagerFactory.h"

#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>

#include <utility>

namespace JellyfinNative {

QmlNetworkAccessManagerFactory::QmlNetworkAccessManagerFactory(QString cacheDirectory, qint64 maximumCacheSize)
    : m_cacheDirectory(std::move(cacheDirectory))
    , m_maximumCacheSize(maximumCacheSize)
{
}

QNetworkAccessManager *QmlNetworkAccessManagerFactory::create(QObject *parent)
{
    auto *manager = new QNetworkAccessManager(parent);
    auto *diskCache = new QNetworkDiskCache(manager);

    QDir().mkpath(m_cacheDirectory);
    diskCache->setCacheDirectory(m_cacheDirectory);
    diskCache->setMaximumCacheSize(m_maximumCacheSize);
    manager->setCache(diskCache);

    return manager;
}

} // namespace JellyfinNative
