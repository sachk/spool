#pragma once

#include "../common/JellyfinTypes.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QSslCertificate>
#include <QTimer>
#include <QUdpSocket>
#include <QUrl>

namespace JellyfinNative {

class DiscoveryController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(bool serverProbeActive READ serverProbeActive NOTIFY serverProbeActiveChanged)
    Q_PROPERTY(bool tlsTrustPending READ tlsTrustPending NOTIFY tlsTrustPendingChanged)
    Q_PROPERTY(QString pendingTlsFingerprint READ pendingTlsFingerprint NOTIFY tlsTrustPendingChanged)

public:
    explicit DiscoveryController(QObject *parent = nullptr);
    ~DiscoveryController() override;

    bool active() const;
    bool serverProbeActive() const;
    bool tlsTrustPending() const
    {
        return !m_pendingTlsFingerprint.isEmpty();
    }
    QString pendingTlsFingerprint() const
    {
        return m_pendingTlsFingerprint;
    }

    static QList<QUrl> serverProbeCandidates(const QString& input);
    static QList<QHostAddress> httpFallbackTargets(
        const QHostAddress& address, const QHostAddress& netmask, int maxTargets = 1022);
    static DiscoveredServer serverFromPublicInfo(const QByteArray& payload, const QUrl& serverUrl, QString *version);

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void probeServer(const QString& input);
    Q_INVOKABLE void cancelServerProbe();
    Q_INVOKABLE void trustPendingCertificate();

signals:
    void activeChanged();
    void serverProbeActiveChanged();
    void serverDiscovered(const JellyfinNative::DiscoveredServer& server);
    void serverProbeSucceeded(
        const QString& input, const JellyfinNative::DiscoveredServer& server, const QString& version, bool plainHttp);
    void serverProbeFailed(const QString& input, const QString& message);
    void tlsTrustPendingChanged();

private slots:
    void sendProbe();
    void handlePendingDatagrams();
    void startHttpFallbackScan();

private:
    bool ensureSocket();
    void enqueueHttpProbeTarget(const QHostAddress& address);
    void pumpHttpProbeQueue();
    void handleHttpProbeResult(const QString& serverUrl, const QByteArray& payload);
    void startNextServerProbe();
    void finishServerProbe(bool notifyFailure);

    QUdpSocket m_socket;
    QTimer m_rescanTimer;
    QTimer m_httpFallbackTimer;
    QNetworkAccessManager m_http;
    QQueue<QHostAddress> m_httpProbeQueue;
    QSet<QString> m_enqueuedHttpProbeTargets;
    QSet<QNetworkReply *> m_httpProbeReplies;
    QQueue<QUrl> m_serverProbeCandidates;
    QNetworkReply *m_serverProbeReply = nullptr;
    QString m_serverProbeInput;
    QString m_pendingTlsInput;
    QString m_pendingTlsFingerprint;
    QUrl m_pendingTlsUrl;
    QSslCertificate m_pendingTlsCertificate;
    int m_inFlightHttpProbes = 0;
    bool m_active = false;
};

} // namespace JellyfinNative
