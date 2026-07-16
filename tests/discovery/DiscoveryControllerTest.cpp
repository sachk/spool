#include "discovery/DiscoveryController.h"

#include <QCoreApplication>
#include <QSet>

#include <cstdlib>
#include <iostream>

using namespace JellyfinNative;

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << message << '\n';
    std::exit(1);
}

QStringList strings(const QList<QUrl>& urls)
{
    QStringList result;
    for (const QUrl& url : urls)
        result.push_back(url.toString(QUrl::FullyEncoded));
    return result;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    require(strings(DiscoveryController::serverProbeCandidates(QStringLiteral("192.168.1.30")))
            == QStringList { QStringLiteral("http://192.168.1.30:8096"), QStringLiteral("https://192.168.1.30"),
                QStringLiteral("http://192.168.1.30") },
        "bare LAN addresses should prefer HTTP port 8096");
    require(strings(DiscoveryController::serverProbeCandidates(QStringLiteral("192.168.1.30:8920")))
            == QStringList { QStringLiteral("http://192.168.1.30:8920"), QStringLiteral("https://192.168.1.30:8920") },
        "bare LAN addresses should retain an explicit port");
    require(strings(DiscoveryController::serverProbeCandidates(QStringLiteral("media.example.test/jellyfin/")))
            == QStringList { QStringLiteral("https://media.example.test/jellyfin"),
                QStringLiteral("http://media.example.test:8096/jellyfin"),
                QStringLiteral("http://media.example.test/jellyfin") },
        "public names should try HTTPS first and preserve base paths");
    require(strings(DiscoveryController::serverProbeCandidates(QStringLiteral("https://media.example.test/root")))
            == QStringList { QStringLiteral("https://media.example.test/root"),
                QStringLiteral("http://media.example.test:8096/root") },
        "explicit HTTPS should fall back to the typical HTTP port");
    require(DiscoveryController::serverProbeCandidates(QStringLiteral("not a url")).isEmpty(),
        "invalid manual addresses should not produce candidates");

    QString version;
    const DiscoveredServer parsed = DiscoveryController::serverFromPublicInfo(
        QByteArrayLiteral(R"({"Id":"server-id","ServerName":"Living Room","Version":"10.11.8"})"),
        QUrl(QStringLiteral("http://192.168.1.30:8096/jellyfin/")), &version);
    require(parsed.id == QStringLiteral("server-id") && parsed.name == QStringLiteral("Living Room")
            && parsed.address == QStringLiteral("http://192.168.1.30:8096/jellyfin")
            && version == QStringLiteral("10.11.8"),
        "public server info should preserve the working base URL and metadata");
    require(DiscoveryController::serverFromPublicInfo(
                QByteArrayLiteral(R"({"ServerName":"Missing id"})"), QUrl(QStringLiteral("http://server")), nullptr)
                .id.isEmpty(),
        "public server info without an id should be rejected");

    const QList<QHostAddress> local24 = DiscoveryController::httpFallbackTargets(
        QHostAddress(QStringLiteral("192.168.20.50")), QHostAddress(QStringLiteral("255.255.255.0")));
    require(local24.size() == 253, "a /24 scan should include every other usable host");
    require(local24.first() == QHostAddress(QStringLiteral("192.168.20.1")),
        "a /24 scan should prioritize the likely gateway/server address");
    require(!local24.contains(QHostAddress(QStringLiteral("192.168.20.50"))),
        "a subnet scan should exclude the client address");

    const QList<QHostAddress> local23 = DiscoveryController::httpFallbackTargets(
        QHostAddress(QStringLiteral("10.20.5.40")), QHostAddress(QStringLiteral("255.255.254.0")));
    require(local23.size() == 509, "a bounded /23 should include both related /24 networks");
    require(
        local23.contains(QHostAddress(QStringLiteral("10.20.4.1"))), "a /23 scan should include the sibling subnet");

    const QList<QHostAddress> broad = DiscoveryController::httpFallbackTargets(
        QHostAddress(QStringLiteral("172.16.8.20")), QHostAddress(QStringLiteral("255.255.0.0")));
    require(broad.contains(QHostAddress(QStringLiteral("172.16.7.1")))
            && broad.contains(QHostAddress(QStringLiteral("172.16.9.1"))),
        "a broad private subnet should probe adjacent /24 networks");
    require(broad.size() == 761, "a broad subnet scan should remain bounded to three /24 networks");
    require(DiscoveryController::httpFallbackTargets(
                QHostAddress(QStringLiteral("203.0.113.5")), QHostAddress(QStringLiteral("255.255.255.0")))
                .isEmpty(),
        "HTTP subnet fallback must not scan public networks");

    return 0;
}
