#pragma once

#include <QQmlNetworkAccessManagerFactory>

#include <QString>

class QNetworkAccessManager;

namespace JellyfinNative {

class QmlNetworkAccessManagerFactory final : public QQmlNetworkAccessManagerFactory
{
public:
    explicit QmlNetworkAccessManagerFactory(QString cacheDirectory, qint64 maximumCacheSize);

    QNetworkAccessManager *create(QObject *parent) override;

private:
    QString m_cacheDirectory;
    qint64 m_maximumCacheSize = 0;
};

} // namespace JellyfinNative
