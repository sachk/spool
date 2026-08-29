#pragma once

#include "SpoolLinkCodec.h"
#include "SpoolRemoteProtocol.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>

#include <optional>

class QUdpSocket;

namespace JellyfinNative {

class JellyfinApiFacade;

// Carries Spool-to-Spool messages by whichever path is available: a direct
// datagram link to the peer when one can be opened, the Jellyfin server
// otherwise. Callers hand a message to send() and never learn which was used.
//
// Two clients in the same country talking through a server on another
// continent pay two intercontinental legs for every message. A direct path
// turns a ~500 ms round trip into a local one, which is what makes scrubbing
// feel attached to the finger and what a clock comparison needs to be worth
// anything.
//
// The peers find each other over the server, which is already the trusted,
// authenticated channel between them: it relays the shared key and the
// candidate addresses. That grants the server nothing it did not already
// have, since it can command both clients directly.
class SpoolLink final : public QObject {
    Q_OBJECT

public:
    enum class State {
        Idle,
        Probing,
        Connected,
    };

    // Reliable messages are always delivered. Coalesced ones are only worth
    // sending while they are current: a newer message with the same command
    // replaces one still waiting, because showing the position before last is
    // worse than showing nothing.
    enum class Delivery {
        Reliable,
        Coalesced,
    };

    explicit SpoolLink(JellyfinApiFacade *api, QObject *parent = nullptr);
    ~SpoolLink() override;

    // This client's own session, so a peer knows where to answer.
    void setLocalSessionId(const QString& sessionId);
    QString localSessionId() const
    {
        return m_localSessionId;
    }

    // Open (or keep) a direct path to this peer. Repeat calls for the peer
    // already selected are ignored, so this is safe to call on every refresh.
    void connectToPeer(const QString& sessionId);
    void reset();

    void send(
        const QString& sessionId, const SpoolRemoteProtocol::Message& message, Delivery delivery = Delivery::Reliable);

    // A message the server delivered. Returns true when it was link
    // signalling, which the application must not see.
    bool handleServerMessage(const SpoolRemoteProtocol::Message& message);

    State state() const
    {
        return m_state;
    }
    bool connected() const
    {
        return m_state == State::Connected;
    }

    // Round trip on whichever path is carrying messages, in milliseconds, or
    // -1 before anything has been measured.
    int roundTripMs() const
    {
        return m_roundTripMs;
    }

    // How far the peer's monotonic clock reads ahead of ours, from the
    // exchange with the lowest round trip seen. The basis for lining up two
    // devices' playback clocks.
    std::optional<qint64> clockOffsetMs() const
    {
        return m_clockOffsetMs;
    }

signals:
    void messageReceived(const SpoolRemoteProtocol::Message& message);
    void stateChanged();
    void roundTripChanged();

private:
    using Endpoint = SpoolLinkCodec::Endpoint;
    using Datagram = SpoolLinkCodec::Datagram;

    bool ensureSocket();
    void gatherCandidates();
    void requestReflexiveCandidate();
    void sendOffer();
    void sendAnswer();
    void beginProbing();
    void stopProbing();
    void setState(State state);
    void setRoundTripMs(int roundTripMs);

    void readDatagrams();
    void handleStunResponse(const QByteArray& datagram);
    void handleLinkDatagram(const QByteArray& datagram, const Endpoint& from);

    bool transmit(Datagram type, const QByteArray& payload, const Endpoint& to);

    void sendProbes();
    void sendKeepalive();
    void noteExchange(qint64 roundTripMs, qint64 offsetMs);

    void flushOutbox();
    void scheduleFlush();
    int pacingIntervalMs() const;
    void deliverViaServer(const QString& sessionId, const SpoolRemoteProtocol::Message& message);

    JellyfinApiFacade *m_api = nullptr;
    QUdpSocket *m_socket = nullptr;

    QString m_localSessionId;
    QString m_peerSessionId;
    QByteArray m_key;
    // Set on the side that generated the key, which is the side that answers
    // an offer rather than making one if both happen to start at once.
    bool m_offered = false;

    QList<Endpoint> m_localCandidates;
    QList<Endpoint> m_peerCandidates;
    Endpoint m_peerEndpoint;

    QTimer m_probeTimer;
    QTimer m_keepaliveTimer;
    QTimer m_flushTimer;
    QElapsedTimer m_clock;
    QByteArray m_stunTransaction;

    // Newest message per command, plus everything that must not be dropped.
    QList<SpoolRemoteProtocol::Message> m_reliableOutbox;
    QList<SpoolRemoteProtocol::Message> m_coalescedOutbox;
    bool m_serverSendInFlight = false;
    qint64 m_lastFlushMs = 0;

    quint64 m_nextSequence = 1;
    SpoolLinkCodec::ReplayWindow m_replayWindow;
    qint64 m_pingSentMs = 0;
    qint64 m_lastPongMs = 0;
    qint64 m_bestRoundTripMs = -1;

    State m_state = State::Idle;
    int m_roundTripMs = -1;
    std::optional<qint64> m_clockOffsetMs;
};

} // namespace JellyfinNative
