#pragma once

#include "../common/JellyfinTypes.h"
#include "HttpRequestPolicy.h"

#include <QCoroTask>

#include <QDateTime>
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
#include <QVariantMap>

#include <vector>

class QNetworkDiskCache;

namespace JellyfinNative {

class JellyfinApiFacade final : public QObject
{
    Q_OBJECT

public:
    explicit JellyfinApiFacade(QNetworkAccessManager *networkAccessManager, QObject *parent = nullptr);
    ~JellyfinApiFacade() override;

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
    void setPlaybackPreferences(qint64 maxStreamingBitrate, bool preferRemux);
    void setArtworkUiWidth(int width);
    int landscapeCardImageWidth() const;
    int landscapeCardImageQuality() const;

    QString buildImageUrl(const QString &itemId, const QString &tag = {}, int maxWidth = 280,
                          int quality = 75, const QString &format = QStringLiteral("webp"),
                          const QString &imageType = QStringLiteral("Primary"),
                          int fillWidth = 0, int fillHeight = 0) const;
    void setImagePrefetchCache(const QString &cacheDirectory, qint64 maximumCacheSize);
    void prefetchImages(const QStringList &urls, int maxConcurrent = 6);
    void cancelPrefetches();
    void cancelRequests();

    QCoro::Task<void> probeServer();
    QCoro::Task<AuthSession> authenticateByName(QString username, QString password);
    QCoro::Task<bool> quickConnectEnabled();
    QCoro::Task<QJsonObject> initiateQuickConnect();
    QCoro::Task<QJsonObject> pollQuickConnect(QString secret);
    QCoro::Task<AuthSession> authenticateWithQuickConnect(QString secret);
    QCoro::Task<QString> fetchCurrentUserName();
    QCoro::Task<QJsonObject> fetchUserConfiguration();
    QCoro::Task<void> updateUserConfiguration(QJsonObject configuration);
    QCoro::Task<QJsonArray> fetchCultures();
    QCoro::Task<std::vector<LibraryItem>> fetchLibraries();
    QCoro::Task<PagedMovieItems> fetchLibraryPage(QString libraryId, QString collectionType = {},
                                                  int startIndex = 0, int limit = 72,
                                                  QVariantMap queryOptions = {});
    QCoro::Task<QVariantMap> fetchLibraryFilterOptions(QString libraryId, QString collectionType = {});
    QCoro::Task<PagedMovieItems> fetchMoviesPage(QString libraryId, int startIndex = 0, int limit = 100);
    QCoro::Task<PagedMovieItems> fetchSeriesPage(QString libraryId, int startIndex = 0, int limit = 100);
    QCoro::Task<std::vector<MovieItem>> fetchMovies(QString libraryId);
    QCoro::Task<std::vector<MovieItem>> fetchSeries(QString libraryId);
    QCoro::Task<std::vector<MovieItem>> fetchSeasons(QString seriesId);
    QCoro::Task<std::vector<MovieItem>> fetchEpisodes(QString seriesId, QString seasonId = {});
    QCoro::Task<std::vector<MovieItem>> fetchResumeItems(int limit = 24);
    QCoro::Task<std::vector<MovieItem>> fetchNextUpEpisodes(int limit = 24);
    QCoro::Task<std::vector<MovieItem>> fetchLatestItems(QString parentId = {}, int limit = 24);
    QCoro::Task<std::vector<MovieItem>> searchItems(QString searchTerm, int limit = 80);
    QCoro::Task<std::vector<MovieItem>> fetchSearchSuggestions(int limit = 20);
    QCoro::Task<std::vector<MovieItem>> fetchSimilarItems(QString itemId, int limit = 24);
    QCoro::Task<std::vector<MovieItem>> fetchItemsByPerson(QString personId, int limit = 80);
    QCoro::Task<std::vector<MovieItem>> fetchItemsByGenre(QString genre, int limit = 200);
    QCoro::Task<std::vector<MovieItem>> fetchItemsByStudio(QString studio, int limit = 200);
    QCoro::Task<void> setItemFavorite(QString itemId, bool favorite);
    QCoro::Task<void> setItemPlayed(QString itemId, bool played);
    QCoro::Task<void> setItemPlaybackPosition(QString itemId, qint64 positionTicks);
    QCoro::Task<std::vector<MediaSegment>> fetchMediaSegments(QString itemId);
    QCoro::Task<TrickplayInfo> fetchTrickplay(QString itemId, QString mediaSourceId, int preferredWidth = 320);
    QString trickplayTileUrl(const QString &itemId, int width, int tileIndex) const;
    QCoro::Task<PlaybackSession> negotiatePlayback(MovieItem movie);

    // SyncPlay REST endpoints used alongside SyncPlayController's WebSocket.
    QCoro::Task<QJsonArray> fetchSyncPlayGroups();
    QCoro::Task<void> createSyncPlayGroup(QString name);
    QCoro::Task<void> joinSyncPlayGroup(QString groupId);
    QCoro::Task<void> leaveSyncPlayGroup();
    QCoro::Task<void> syncPlayRequestPlay();
    QCoro::Task<void> syncPlayRequestPause();
    QCoro::Task<void> syncPlayRequestSeek(qint64 positionTicks);
    QCoro::Task<QJsonObject> fetchUtcTime();
    QCoro::Task<void> syncPlayReportPing(qint64 pingMs);
    QCoro::Task<void> syncPlayReportBuffering(
        bool buffering, qint64 positionTicks, bool playing,
        QString playlistItemId, QDateTime serverTime);

    QCoro::Task<void> postCapabilities();
    QCoro::Task<void> reportPlaybackStart(PlaybackSession session);
    QCoro::Task<void> reportPlaybackProgress(PlaybackSession session, qint64 positionTicks, bool paused);
    QCoro::Task<void> reportPlaybackStopped(PlaybackSession session, qint64 positionTicks, bool failed);

signals:
    void authenticationExpired(const QString &message);

private:
    enum class HttpMethod {
        Get,
        Post,
        Delete,
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
    HttpOperation operationFor(HttpMethod method, const QString &path) const;
    bool shouldExpireSession(const QString &path) const;

    QNetworkAccessManager *m_networkAccessManager = nullptr;
    QNetworkAccessManager *m_imagePrefetchNetworkAccessManager = nullptr;
    QNetworkDiskCache *m_imagePrefetchDiskCache = nullptr;
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
    QSet<QNetworkReply *> m_activeReplies;
    int m_prefetchInFlight = 0;
    int m_prefetchMaxConcurrent = 6;
    int m_artworkUiWidth = 1920;
    qint64 m_maxStreamingBitrate = 120'000'000;
    bool m_preferRemux = true;
    bool m_authExpirationReported = false;
    bool m_shuttingDown = false;
};

} // namespace JellyfinNative
