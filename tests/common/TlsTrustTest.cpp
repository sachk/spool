#include "common/TlsTrust.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QSslKey>
#include <QSslSocket>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTimer>

#include <cstdlib>
#include <iostream>

using JellyfinNative::TlsTrustController;

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
}

QByteArray fixture(const char *name)
{
    QFile file(QStringLiteral(TEST_SOURCE_DIR "/tests/fixtures/") + QString::fromLatin1(name));
    require(file.open(QIODevice::ReadOnly), "TLS fixture could not be opened");
    return file.readAll();
}

class LocalTlsServer final : public QTcpServer {
public:
    LocalTlsServer(QSslCertificate certificate, QSslKey key)
        : m_certificate(std::move(certificate))
        , m_key(std::move(key))
    {
    }

protected:
    void incomingConnection(qintptr descriptor) override
    {
        auto *socket = new QSslSocket(this);
        if (!socket->setSocketDescriptor(descriptor)) {
            socket->deleteLater();
            return;
        }
        socket->setLocalCertificate(m_certificate);
        socket->setPrivateKey(m_key);
        connect(socket, &QSslSocket::readyRead, socket, [socket]() {
            socket->readAll();
            socket->write("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
            socket->flush();
            socket->disconnectFromHost();
        });
        connect(socket, &QSslSocket::disconnected, socket, &QObject::deleteLater);
        socket->startServerEncryption();
    }

private:
    QSslCertificate m_certificate;
    QSslKey m_key;
};

struct RequestResult {
    QNetworkReply::NetworkError error = QNetworkReply::UnknownNetworkError;
    QByteArray body;
};

RequestResult request(QNetworkAccessManager& manager, const QUrl& url)
{
    QNetworkReply *reply = manager.get(QNetworkRequest(url));
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(3'000);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timeout.start();
    loop.exec();
    require(timeout.isActive(), "TLS fixture request timed out");
    const RequestResult result { reply->error(), reply->readAll() };
    manager.clearConnectionCache();
    reply->deleteLater();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    return result;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("JellyfinNativeTests"));
    QCoreApplication::setApplicationName(QStringLiteral("TlsTrustTest"));

    QTemporaryDir settingsDirectory;
    require(settingsDirectory.isValid(), "temporary settings directory was not created");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());
    QSettings().clear();

    const QSslCertificate certificate(fixture("tls-test-cert.pem"), QSsl::Pem);
    const QSslKey key(fixture("tls-test-key.pem"), QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
    require(!certificate.isNull() && !key.isNull(), "TLS fixtures did not parse");

    require(TlsTrustController::endpointKey(QUrl(QStringLiteral("https://Server.Example")))
            == TlsTrustController::endpointKey(QUrl(QStringLiteral("wss://server.example:443/socket"))),
        "HTTPS API and secure WebSocket should share one exact endpoint trust decision");
    require(TlsTrustController::endpointKey(QUrl(QStringLiteral("https://server.example")))
            != TlsTrustController::endpointKey(QUrl(QStringLiteral("https://server.example:8443"))),
        "certificate trust must not spread to another port");
    require(TlsTrustController::endpointKey(QUrl(QStringLiteral("https://server.example")))
            != TlsTrustController::endpointKey(QUrl(QStringLiteral("https://other.example"))),
        "certificate trust must not spread to another host");
    require(TlsTrustController::displayFingerprint(certificate).count(QLatin1Char(':')) == 31,
        "SHA-256 fingerprint should be displayed as 32 colon-separated bytes");

    LocalTlsServer server(certificate, key);
    require(server.listen(QHostAddress::LocalHost), "local TLS fixture server did not listen");
    const QUrl url(QStringLiteral("https://localhost:%1/test").arg(server.serverPort()));

    TlsTrustController trust;
    QNetworkAccessManager manager;
    trust.attachNetworkAccessManager(&manager, QStringLiteral("TLS fixture"));

    require(
        request(manager, url).error != QNetworkReply::NoError, "an unknown self-signed certificate should fail closed");
    require(trust.pending(), "an unknown certificate should create a blocking trust decision");
    require(trust.pendingAuthority() == TlsTrustController::authority(url),
        "trust decision should identify the exact authority");
    require(trust.pendingFingerprint() == TlsTrustController::displayFingerprint(certificate),
        "trust decision should expose the peer SHA-256 fingerprint");
    require(!trust.pendingErrors().isEmpty() && trust.pendingSource() == QStringLiteral("TLS fixture"),
        "trust decision should expose the verification error and request source");

    trust.trustOnce();
    const RequestResult once = request(manager, url);
    require(once.error == QNetworkReply::NoError && once.body == QByteArrayLiteral("ok"),
        "Trust Once should allow exactly the next matching request");
    require(request(manager, url).error != QNetworkReply::NoError && trust.pending(),
        "Trust Once should not persist after one matching request");

    trust.remember();
    const RequestResult remembered = request(manager, url);
    require(remembered.error == QNetworkReply::NoError && remembered.body == QByteArrayLiteral("ok"),
        "Remember should allow later requests with the same endpoint fingerprint");
    require(trust.isTrusted(url, certificate), "remembered certificate should be trusted for its exact endpoint");
    require(!trust.isTrusted(QUrl(QStringLiteral("https://localhost:8443")), certificate),
        "remembered certificate should not spread to another port");
    require(trust.rememberedCertificates().size() == 1,
        "remembered certificate management should list the stored endpoint");

    TlsTrustController restored;
    require(restored.isTrusted(url, certificate), "remembered certificate should survive controller reconstruction");
    const QString storedKey
        = restored.rememberedCertificates().constFirst().toMap().value(QStringLiteral("key")).toString();
    restored.removeRemembered(storedKey);
    require(!restored.isTrusted(url, certificate) && restored.rememberedCertificates().isEmpty(),
        "forgetting a certificate should remove both trust and management metadata");

    trust.removeRemembered(storedKey);
    require(request(manager, url).error != QNetworkReply::NoError && trust.pending(),
        "a forgotten certificate should fail closed on the next request");
    trust.cancel();
    require(!trust.pending(), "Cancel should reject and clear the pending trust decision");

    std::cout << "tls-trust: ok\n";
    return EXIT_SUCCESS;
}
