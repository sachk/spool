#pragma once

#include <QCryptographicHash>
#include <QNetworkReply>
#include <QSettings>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>
#include <QUrl>

#if !QT_CONFIG(ssl)
#error "Jellyfin Native requires a Qt Network build with TLS support"
#endif

namespace JellyfinNative::TlsTrust {

inline QSslCertificate peerCertificate(QNetworkReply *reply, const QList<QSslError>& errors)
{
    if (reply) {
        const QSslCertificate certificate = reply->sslConfiguration().peerCertificate();
        if (!certificate.isNull())
            return certificate;
    }
    for (const QSslError& error : errors) {
        if (!error.certificate().isNull())
            return error.certificate();
    }
    return QSslCertificate();
}

inline QByteArray fingerprint(const QSslCertificate& certificate)
{
    return certificate.digest(QCryptographicHash::Sha256).toHex();
}

inline QString displayFingerprint(const QSslCertificate& certificate)
{
    const QByteArray compact = fingerprint(certificate).toUpper();
    QStringList groups;
    for (qsizetype i = 0; i < compact.size(); i += 2)
        groups.push_back(QString::fromLatin1(compact.mid(i, 2)));
    return groups.join(QLatin1Char(':'));
}

inline QString endpointKey(const QUrl& url)
{
    QString scheme = url.scheme().toLower();
    if (scheme == QStringLiteral("wss"))
        scheme = QStringLiteral("https");
    else if (scheme == QStringLiteral("ws"))
        scheme = QStringLiteral("http");
    const int port = url.port(scheme == QStringLiteral("https") ? 443 : -1);
    const QByteArray endpoint
        = (scheme + QStringLiteral("://") + url.host().toLower() + QLatin1Char(':') + QString::number(port)).toUtf8();
    return QStringLiteral("tls/trusted/")
        + QString::fromLatin1(QCryptographicHash::hash(endpoint, QCryptographicHash::Sha256).toHex());
}

inline bool isTrusted(const QUrl& url, const QSslCertificate& certificate)
{
    const QByteArray expected
        = QSettings().value(endpointKey(url) + QStringLiteral("/fingerprint")).toByteArray().toLower();
    return !expected.isEmpty() && expected == fingerprint(certificate);
}

inline void remember(const QUrl& url, const QSslCertificate& certificate)
{
    QSettings settings;
    const QString key = endpointKey(url);
    settings.setValue(key + QStringLiteral("/fingerprint"), fingerprint(certificate));
    settings.setValue(key + QStringLiteral("/certificatePem"), certificate.toPem());
}

inline QSslCertificate trustedCertificate(const QUrl& url)
{
    QSettings settings;
    const QString key = endpointKey(url);
    const QSslCertificate certificate(settings.value(key + QStringLiteral("/certificatePem")).toByteArray(), QSsl::Pem);
    return !certificate.isNull() && isTrusted(url, certificate) ? certificate : QSslCertificate();
}

} // namespace JellyfinNative::TlsTrust
