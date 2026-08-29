#include "SpoolLinkCodec.h"

#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QtEndian>

#include <algorithm>

namespace JellyfinNative {
namespace {

    constexpr char kMagic[4] = { 'S', 'P', 'L', 'K' };
    constexpr quint8 kVersion = 1;
    constexpr int kHeaderBytes = 16;
    constexpr int kTagBytes = 32;

} // namespace

QString SpoolLinkCodec::Endpoint::toString() const
{
    return address.protocol() == QAbstractSocket::IPv6Protocol
        ? QStringLiteral("[%1]:%2").arg(address.toString()).arg(port)
        : QStringLiteral("%1:%2").arg(address.toString()).arg(port);
}

std::optional<SpoolLinkCodec::Endpoint> SpoolLinkCodec::Endpoint::fromString(const QString& text)
{
    const int separator = text.lastIndexOf(QLatin1Char(':'));
    if (separator <= 0)
        return std::nullopt;

    QString host = text.left(separator);
    if (host.startsWith(QLatin1Char('[')) && host.endsWith(QLatin1Char(']')))
        host = host.mid(1, host.size() - 2);

    bool ok = false;
    const uint port = text.mid(separator + 1).toUInt(&ok);
    if (!ok || port == 0 || port > 65535)
        return std::nullopt;

    Endpoint endpoint;
    if (!endpoint.address.setAddress(host))
        return std::nullopt;
    endpoint.port = static_cast<quint16>(port);
    return endpoint;
}

QByteArray SpoolLinkCodec::seal(const QByteArray& key, Datagram type, quint64 sequence, const QByteArray& payload)
{
    QByteArray datagram;
    datagram.reserve(kHeaderBytes + payload.size() + kTagBytes);
    datagram.append(kMagic, 4);
    datagram.append(static_cast<char>(kVersion));
    datagram.append(static_cast<char>(type));
    datagram.append(2, '\0');

    QByteArray sequenceBytes(8, '\0');
    qToBigEndian<quint64>(sequence, sequenceBytes.data());
    datagram.append(sequenceBytes);
    datagram.append(payload);

    QMessageAuthenticationCode code(QCryptographicHash::Sha256, key);
    code.addData(datagram);
    datagram.append(code.result());
    return datagram;
}

bool SpoolLinkCodec::open(
    const QByteArray& key, const QByteArray& datagram, Datagram& type, QByteArray& payload, quint64& sequence)
{
    if (datagram.size() < kHeaderBytes + kTagBytes || key.isEmpty())
        return false;
    if (!std::equal(kMagic, kMagic + 4, datagram.cbegin()) || static_cast<quint8>(datagram[4]) != kVersion)
        return false;

    const QByteArray body = datagram.left(datagram.size() - kTagBytes);
    QMessageAuthenticationCode code(QCryptographicHash::Sha256, key);
    code.addData(body);
    // Constant time: comparing tags with an early exit would leak how much of
    // a forged one was right.
    const QByteArray expected = code.result();
    const QByteArray actual = datagram.right(kTagBytes);
    if (expected.size() != actual.size())
        return false;
    quint8 difference = 0;
    for (int index = 0; index < expected.size(); ++index)
        difference |= static_cast<quint8>(expected[index]) ^ static_cast<quint8>(actual[index]);
    if (difference != 0)
        return false;

    type = static_cast<Datagram>(static_cast<quint8>(datagram[5]));
    sequence = qFromBigEndian<quint64>(datagram.constData() + 8);
    payload = body.mid(kHeaderBytes);
    return true;
}

void SpoolLinkCodec::ReplayWindow::reset()
{
    m_highest = 0;
    m_seen = 0;
}

bool SpoolLinkCodec::ReplayWindow::accept(quint64 sequence)
{
    constexpr quint64 kWindow = 64;
    if (sequence == 0)
        return false;
    if (sequence > m_highest) {
        const quint64 shift = sequence - m_highest;
        m_seen = shift >= kWindow ? 0 : (m_seen << shift);
        m_seen |= 1;
        m_highest = sequence;
        return true;
    }
    const quint64 age = m_highest - sequence;
    if (age >= kWindow)
        return false;
    const quint64 bit = quint64(1) << age;
    if (m_seen & bit)
        return false;
    m_seen |= bit;
    return true;
}

} // namespace JellyfinNative
