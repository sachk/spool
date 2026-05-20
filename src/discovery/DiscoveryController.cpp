#include "DiscoveryController.h"

#include "../diagnostics/Diagnostics.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace JellyfinNative {

namespace {

constexpr quint16 kDiscoveryPort = 7359;
constexpr auto kDiscoveryPayload = "who is JellyfinServer?";
constexpr int kHttpProbePort = 8096;
constexpr int kHttpProbeTimeoutMs = 1000;
constexpr int kHttpProbeConcurrency = 6;
constexpr int kInitialHttpFallbackDelayMs = 4000;

}

DiscoveryController::DiscoveryController(QObject *parent)
    : QObject(parent)
{
    connect(&m_socket, &QUdpSocket::readyRead, this, &DiscoveryController::handlePendingDatagrams);
    connect(&m_rescanTimer, &QTimer::timeout, this, &DiscoveryController::sendProbe);
    m_rescanTimer.setInterval(15000);
    connect(&m_httpFallbackTimer, &QTimer::timeout, this, &DiscoveryController::startHttpFallbackScan);
    m_httpFallbackTimer.setInterval(60000);
}

DiscoveryController::~DiscoveryController()
{
    stop();
}

bool DiscoveryController::active() const
{
    return m_active;
}

void DiscoveryController::start()
{
    Diagnostics::Task task(QStringLiteral("discovery_start"));
    if (m_active)
        return;

    m_active = ensureSocket();
    qInfo() << "discovery active" << m_active << "bound to" << m_socket.localAddress() << m_socket.localPort();
    emit activeChanged();
    if (!m_active)
        return;

    sendProbe();
    m_rescanTimer.start();
    m_httpFallbackTimer.start();
    QTimer::singleShot(kInitialHttpFallbackDelayMs, this, &DiscoveryController::startHttpFallbackScan);
}

void DiscoveryController::stop()
{
    Diagnostics::Phase phase(QStringLiteral("shutdown"), QStringLiteral("discovery_stop"), {{QStringLiteral("active"), m_active}, {QStringLiteral("inFlightHttpProbes"), m_inFlightHttpProbes}});
    if (!m_active)
        return;

    m_active = false;
    m_rescanTimer.stop();
    m_httpFallbackTimer.stop();
    m_socket.close();
    m_httpProbeQueue.clear();
    m_enqueuedHttpProbeTargets.clear();
    const auto replies = m_httpProbeReplies;
    for (QNetworkReply *reply : replies) {
        if (!reply)
            continue;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    m_httpProbeReplies.clear();
    m_inFlightHttpProbes = 0;
    emit activeChanged();
}

void DiscoveryController::sendProbe()
{
    if (!m_active)
        return;

    m_socket.writeDatagram(QByteArrayLiteral(kDiscoveryPayload), QHostAddress::Broadcast, kDiscoveryPort);
    qInfo() << "discovery probe sent" << QHostAddress(QHostAddress::Broadcast).toString() << kDiscoveryPort;

    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            !(iface.flags() & QNetworkInterface::IsRunning) ||
            (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol || entry.broadcast().isNull())
                continue;

            m_socket.writeDatagram(QByteArrayLiteral(kDiscoveryPayload), entry.broadcast(), kDiscoveryPort);
            qInfo() << "discovery probe sent" << iface.humanReadableName() << entry.broadcast().toString()
                    << kDiscoveryPort;
        }
    }
}

void DiscoveryController::handlePendingDatagrams()
{
    while (m_socket.hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket.receiveDatagram();
        if (datagram.data() == QByteArrayLiteral(kDiscoveryPayload))
            continue;

        const auto document = QJsonDocument::fromJson(datagram.data());
        if (!document.isObject())
            continue;
        const auto object = document.object();
        if (!object.contains(QStringLiteral("Id")) || !object.contains(QStringLiteral("Name")) ||
            !object.contains(QStringLiteral("Address"))) {
            continue;
        }

        qInfo() << "discovery reply" << datagram.senderAddress().toString() << object;
        emit serverDiscovered({
            object.value(QStringLiteral("Id")).toString(),
            object.value(QStringLiteral("Name")).toString(),
            object.value(QStringLiteral("Address")).toString(),
        });
    }
}

void DiscoveryController::startHttpFallbackScan()
{
    Diagnostics::Task task(QStringLiteral("discovery_http_fallback_scan"));
    if (!m_active || m_inFlightHttpProbes > 0 || !m_httpProbeQueue.isEmpty())
        return;

    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            !(iface.flags() & QNetworkInterface::IsRunning) ||
            (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol || entry.netmask().isNull())
                continue;

            quint32 start = 0;
            quint32 end = 0;
            const quint32 ip = entry.ip().toIPv4Address();
            const quint32 netmask = entry.netmask().toIPv4Address();
            const quint32 network = ip & netmask;
            const quint32 broadcast = network | ~netmask;

            if (broadcast > network + 1 && (broadcast - network - 1) <= 254) {
                start = network + 1;
                end = broadcast - 1;
            } else {
                start = ip & 0xFFFFFF00u;
                end = start + 254;
                ++start;
            }

            for (quint32 candidate = start; candidate <= end; ++candidate) {
                if (candidate == ip)
                    continue;
                enqueueHttpProbeTarget(QHostAddress(candidate));
            }
        }
    }

    qInfo() << "http discovery queued" << m_httpProbeQueue.size() << "targets";
    pumpHttpProbeQueue();
}

bool DiscoveryController::ensureSocket()
{
    if (m_socket.state() == QAbstractSocket::BoundState)
        return true;

    return m_socket.bind(QHostAddress::AnyIPv4, kDiscoveryPort,
                         QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
}

void DiscoveryController::enqueueHttpProbeTarget(const QHostAddress &address)
{
    const QString key = address.toString();
    if (m_enqueuedHttpProbeTargets.contains(key))
        return;

    m_enqueuedHttpProbeTargets.insert(key);
    m_httpProbeQueue.enqueue(address);
}

void DiscoveryController::pumpHttpProbeQueue()
{
    while (m_inFlightHttpProbes < kHttpProbeConcurrency && !m_httpProbeQueue.isEmpty()) {
        const QHostAddress address = m_httpProbeQueue.dequeue();
        const QString host = address.toString();
        const QString serverUrl = QStringLiteral("http://%1:%2").arg(host).arg(kHttpProbePort);

        QNetworkRequest request(QUrl(serverUrl + QStringLiteral("/System/Info/Public")));
        request.setTransferTimeout(kHttpProbeTimeoutMs);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("JellyfinNativeDiscovery/0.1.0"));

        QNetworkReply *reply = m_http.get(request);
        m_httpProbeReplies.insert(reply);
        ++m_inFlightHttpProbes;
        Diagnostics::logEvent(QStringLiteral("network"), QStringLiteral("discovery_probe_begin"), {{QStringLiteral("host"), host}, {QStringLiteral("inFlight"), m_inFlightHttpProbes}});

        connect(reply, &QNetworkReply::finished, this, [this, reply, serverUrl, host]() {
            m_httpProbeReplies.remove(reply);
            if (reply->error() == QNetworkReply::NoError) {
                const QByteArray payload = reply->readAll();
                qInfo() << "http discovery reply" << host;
                handleHttpProbeResult(serverUrl, payload);
            }

            reply->deleteLater();
            m_inFlightHttpProbes = qMax(0, m_inFlightHttpProbes - 1);
            m_enqueuedHttpProbeTargets.remove(host);
            Diagnostics::logEvent(QStringLiteral("network"), QStringLiteral("discovery_probe_end"), {{QStringLiteral("host"), host}, {QStringLiteral("inFlight"), m_inFlightHttpProbes}});
            pumpHttpProbeQueue();
        });
    }
}

void DiscoveryController::handleHttpProbeResult(const QString &serverUrl, const QByteArray &payload)
{
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject())
        return;

    const QJsonObject object = document.object();
    const QString id = object.value(QStringLiteral("Id")).toString();
    const QString name = object.value(QStringLiteral("ServerName")).toString();
    if (id.isEmpty() || name.isEmpty())
        return;

    emit serverDiscovered({
        id,
        name,
        serverUrl,
    });
}

} // namespace JellyfinNative
