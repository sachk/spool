#pragma once

#include "../common/JellyfinTypes.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QQueue>
#include <QSet>
#include <QObject>
#include <QTimer>
#include <QUdpSocket>

namespace JellyfinNative {

class DiscoveryController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)

public:
    explicit DiscoveryController(QObject *parent = nullptr);
    ~DiscoveryController() override;

    bool active() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

signals:
    void activeChanged();
    void serverDiscovered(const JellyfinNative::DiscoveredServer &server);

private slots:
    void sendProbe();
    void handlePendingDatagrams();
    void startHttpFallbackScan();

private:
    bool ensureSocket();
    void enqueueHttpProbeTarget(const QHostAddress &address);
    void pumpHttpProbeQueue();
    void handleHttpProbeResult(const QString &serverUrl, const QByteArray &payload);

    QUdpSocket m_socket;
    QTimer m_rescanTimer;
    QTimer m_httpFallbackTimer;
    QNetworkAccessManager m_http;
    QQueue<QHostAddress> m_httpProbeQueue;
    QSet<QString> m_enqueuedHttpProbeTargets;
    QSet<QNetworkReply *> m_httpProbeReplies;
    int m_inFlightHttpProbes = 0;
    bool m_active = false;
};

} // namespace JellyfinNative
