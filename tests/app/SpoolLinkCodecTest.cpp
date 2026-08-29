#include "app/SpoolLinkCodec.h"

#include "TestMain.h"

#include <cstdlib>
#include <iostream>

using namespace JellyfinNative;
using namespace JellyfinNative::SpoolLinkCodec;

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
}

QByteArray testKey(char fill = 'k')
{
    return QByteArray(32, fill);
}

} // namespace

JELLYFIN_TEST_MAIN("spool-link-codec")
{
    const QByteArray key = testKey();
    const QByteArray payload = QByteArrayLiteral("{\"c\":\"SeekPreview\"}");

    Datagram type {};
    QByteArray decoded;
    quint64 sequence = 0;

    const QByteArray sealed = seal(key, Datagram::Payload, 7, payload);
    require(open(key, sealed, type, decoded, sequence), "a sealed datagram did not open");
    require(type == Datagram::Payload, "the datagram type changed");
    require(sequence == 7, "the sequence number changed");
    require(decoded == payload, "the payload changed");

    // Authentication: the key is what separates a peer from anyone else who
    // can reach the port.
    require(!open(testKey('x'), sealed, type, decoded, sequence), "a datagram opened under the wrong key");

    // First payload byte, just past the 16-byte header.
    QByteArray tampered = sealed;
    tampered[16] = char(tampered[16] ^ 0x01);
    require(!open(key, tampered, type, decoded, sequence), "a modified payload was accepted");

    QByteArray truncated = sealed.left(sealed.size() - 1);
    require(!open(key, truncated, type, decoded, sequence), "a truncated datagram was accepted");

    QByteArray foreign = sealed;
    foreign[0] = 'X';
    require(!open(key, foreign, type, decoded, sequence), "a datagram from another protocol was accepted");

    // Replay: out of order is still new, twice never is.
    ReplayWindow window;
    require(window.accept(1), "the first sequence number was rejected");
    require(!window.accept(1), "a replayed sequence number was accepted");
    require(window.accept(3), "a forward jump was rejected");
    require(window.accept(2), "a reordered datagram was rejected");
    require(!window.accept(2), "a reordered datagram was accepted twice");
    require(!window.accept(0), "a zero sequence number was accepted");
    require(window.accept(500), "a large forward jump was rejected");
    require(!window.accept(3), "a sequence number far behind the window was accepted");
    require(window.accept(499), "a datagram just inside the window was rejected");

    window.reset();
    require(window.accept(1), "a reset window did not start over");

    // Endpoints survive the trip through a text field in the envelope.
    const auto v4 = Endpoint::fromString(QStringLiteral("192.168.0.200:41234"));
    require(v4.has_value() && v4->port == 41234, "an IPv4 endpoint did not parse");
    require(Endpoint::fromString(v4->toString()) == v4, "an IPv4 endpoint did not round-trip");

    const auto v6 = Endpoint::fromString(QStringLiteral("[2001:db8::1]:9"));
    require(v6.has_value() && v6->port == 9, "an IPv6 endpoint did not parse");
    require(Endpoint::fromString(v6->toString()) == v6, "an IPv6 endpoint did not round-trip");
    require(v6->toString().startsWith(QLatin1Char('[')),
        "an IPv6 endpoint must bracket its address or the port cannot be found again");

    require(!Endpoint::fromString(QStringLiteral("192.168.0.200")), "an endpoint with no port was accepted");
    require(!Endpoint::fromString(QStringLiteral("192.168.0.200:0")), "port zero was accepted");
    require(!Endpoint::fromString(QStringLiteral("192.168.0.200:70000")), "an out-of-range port was accepted");
    require(!Endpoint::fromString(QStringLiteral("not-an-address:1")), "a non-address was accepted");

    return EXIT_SUCCESS;
}
