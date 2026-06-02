#pragma once

#include "../common/JellyfinTypes.h"

#include <QCoroTask>

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequestFactory>
#include <QObject>
#include <QSet>
#include <QRestAccessManager>
#include <QString>
#include <QStringList>
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

    // Forwarded into the QNetworkRequestFactory common headers so every API
    // call hints the server about our locale. Jellyfin uses this to localise
    // server-returned strings (Continue Watching titles, etc.).
    void setAcceptLanguage(const QString &bcp47Tag);

    void setSession(const AuthSession &session);
    AuthSession session() const;

    QString buildImageUrl(const QString &itemId, const QString &tag = {}, int maxWidth = 280,
                          int quality = 75, const QString &format = QStringLiteral("webp")) const;
    void prefetchImages(const QStringList &urls, int maxConcurrent = 6);
    void cancelPrefetches();

    QCoro::Task<void> probeServer();
    QCoro::Task<AuthSession> authenticateByName(QString username, QString password);
    QCoro::Task<bool> quickConnectEnabled();
    QCoro::Task<QJsonObject> initiateQuickConnect();
    QCoro::Task<QJsonObject> pollQuickConnect(QString secret);
    QCoro::Task<AuthSession> authenticateWithQuickConnect(QString secret);
    QCoro::Task<std::vector<LibraryItem>> fetchLibraries();
    QCoro::Task<std::vector<MovieItem>> fetchMovies(QString libraryId);
    QCoro::Task<std::vector<MovieItem>> fetchSeries(QString libraryId);
    QCoro::Task<std::vector<MovieItem>> fetchSeasons(QString seriesId);
    QCoro::Task<std::vector<MovieItem>> fetchEpisodes(QString seriesId, QString seasonId = {});
    QCoro::Task<std::vector<MovieItem>> fetchResumeItems(int limit = 24);
    QCoro::Task<std::vector<MovieItem>> fetchNextUpEpisodes(int limit = 24);
    QCoro::Task<std::vector<MovieItem>> fetchLatestItems(QString parentId = {}, int limit = 24);
    QCoro::Task<std::vector<MovieItem>> searchItems(QString searchTerm, int limit = 80);
    QCoro::Task<std::vector<MediaSegment>> fetchMediaSegments(QString itemId);
    QCoro::Task<TrickplayInfo> fetchTrickplay(QString itemId, QString mediaSourceId, int preferredWidth = 320);
    QString trickplayTileUrl(const QString &itemId, int width, int tileIndex) const;
    QCoro::Task<PlaybackSession> negotiateDirectPlay(MovieItem movie);

    // SyncPlay REST endpoints. These mirror the calls the official web client
    // makes. Realtime sync still requires a WebSocket connection (Qt
    // WebSockets module not yet built into our static Qt 6.11) — the methods
    // below provide the group-management half today.
    QCoro::Task<QJsonArray> fetchSyncPlayGroups();
    QCoro::Task<void> createSyncPlayGroup(QString name);
    QCoro::Task<void> joinSyncPlayGroup(QString groupId);
    QCoro::Task<void> leaveSyncPlayGroup();
    QCoro::Task<void> syncPlayRequestPlay();
    QCoro::Task<void> syncPlayRequestPause();
    QCoro::Task<void> syncPlayRequestSeek(qint64 positionTicks);

    QCoro::Task<void> postCapabilities();
    QCoro::Task<void> reportPlaybackStart(PlaybackSession session);
    QCoro::Task<void> reportPlaybackProgress(PlaybackSession session, qint64 positionTicks, bool paused);
    QCoro::Task<void> reportPlaybackStopped(PlaybackSession session, qint64 positionTicks, bool failed);

private:
    enum class HttpMethod {
        Get,
        Post,
    };

    QNetworkRequest createRequest(const QString &path, const QUrlQuery &query = {}) const;
    QString createAuthorizationHeader(const QString &tokenOverride = {}) const;
    QCoro::Task<QJsonDocument> requestJson(HttpMethod method, QString path, QUrlQuery query = {},
                                           QJsonDocument body = {});
    QCoro::Task<void> requestNoContent(HttpMethod method, QString path, QJsonDocument body);
    QCoro::Task<QByteArray> requestBytes(HttpMethod method, QString path, QUrlQuery query = {},
                                         QJsonDocument body = {});

    QJsonObject buildDeviceProfile() const;
    PlaybackSession buildPlaybackSession(const MovieItem &movie, const QJsonObject &playbackResponse) const;
    void pumpImagePrefetch();

    QNetworkAccessManager *m_networkAccessManager = nullptr;
    QRestAccessManager m_rest;
    QNetworkRequestFactory m_requestFactory;
    QString m_serverUrl;
    QString m_deviceId;
    QString m_deviceName = QStringLiteral("LG webOS TV");
    QString m_clientVersion = QStringLiteral("0.1.0");
    AuthSession m_session;
    QStringList m_prefetchQueue;
    QSet<QString> m_prefetchSeen;
    QSet<QNetworkReply *> m_prefetchReplies;
    int m_prefetchInFlight = 0;
    int m_prefetchMaxConcurrent = 6;
};

} // namespace JellyfinNative
