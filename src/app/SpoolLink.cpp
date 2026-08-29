#include "SpoolLink.h"

#include "../api/JellyfinApiFacade.h"
#include "../common/AsyncTask.h"

#include <QDateTime>
#include <QDebug>
#include <QHostInfo>
#include <QMessageAuthenticationCode>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QStringList>
#include <QUdpSocket>
#include <QtEndian>

#include <algorithm>

namespace JellyfinNative {
namespace {

    constexpr int kKeyBytes = 32;

    // Long enough that a punched pinhole cannot be reached by chance, short
    // enough that a stalled handshake gives up while the user still cares.
    constexpr int kProbeIntervalMs = 250;
    constexpr int kProbeGiveUpMs = 6'000;
    constexpr int kKeepaliveIntervalMs = 10'000;
    // Most consumer NATs hold a UDP mapping for 30 s or more; a peer that has
    // answered nothing for this long is gone.
    constexpr int kPeerLostMs = 32'000;

    // The floor is a display refresh: sending faster cannot change what the
    // other end draws. The ceiling keeps a slow link from queueing without
    // bound.
    constexpr int kMinPacingMs = 16;
    constexpr int kMaxPacingMs = 250;

    const QString kFromField = QStringLiteral("From");
    const QString kKeyField = QStringLiteral("Key");
    const QString kCandidatesField = QStringLiteral("Candidates");

    // Public STUN, used only to learn this device's own reflexive address for
    // IPv4. Nothing about the session is sent.
    const char *kStunHost = "stun.l.google.com";
    constexpr quint16 kStunPort = 19302;
    constexpr quint32 kStunCookie = 0x2112A442;

    bool isUsableCandidateAddress(const QHostAddress& address)
    {
        if (address.isNull() || address.isLoopback() || address.isLinkLocal() || address.isMulticast())
            return false;
        // A unique-local or global v6 address is reachable between peers; a v4
        // private address only helps when both sit on the same network, which
        // is exactly the case worth trying first.
        return true;
    }

    QByteArray stunBindingRequest(const QByteArray& transaction)
    {
        QByteArray request(20, '\0');
        qToBigEndian<quint16>(0x0001, request.data());
        qToBigEndian<quint16>(0, request.data() + 2);
        qToBigEndian<quint32>(kStunCookie, request.data() + 4);
        std::copy(transaction.cbegin(), transaction.cend(), request.begin() + 8);
        return request;
    }

} // namespace

SpoolLink::SpoolLink(JellyfinApiFacade *api, QObject *parent)
    : QObject(parent)
    , m_api(api)
{
    m_clock.start();

    m_probeTimer.setInterval(kProbeIntervalMs);
    connect(&m_probeTimer, &QTimer::timeout, this, &SpoolLink::sendProbes);

    m_keepaliveTimer.setInterval(kKeepaliveIntervalMs);
    connect(&m_keepaliveTimer, &QTimer::timeout, this, &SpoolLink::sendKeepalive);

    m_flushTimer.setSingleShot(true);
    connect(&m_flushTimer, &QTimer::timeout, this, &SpoolLink::flushOutbox);
}

SpoolLink::~SpoolLink() = default;

void SpoolLink::setLocalSessionId(const QString& sessionId)
{
    m_localSessionId = sessionId;
}

void SpoolLink::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
}

void SpoolLink::setRoundTripMs(int roundTripMs)
{
    if (m_roundTripMs == roundTripMs)
        return;
    m_roundTripMs = roundTripMs;
    emit roundTripChanged();
}

void SpoolLink::reset()
{
    stopProbing();
    m_keepaliveTimer.stop();
    m_peerSessionId.clear();
    m_key.clear();
    m_offered = false;
    m_peerCandidates.clear();
    m_peerEndpoint = {};
    m_reliableOutbox.clear();
    m_coalescedOutbox.clear();
    m_replayWindow.reset();
    m_bestRoundTripMs = -1;
    m_lastPongMs = 0;
    m_clockOffsetMs.reset();
    setRoundTripMs(-1);
    setState(State::Idle);
}

bool SpoolLink::ensureSocket()
{
    if (m_socket)
        return true;

    m_socket = new QUdpSocket(this);
    // One dual-stack socket, so a v6 path and a v4 path share a port and the
    // peer only has to be told one number per address family.
    if (!m_socket->bind(QHostAddress(QHostAddress::Any), 0, QAbstractSocket::DefaultForPlatform)) {
        qInfo() << "spool link: could not bind a local socket:" << m_socket->errorString();
        delete m_socket;
        m_socket = nullptr;
        return false;
    }
    connect(m_socket, &QUdpSocket::readyRead, this, &SpoolLink::readDatagrams);
    return true;
}

void SpoolLink::gatherCandidates()
{
    m_localCandidates.clear();
    if (!m_socket)
        return;
    const quint16 port = m_socket->localPort();
    for (const QHostAddress& address : QNetworkInterface::allAddresses()) {
        if (!isUsableCandidateAddress(address))
            continue;
        Endpoint endpoint;
        // Qt keeps the scope id on link-local and some global v6 addresses; it
        // is meaningless to the peer and breaks parsing on the far side.
        endpoint.address = QHostAddress(address.toString().section(QLatin1Char('%'), 0, 0));
        endpoint.port = port;
        if (endpoint.isValid() && !m_localCandidates.contains(endpoint))
            m_localCandidates.push_back(endpoint);
    }
}

void SpoolLink::requestReflexiveCandidate()
{
    if (!m_socket)
        return;
    m_stunTransaction.resize(12);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(m_stunTransaction.data()), 3);
    // Sent from the same socket the peer will talk to, so the mapping STUN
    // reports is the mapping the peer can reach.
    QHostInfo::lookupHost(QString::fromLatin1(kStunHost), this, [this](const QHostInfo& info) {
        if (!m_socket || info.addresses().isEmpty() || m_stunTransaction.isEmpty())
            return;
        for (const QHostAddress& address : info.addresses()) {
            if (address.protocol() == QAbstractSocket::IPv4Protocol) {
                m_socket->writeDatagram(stunBindingRequest(m_stunTransaction), address, kStunPort);
                return;
            }
        }
    });
}

void SpoolLink::connectToPeer(const QString& sessionId)
{
    if (sessionId.isEmpty() || sessionId == m_localSessionId) {
        reset();
        return;
    }
    if (sessionId == m_peerSessionId && m_state != State::Idle)
        return;

    const bool sameSession = sessionId == m_peerSessionId;
    reset();
    m_peerSessionId = sessionId;
    if (!ensureSocket())
        return;

    gatherCandidates();
    requestReflexiveCandidate();

    m_key.resize(kKeyBytes);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(m_key.data()), kKeyBytes / 4);
    m_offered = true;

    // Give a reflexive candidate a moment to arrive, then offer whatever was
    // gathered. Local addresses alone are enough on a shared network.
    QTimer::singleShot(sameSession ? 0 : 400, this, [this]() {
        if (!m_peerSessionId.isEmpty() && m_offered)
            sendOffer();
    });
}

void SpoolLink::sendOffer()
{
    if (m_localSessionId.isEmpty() || m_peerSessionId.isEmpty() || m_localCandidates.isEmpty())
        return;
    QStringList candidates;
    candidates.reserve(m_localCandidates.size());
    for (const Endpoint& endpoint : m_localCandidates)
        candidates.push_back(endpoint.toString());

    SpoolRemoteProtocol::Message offer { SpoolRemoteProtocol::Command::LinkOffer,
        {
            { kFromField, m_localSessionId },
            { kKeyField, QString::fromLatin1(m_key.toBase64()) },
            { kCandidatesField, candidates.join(QLatin1Char(' ')) },
        } };
    deliverViaServer(m_peerSessionId, offer);
    beginProbing();
}

void SpoolLink::sendAnswer()
{
    if (m_localSessionId.isEmpty() || m_peerSessionId.isEmpty() || m_localCandidates.isEmpty())
        return;
    QStringList candidates;
    candidates.reserve(m_localCandidates.size());
    for (const Endpoint& endpoint : m_localCandidates)
        candidates.push_back(endpoint.toString());

    SpoolRemoteProtocol::Message answer { SpoolRemoteProtocol::Command::LinkAnswer,
        {
            { kFromField, m_localSessionId },
            { kCandidatesField, candidates.join(QLatin1Char(' ')) },
        } };
    deliverViaServer(m_peerSessionId, answer);
}

bool SpoolLink::handleServerMessage(const SpoolRemoteProtocol::Message& message)
{
    const bool isOffer = message.command == SpoolRemoteProtocol::Command::LinkOffer;
    const bool isAnswer = message.command == SpoolRemoteProtocol::Command::LinkAnswer;
    if (!isOffer && !isAnswer)
        return false;

    const QString from = message.text(kFromField);
    if (from.isEmpty())
        return true;

    QList<Endpoint> candidates;
    const QStringList encoded = message.text(kCandidatesField).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& text : encoded) {
        if (const std::optional<Endpoint> endpoint = Endpoint::fromString(text))
            candidates.push_back(*endpoint);
    }
    if (candidates.isEmpty())
        return true;

    if (isOffer) {
        const QByteArray key = QByteArray::fromBase64(message.text(kKeyField).toLatin1());
        if (key.size() != kKeyBytes)
            return true;
        // The offering side owns the key, so a simultaneous offer from both
        // ends settles on one of them rather than on two different keys.
        if (m_offered && m_localSessionId > from)
            return true;
        if (!ensureSocket())
            return true;
        stopProbing();
        m_peerSessionId = from;
        m_key = key;
        m_offered = false;
        m_peerEndpoint = {};
        m_replayWindow.reset();
        m_peerCandidates = candidates;
        gatherCandidates();
        sendAnswer();
        beginProbing();
        return true;
    }

    if (from != m_peerSessionId || m_key.isEmpty())
        return true;
    m_peerCandidates = candidates;
    beginProbing();
    return true;
}

void SpoolLink::beginProbing()
{
    if (m_peerCandidates.isEmpty() || m_key.isEmpty() || !m_socket)
        return;
    if (m_state != State::Connected)
        setState(State::Probing);
    m_probeTimer.start();
    sendProbes();
    QTimer::singleShot(kProbeGiveUpMs, this, [this]() {
        if (m_state == State::Probing) {
            // Nothing answered. The server path stays in service, so this is a
            // missed optimisation rather than a failure.
            qInfo() << "spool link: no direct path to" << m_peerSessionId << "- staying on the server";
            stopProbing();
            setState(State::Idle);
        }
    });
}

void SpoolLink::stopProbing()
{
    m_probeTimer.stop();
}

void SpoolLink::sendProbes()
{
    if (m_state == State::Connected) {
        stopProbing();
        return;
    }
    // Both ends probe at once: that simultaneous exchange is what opens the
    // pinhole each side's firewall needs for the other's datagrams.
    for (const Endpoint& candidate : m_peerCandidates)
        transmit(Datagram::Probe, {}, candidate);
}

void SpoolLink::sendKeepalive()
{
    if (m_state != State::Connected)
        return;
    if (m_lastPongMs > 0 && m_clock.elapsed() - m_lastPongMs > kPeerLostMs) {
        qInfo() << "spool link: peer stopped answering, falling back to the server";
        const QString peer = m_peerSessionId;
        reset();
        connectToPeer(peer);
        return;
    }
    m_pingSentMs = m_clock.elapsed();
    QByteArray payload(8, '\0');
    qToBigEndian<qint64>(m_pingSentMs, payload.data());
    transmit(Datagram::Ping, payload, m_peerEndpoint);
}

bool SpoolLink::transmit(Datagram type, const QByteArray& payload, const Endpoint& to)
{
    if (!m_socket || m_key.isEmpty() || !to.isValid())
        return false;
    const QByteArray datagram = SpoolLinkCodec::seal(m_key, type, m_nextSequence++, payload);
    return m_socket->writeDatagram(datagram, to.address, to.port) == datagram.size();
}

void SpoolLink::readDatagrams()
{
    while (m_socket && m_socket->hasPendingDatagrams()) {
        QByteArray buffer(int(m_socket->pendingDatagramSize()), '\0');
        QHostAddress sender;
        quint16 senderPort = 0;
        const qint64 read = m_socket->readDatagram(buffer.data(), buffer.size(), &sender, &senderPort);
        if (read <= 0)
            continue;
        buffer.truncate(int(read));

        if (buffer.size() >= 20 && (static_cast<quint8>(buffer[0]) & 0xC0) == 0) {
            handleStunResponse(buffer);
            continue;
        }
        Endpoint from;
        from.address = sender;
        from.port = senderPort;
        handleLinkDatagram(buffer, from);
    }
}

void SpoolLink::handleStunResponse(const QByteArray& datagram)
{
    if (qFromBigEndian<quint16>(datagram.constData()) != 0x0101)
        return;
    if (datagram.mid(8, 12) != m_stunTransaction)
        return;

    qsizetype offset = 20;
    // The declared length, never trusted past what actually arrived.
    const qsizetype length
        = std::min<qsizetype>(qFromBigEndian<quint16>(datagram.constData() + 2) + 20, datagram.size());
    while (offset + 4 <= length) {
        const quint16 type = qFromBigEndian<quint16>(datagram.constData() + offset);
        const quint16 size = qFromBigEndian<quint16>(datagram.constData() + offset + 2);
        const qsizetype valueAt = offset + 4;
        if (valueAt + size > datagram.size())
            return;

        // XOR-MAPPED-ADDRESS. The obfuscation exists so middleboxes cannot
        // rewrite the address they are reporting.
        if (type == 0x0020 && size >= 8) {
            const quint8 family = static_cast<quint8>(datagram[valueAt + 1]);
            const quint16 port
                = qFromBigEndian<quint16>(datagram.constData() + valueAt + 2) ^ quint16(kStunCookie >> 16);
            if (family == 0x01) {
                const quint32 address = qFromBigEndian<quint32>(datagram.constData() + valueAt + 4) ^ kStunCookie;
                Endpoint reflexive;
                reflexive.address = QHostAddress(address);
                reflexive.port = port;
                if (reflexive.isValid() && !m_localCandidates.contains(reflexive)) {
                    m_localCandidates.push_back(reflexive);
                    qInfo() << "spool link: reflexive candidate" << reflexive.toString();
                }
            }
            return;
        }
        offset = valueAt + ((size + 3) & ~3);
    }
}

void SpoolLink::handleLinkDatagram(const QByteArray& datagram, const Endpoint& from)
{
    Datagram type {};
    QByteArray payload;
    quint64 sequence = 0;
    if (!SpoolLinkCodec::open(m_key, datagram, type, payload, sequence) || !m_replayWindow.accept(sequence))
        return;

    const qint64 now = m_clock.elapsed();
    switch (type) {
    case Datagram::Probe:
        // Answer from the address it arrived on, which is the mapping that
        // actually works rather than the one the peer guessed.
        transmit(Datagram::ProbeAck, {}, from);
        break;

    case Datagram::ProbeAck:
        if (m_state != State::Connected) {
            m_peerEndpoint = from;
            m_lastPongMs = now;
            stopProbing();
            setState(State::Connected);
            qInfo() << "spool link: direct path to" << m_peerSessionId << "via" << from.toString();
            sendKeepalive();
            flushOutbox();
        }
        break;

    case Datagram::Ping: {
        if (payload.size() < 8)
            break;
        QByteArray reply(24, '\0');
        // t1 as the peer sent it, then our receive and send instants: enough
        // for the far side to solve for offset and round trip together.
        std::copy(payload.cbegin(), payload.cbegin() + 8, reply.begin());
        qToBigEndian<qint64>(now, reply.data() + 8);
        qToBigEndian<qint64>(m_clock.elapsed(), reply.data() + 16);
        transmit(Datagram::Pong, reply, from);
        break;
    }

    case Datagram::Pong: {
        if (payload.size() < 24)
            break;
        const qint64 t1 = qFromBigEndian<qint64>(payload.constData());
        const qint64 t2 = qFromBigEndian<qint64>(payload.constData() + 8);
        const qint64 t3 = qFromBigEndian<qint64>(payload.constData() + 16);
        const qint64 t4 = now;
        m_lastPongMs = now;
        noteExchange((t4 - t1) - (t3 - t2), ((t2 - t1) + (t3 - t4)) / 2);
        break;
    }

    case Datagram::Payload:
        if (const std::optional<SpoolRemoteProtocol::Message> message = SpoolRemoteProtocol::deserialize(payload)) {
            emit messageReceived(*message);
        }
        break;
    }
}

void SpoolLink::noteExchange(qint64 roundTripMs, qint64 offsetMs)
{
    if (roundTripMs < 0)
        return;
    setRoundTripMs(int(std::min<qint64>(roundTripMs, kMaxPacingMs * 4)));
    // Offset error is bounded by how unevenly the two legs were delayed, so
    // the sample with the smallest round trip is the most trustworthy one
    // rather than the average of many.
    if (m_bestRoundTripMs < 0 || roundTripMs <= m_bestRoundTripMs) {
        m_bestRoundTripMs = roundTripMs;
        m_clockOffsetMs = offsetMs;
    }
}

int SpoolLink::pacingIntervalMs() const
{
    // Sending faster than the path can answer only builds a queue; slower than
    // a frame cannot be seen. Between those, follow the measurement.
    const int measured = m_roundTripMs >= 0 ? m_roundTripMs : kMaxPacingMs;
    return std::clamp(measured, kMinPacingMs, kMaxPacingMs);
}

void SpoolLink::send(const QString& sessionId, const SpoolRemoteProtocol::Message& message, Delivery delivery)
{
    if (sessionId.isEmpty() || !message.isValid())
        return;
    // Only the selected peer has a direct path; anything else is the server's
    // job and goes out immediately.
    if (sessionId != m_peerSessionId) {
        deliverViaServer(sessionId, message);
        return;
    }

    if (delivery == Delivery::Coalesced) {
        const auto stale = std::find_if(m_coalescedOutbox.begin(), m_coalescedOutbox.end(),
            [&message](const SpoolRemoteProtocol::Message& queued) { return queued.command == message.command; });
        if (stale != m_coalescedOutbox.end())
            *stale = message;
        else
            m_coalescedOutbox.push_back(message);
    } else {
        m_reliableOutbox.push_back(message);
    }
    scheduleFlush();
}

// Always returns through the event loop, never straight into flushOutbox().
// A synchronous hand-back would let the two call each other without ever
// unwinding, which wedges the thread that draws the interface.
void SpoolLink::scheduleFlush()
{
    if (m_flushTimer.isActive())
        return;
    const qint64 sinceLast = m_clock.elapsed() - m_lastFlushMs;
    const int interval = pacingIntervalMs();
    m_flushTimer.start(int(std::max<qint64>(0, interval - sinceLast)));
}

void SpoolLink::flushOutbox()
{
    if (m_reliableOutbox.isEmpty() && m_coalescedOutbox.isEmpty())
        return;

    if (connected()) {
        // The direct path has no outstanding-request limit worth enforcing:
        // a datagram is gone the moment it is written.
        const QList<SpoolRemoteProtocol::Message> pending = m_reliableOutbox + m_coalescedOutbox;
        m_reliableOutbox.clear();
        m_coalescedOutbox.clear();
        m_lastFlushMs = m_clock.elapsed();
        for (const SpoolRemoteProtocol::Message& message : pending)
            transmit(Datagram::Payload, SpoolRemoteProtocol::serialize(message), m_peerEndpoint);
        return;
    }

    // Through the server, one request at a time. More than that and the
    // replies queue behind each other and arrive long after they mattered.
    // Nothing is scheduled here: the request in flight rearms the flush when
    // it finishes, which is the only moment the answer can change.
    if (m_serverSendInFlight)
        return;
    const SpoolRemoteProtocol::Message message
        = m_reliableOutbox.isEmpty() ? m_coalescedOutbox.takeFirst() : m_reliableOutbox.takeFirst();
    m_lastFlushMs = m_clock.elapsed();
    deliverViaServer(m_peerSessionId, message);
}

void SpoolLink::deliverViaServer(const QString& sessionId, const SpoolRemoteProtocol::Message& message)
{
    if (!m_api || sessionId.isEmpty())
        return;
    const bool paced = sessionId == m_peerSessionId;
    if (paced)
        m_serverSendInFlight = true;
    const qint64 sentAtMs = m_clock.elapsed();

    Async::runScoped(
        this,
        m_api->sendRemoteGeneralCommand(
            sessionId, SpoolRemoteProtocol::envelopeCommand(), SpoolRemoteProtocol::encode(message)),
        [this, paced, sentAtMs]() {
            if (!paced)
                return;
            m_serverSendInFlight = false;
            // With no direct path, the server round trip is the pace the link
            // can actually keep.
            if (!connected())
                setRoundTripMs(int(m_clock.elapsed() - sentAtMs));
            if (!m_reliableOutbox.isEmpty() || !m_coalescedOutbox.isEmpty())
                scheduleFlush();
        },
        [this, paced](const std::exception_ptr&) {
            if (!paced)
                return;
            m_serverSendInFlight = false;
            if (!m_reliableOutbox.isEmpty() || !m_coalescedOutbox.isEmpty())
                scheduleFlush();
        },
        "spool link server message");
}

} // namespace JellyfinNative
