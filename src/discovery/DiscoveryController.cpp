#include "DiscoveryController.h"
#include "../common/NetworkAddress.h"
#include "../common/TlsTrust.h"

#include "../diagnostics/Diagnostics.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

#include <utility>

namespace JellyfinNative {

namespace {

    constexpr quint16 kDiscoveryPort = 7359;
    constexpr auto kDiscoveryPayload = "who is JellyfinServer?";
    constexpr int kHttpProbePort = 8096;
    constexpr int kHttpProbeTimeoutMs = 600;
    constexpr int kManualProbeTimeoutMs = 3000;
    constexpr int kHttpProbeConcurrency = 8;
    constexpr int kInitialHttpFallbackDelayMs = 1500;
    constexpr int kMaxAutomaticHttpProbeTargets = 254;

    QUrl normalizedServerUrl(QUrl url)
    {
        url.setFragment({});
        url.setQuery({});
        QString path = url.path();
        while (path.endsWith(QLatin1Char('/')) && path.size() > 1)
            path.chop(1);
        if (path == QStringLiteral("/"))
            path.clear();
        url.setPath(path);
        return url;
    }

    QUrl publicInfoUrl(QUrl serverUrl)
    {
        QString path = serverUrl.path();
        while (path.endsWith(QLatin1Char('/')))
            path.chop(1);
        serverUrl.setPath(path + QStringLiteral("/System/Info/Public"));
        return serverUrl;
    }

    QUrl serverUrlFromPublicInfoUrl(QUrl publicInfo)
    {
        static const QString suffix = QStringLiteral("/System/Info/Public");
        QString path = publicInfo.path();
        if (path.endsWith(suffix))
            path.chop(suffix.size());
        publicInfo.setPath(path);
        return normalizedServerUrl(publicInfo);
    }

    void appendUniqueUrl(QList<QUrl> *urls, QSet<QString> *seen, const QUrl& candidate)
    {
        const QUrl normalized = normalizedServerUrl(candidate);
        if (!normalized.isValid() || normalized.host().isEmpty()
            || (normalized.scheme() != QStringLiteral("http") && normalized.scheme() != QStringLiteral("https"))) {
            return;
        }
        const QString key = normalized.toString(QUrl::FullyEncoded);
        if (seen->contains(key))
            return;
        seen->insert(key);
        urls->push_back(normalized);
    }

    void appendSubnetTargets(QList<QHostAddress> *targets, QSet<quint32> *seen, quint32 network, quint32 broadcast,
        quint32 ownAddress, int maxTargets)
    {
        if (broadcast <= network + 1 || targets->size() >= maxTargets)
            return;

        const auto append = [&](quint32 candidate) {
            if (targets->size() >= maxTargets || candidate <= network || candidate >= broadcast
                || candidate == ownAddress || seen->contains(candidate)) {
                return;
            }
            seen->insert(candidate);
            targets->push_back(QHostAddress(candidate));
        };

        append(network + 1);
        append(broadcast - 1);
        if (ownAddress > network + 1)
            append(ownAddress - 1);
        if (ownAddress + 1 < broadcast)
            append(ownAddress + 1);
        for (quint32 candidate = network + 1; candidate < broadcast && targets->size() < maxTargets; ++candidate)
            append(candidate);
    }

}

DiscoveryController::DiscoveryController(TlsTrustController *tlsTrust, QObject *parent)
    : QObject(parent)
{
    if (tlsTrust) {
        tlsTrust->attachNetworkAccessManager(&m_http, QStringLiteral("Server discovery"));
        connect(
            &m_http, &QNetworkAccessManager::sslErrors, this, [this](QNetworkReply *reply, const QList<QSslError>&) {
                if (reply == m_serverProbeReply)
                    m_tlsRetryInput = m_serverProbeInput;
            });
        connect(
            tlsTrust, &TlsTrustController::decisionResolved, this, [this](const QString& source, bool trusted, bool) {
                if (source != QStringLiteral("Server discovery") || m_tlsRetryInput.isEmpty())
                    return;
                const QString input = std::exchange(m_tlsRetryInput, {});
                if (trusted)
                    probeServer(input);
            });
    }
    connect(&m_socket, &QUdpSocket::readyRead, this, &DiscoveryController::handlePendingDatagrams);
    connect(&m_rescanTimer, &QTimer::timeout, this, &DiscoveryController::sendProbe);
    m_rescanTimer.setInterval(15000);
}

DiscoveryController::~DiscoveryController()
{
    stop();
}

bool DiscoveryController::active() const
{
    return m_active;
}

bool DiscoveryController::serverProbeActive() const
{
    return m_serverProbeReply || !m_serverProbeCandidates.isEmpty();
}

QList<QUrl> DiscoveryController::serverProbeCandidates(const QString& input)
{
    QString value = input.trimmed();
    while (value.endsWith(QLatin1Char('/')))
        value.chop(1);
    if (value.isEmpty())
        return {};

    static const QRegularExpression explicitScheme(
        QStringLiteral(R"(^[a-z][a-z0-9+.-]*://)"), QRegularExpression::CaseInsensitiveOption);
    const bool explicitUrl = explicitScheme.match(value).hasMatch();
    QUrl parsed(explicitUrl ? value : QStringLiteral("http://") + value);
    if (!parsed.isValid() || parsed.host().isEmpty())
        return {};

    parsed = normalizedServerUrl(parsed);
    const QString host = parsed.host();
    const bool lan = isLanHost(host);
    const int suppliedPort = parsed.port(-1);
    QList<QUrl> result;
    QSet<QString> seen;

    const auto add = [&](const QString& scheme, int port) {
        QUrl candidate = parsed;
        candidate.setScheme(scheme);
        candidate.setPort(port);
        appendUniqueUrl(&result, &seen, candidate);
    };

    if (explicitUrl) {
        appendUniqueUrl(&result, &seen, parsed);
        if (parsed.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0)
            add(QStringLiteral("http"), suppliedPort > 0 && suppliedPort != 443 ? suppliedPort : kHttpProbePort);
        else if (parsed.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0 && suppliedPort < 0)
            add(QStringLiteral("http"), kHttpProbePort);
        return result;
    }

    if (lan) {
        add(QStringLiteral("http"), suppliedPort > 0 ? suppliedPort : kHttpProbePort);
        add(QStringLiteral("https"), suppliedPort);
        if (suppliedPort < 0)
            add(QStringLiteral("http"), -1);
    } else {
        add(QStringLiteral("https"), suppliedPort);
        add(QStringLiteral("http"), suppliedPort > 0 ? suppliedPort : kHttpProbePort);
        if (suppliedPort < 0)
            add(QStringLiteral("http"), -1);
    }
    return result;
}

QList<QHostAddress> DiscoveryController::httpFallbackTargets(
    const QHostAddress& address, const QHostAddress& netmask, int maxTargets)
{
    QList<QHostAddress> targets;
    if (maxTargets <= 0 || address.protocol() != QAbstractSocket::IPv4Protocol || !isPrivateNetworkAddress(address))
        return targets;

    const quint32 own = address.toIPv4Address();
    const quint32 mask = netmask.protocol() == QAbstractSocket::IPv4Protocol ? netmask.toIPv4Address() : 0xffffff00u;
    QSet<quint32> seen;

    const quint32 local24 = own & 0xffffff00u;
    appendSubnetTargets(&targets, &seen, local24, local24 | 0xffu, own, maxTargets);

    const quint32 configuredNetwork = own & mask;
    const quint32 configuredBroadcast = configuredNetwork | ~mask;
    const quint64 configuredHosts = static_cast<quint64>(configuredBroadcast) - configuredNetwork - 1;
    if (configuredHosts > 0 && configuredHosts <= static_cast<quint64>(maxTargets)) {
        appendSubnetTargets(&targets, &seen, configuredNetwork, configuredBroadcast, own, maxTargets);
    } else {
        if (local24 >= 0x100u)
            appendSubnetTargets(&targets, &seen, local24 - 0x100u, local24 - 1u, own, maxTargets);
        if (local24 <= 0xfffffe00u)
            appendSubnetTargets(&targets, &seen, local24 + 0x100u, local24 + 0x1ffu, own, maxTargets);
    }
    return targets;
}

DiscoveredServer DiscoveryController::serverFromPublicInfo(
    const QByteArray& payload, const QUrl& serverUrl, QString *version)
{
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject())
        return {};

    const QJsonObject object = document.object();
    const QString id = object.value(QStringLiteral("Id")).toString().trimmed();
    QString name = object.value(QStringLiteral("ServerName")).toString().trimmed();
    if (name.isEmpty())
        name = object.value(QStringLiteral("Name")).toString().trimmed();
    if (id.isEmpty() || name.isEmpty())
        return {};
    if (version)
        *version = object.value(QStringLiteral("Version")).toString().trimmed();
    return { id, name, normalizedServerUrl(serverUrl).toString(QUrl::FullyEncoded) };
}

void DiscoveryController::start()
{
    Diagnostics::Task task(QStringLiteral("discovery_start"));
    if (m_active)
        return;

    m_active = true;
    const bool udpReady = ensureSocket();
    qInfo() << "discovery active" << m_active << "udpReady=" << udpReady;
    emit activeChanged();

    if (udpReady)
        sendProbe();
    m_rescanTimer.start();
    QTimer::singleShot(kInitialHttpFallbackDelayMs, this, &DiscoveryController::startHttpFallbackScan);
}

void DiscoveryController::stop()
{
    Diagnostics::Phase phase(QStringLiteral("shutdown"), QStringLiteral("discovery_stop"),
        { { QStringLiteral("active"), m_active }, { QStringLiteral("inFlightHttpProbes"), m_inFlightHttpProbes } });
    cancelServerProbe();
    if (!m_active)
        return;

    m_active = false;
    m_rescanTimer.stop();
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
    if (!m_active || m_socket.state() != QAbstractSocket::BoundState)
        return;

    m_socket.writeDatagram(QByteArray(kDiscoveryPayload), QHostAddress::Broadcast, kDiscoveryPort);
    qInfo() << "discovery broadcast probe sent";

    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp) || !(iface.flags() & QNetworkInterface::IsRunning)
            || (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol || entry.broadcast().isNull())
                continue;

            m_socket.writeDatagram(QByteArray(kDiscoveryPayload), entry.broadcast(), kDiscoveryPort);
            qInfo() << "discovery interface probe sent";
        }
    }
}

void DiscoveryController::handlePendingDatagrams()
{
    while (m_socket.hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket.receiveDatagram();
        if (datagram.data() == QByteArray(kDiscoveryPayload))
            continue;

        const auto document = QJsonDocument::fromJson(datagram.data());
        if (!document.isObject())
            continue;
        const auto object = document.object();
        if (!object.contains(QStringLiteral("Id")) || !object.contains(QStringLiteral("Name"))
            || !object.contains(QStringLiteral("Address"))) {
            continue;
        }

        qInfo() << "discovery reply accepted";
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
    for (const QNetworkInterface& iface : interfaces) {
        if (m_httpProbeQueue.size() >= kMaxAutomaticHttpProbeTargets)
            break;
        if (!(iface.flags() & QNetworkInterface::IsUp) || !(iface.flags() & QNetworkInterface::IsRunning)
            || (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            if (m_httpProbeQueue.size() >= kMaxAutomaticHttpProbeTargets)
                break;
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol || entry.netmask().isNull())
                continue;

            const auto targets = httpFallbackTargets(
                entry.ip(), entry.netmask(), kMaxAutomaticHttpProbeTargets - m_httpProbeQueue.size());
            for (const QHostAddress& target : targets)
                enqueueHttpProbeTarget(target);
        }
    }

    qInfo() << "http discovery queued" << m_httpProbeQueue.size() << "targets";
    pumpHttpProbeQueue();
}

bool DiscoveryController::ensureSocket()
{
    if (m_socket.state() == QAbstractSocket::BoundState)
        return true;

    return m_socket.bind(
        QHostAddress::AnyIPv4, kDiscoveryPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
}

void DiscoveryController::enqueueHttpProbeTarget(const QHostAddress& address)
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
        Diagnostics::logEvent(QStringLiteral("network"), QStringLiteral("discovery_probe_begin"),
            { { QStringLiteral("host"), host }, { QStringLiteral("inFlight"), m_inFlightHttpProbes } });

        connect(reply, &QNetworkReply::finished, this, [this, reply, serverUrl, host]() {
            m_httpProbeReplies.remove(reply);
            if (reply->error() == QNetworkReply::NoError) {
                const QByteArray payload = reply->readAll();
                qInfo() << "http discovery reply accepted";
                handleHttpProbeResult(serverUrl, payload);
            }

            reply->deleteLater();
            m_inFlightHttpProbes = qMax(0, m_inFlightHttpProbes - 1);
            m_enqueuedHttpProbeTargets.remove(host);
            Diagnostics::logEvent(QStringLiteral("network"), QStringLiteral("discovery_probe_end"),
                { { QStringLiteral("host"), host }, { QStringLiteral("inFlight"), m_inFlightHttpProbes } });
            pumpHttpProbeQueue();
        });
    }
}

void DiscoveryController::handleHttpProbeResult(const QString& serverUrl, const QByteArray& payload)
{
    const DiscoveredServer server = serverFromPublicInfo(payload, QUrl(serverUrl), nullptr);
    if (server.id.isEmpty())
        return;
    emit serverDiscovered(server);
}

void DiscoveryController::probeServer(const QString& input)
{
    cancelServerProbe();
    m_serverProbeInput = input.trimmed();
    const QList<QUrl> candidates = serverProbeCandidates(m_serverProbeInput);
    for (const QUrl& candidate : candidates)
        m_serverProbeCandidates.enqueue(candidate);
    if (m_serverProbeCandidates.isEmpty()) {
        emit serverProbeFailed(m_serverProbeInput, QStringLiteral("Enter a valid Jellyfin server address."));
        return;
    }
    emit serverProbeActiveChanged();
    startNextServerProbe();
}

void DiscoveryController::cancelServerProbe()
{
    const bool wasActive = serverProbeActive();
    m_serverProbeCandidates.clear();
    m_serverProbeInput.clear();
    if (m_serverProbeReply) {
        m_serverProbeReply->disconnect(this);
        m_serverProbeReply->abort();
        m_serverProbeReply->deleteLater();
        m_serverProbeReply = nullptr;
    }
    if (wasActive)
        emit serverProbeActiveChanged();
}

void DiscoveryController::startNextServerProbe()
{
    if (m_serverProbeReply || m_serverProbeCandidates.isEmpty()) {
        if (!m_serverProbeReply && m_serverProbeCandidates.isEmpty())
            finishServerProbe(true);
        return;
    }

    const QUrl serverUrl = m_serverProbeCandidates.dequeue();
    QNetworkRequest request(publicInfoUrl(serverUrl));
    request.setTransferTimeout(kManualProbeTimeoutMs);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Tern/0.1.0"));

    QNetworkReply *reply = m_http.get(request);
    m_serverProbeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (m_serverProbeReply != reply)
            return;
        m_serverProbeReply = nullptr;
        const QByteArray payload = reply->error() == QNetworkReply::NoError ? reply->readAll() : QByteArray {};
        reply->deleteLater();

        QString version;
        const DiscoveredServer server
            = serverFromPublicInfo(payload, serverUrlFromPublicInfoUrl(reply->url()), &version);
        if (!server.id.isEmpty()) {
            const QString input = m_serverProbeInput;
            const bool plainHttp = QUrl(server.address).scheme() == QStringLiteral("http");
            m_serverProbeCandidates.clear();
            m_serverProbeInput.clear();
            emit serverProbeActiveChanged();
            emit serverDiscovered(server);
            emit serverProbeSucceeded(input, server, version, plainHttp);
            return;
        }
        startNextServerProbe();
    });
}

void DiscoveryController::finishServerProbe(bool notifyFailure)
{
    const QString input = m_serverProbeInput;
    const bool wasActive = serverProbeActive() || !input.isEmpty();
    m_serverProbeCandidates.clear();
    m_serverProbeInput.clear();
    if (wasActive)
        emit serverProbeActiveChanged();
    if (notifyFailure)
        emit serverProbeFailed(input, QStringLiteral("No Jellyfin server responded at that address."));
}

} // namespace JellyfinNative
