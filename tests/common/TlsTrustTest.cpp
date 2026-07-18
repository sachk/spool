#include "common/TlsTrust.h"

#include <QCoreApplication>

#include <cstdlib>
#include <iostream>

using JellyfinNative::TlsTrust::endpointKey;

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    require(endpointKey(QUrl(QStringLiteral("https://Server.Example")))
            == endpointKey(QUrl(QStringLiteral("wss://server.example:443/socket"))),
        "HTTPS API and secure WebSocket should share one exact endpoint trust decision");
    require(endpointKey(QUrl(QStringLiteral("https://server.example")))
            != endpointKey(QUrl(QStringLiteral("https://server.example:8443"))),
        "certificate trust must not spread to another port");
    require(endpointKey(QUrl(QStringLiteral("https://server.example")))
            != endpointKey(QUrl(QStringLiteral("https://other.example"))),
        "certificate trust must not spread to another host");
    return EXIT_SUCCESS;
}
