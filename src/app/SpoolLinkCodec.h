#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QString>

#include <optional>

namespace JellyfinNative {

// The wire format, with no I/O in it. Kept apart from the socket so the
// framing, the authentication and the replay rule can be tested directly
// rather than through two devices and a network.
namespace SpoolLinkCodec {

    enum class Datagram : quint8 {
        Probe = 1,
        ProbeAck = 2,
        Payload = 3,
        Ping = 4,
        Pong = 5,
    };

    struct Endpoint {
        QHostAddress address;
        quint16 port = 0;

        bool operator==(const Endpoint& other) const
        {
            return port == other.port && address.isEqual(other.address);
        }
        bool isValid() const
        {
            return port != 0 && !address.isNull();
        }
        QString toString() const;
        static std::optional<Endpoint> fromString(const QString& text);
    };

    QByteArray seal(const QByteArray& key, Datagram type, quint64 sequence, const QByteArray& payload);
    bool open(
        const QByteArray& key, const QByteArray& datagram, Datagram& type, QByteArray& payload, quint64& sequence);

    // Accepts each sequence number once. A datagram that arrives out of order
    // is still new; one that arrives twice never is.
    class ReplayWindow {
    public:
        bool accept(quint64 sequence);
        void reset();

    private:
        quint64 m_highest = 0;
        quint64 m_seen = 0;
    };

} // namespace SpoolLinkCodec

} // namespace JellyfinNative
