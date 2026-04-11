#pragma once

#include "../common/JellyfinTypes.h"

#include <QCoroTask>

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkRequestFactory>
#include <QObject>
#include <QRestAccessManager>
#include <QString>
#include <QUrlQuery>

#include <vector>

namespace JellyfinNative {

class JellyfinApiFacade final : public QObject
{
    Q_OBJECT

public:
    explicit JellyfinApiFacade(QNetworkAccessManager *networkAccessManager, QObject *parent = nullptr);

    void setServerUrl(const QString &serverUrl);
    QString serverUrl() const;

    void setDeviceIdentity(const QString &deviceId, const QString &deviceName, const QString &clientVersion);
    QString deviceId() const;

    void setSession(const AuthSession &session);
    AuthSession session() const;

    QString buildImageUrl(const QString &itemId, int maxWidth = 360) const;

    QCoro::Task<void> probeServer();
    QCoro::Task<AuthSession> authenticateByName(const QString &username, const QString &password);
    QCoro::Task<bool> quickConnectEnabled();
    QCoro::Task<QJsonObject> initiateQuickConnect();
    QCoro::Task<QJsonObject> pollQuickConnect(const QString &secret);
    QCoro::Task<AuthSession> authenticateWithQuickConnect(const QString &secret);
    QCoro::Task<std::vector<LibraryItem>> fetchLibraries();
    QCoro::Task<std::vector<MovieItem>> fetchMovies(const QString &libraryId);
    QCoro::Task<PlaybackSession> negotiateDirectPlay(const MovieItem &movie);
    QCoro::Task<void> postCapabilities();
    QCoro::Task<void> reportPlaybackStart(const PlaybackSession &session);
    QCoro::Task<void> reportPlaybackProgress(const PlaybackSession &session, qint64 positionTicks, bool paused);
    QCoro::Task<void> reportPlaybackStopped(const PlaybackSession &session, qint64 positionTicks, bool failed);

private:
    enum class HttpMethod {
        Get,
        Post,
    };

    QNetworkRequest createRequest(const QString &path, const QUrlQuery &query = {}) const;
    QString createAuthorizationHeader(const QString &tokenOverride = {}) const;
    QCoro::Task<QJsonDocument> requestJson(HttpMethod method, const QString &path, const QUrlQuery &query = {},
                                           const QJsonDocument &body = {});
    QCoro::Task<void> requestNoContent(HttpMethod method, const QString &path, const QJsonDocument &body);
    QCoro::Task<QByteArray> requestBytes(HttpMethod method, const QString &path, const QUrlQuery &query = {},
                                         const QJsonDocument &body = {});

    QJsonObject buildDeviceProfile() const;
    PlaybackSession buildPlaybackSession(const MovieItem &movie, const QJsonObject &playbackResponse) const;

    QNetworkAccessManager *m_networkAccessManager = nullptr;
    QRestAccessManager m_rest;
    QNetworkRequestFactory m_requestFactory;
    QString m_serverUrl;
    QString m_deviceId;
    QString m_deviceName = QStringLiteral("LG webOS TV");
    QString m_clientVersion = QStringLiteral("0.1.0");
    AuthSession m_session;
};

} // namespace JellyfinNative
