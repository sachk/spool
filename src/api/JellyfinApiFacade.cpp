#include "JellyfinApiFacade.h"

#include <QCoroNetwork>

#include <QHttpHeaders>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <stdexcept>

namespace JellyfinNative {

namespace {

QString requireString(const QJsonObject &object, const QString &key)
{
    const QString value = object.value(key).toString();
    if (value.isEmpty())
        throw std::runtime_error(QStringLiteral("Missing field: %1").arg(key).toStdString());
    return value;
}

QString cleanContainerName(const QString &container)
{
    if (container.contains(QLatin1Char(',')))
        return container.section(QLatin1Char(','), 0, 0).trimmed();
    return container.trimmed();
}

bool isQuickConnectPath(const QString &path)
{
    return path.startsWith(QStringLiteral("/QuickConnect/")) ||
           path == QStringLiteral("/Users/AuthenticateWithQuickConnect");
}

QJsonArray itemsArrayFromDocument(const QJsonDocument &document)
{
    if (document.isArray())
        return document.array();
    return document.object().value(QStringLiteral("Items")).toArray();
}

QString episodeSubtitle(const QJsonObject &object)
{
    const int season = object.value(QStringLiteral("ParentIndexNumber")).toInt();
    const int episode = object.value(QStringLiteral("IndexNumber")).toInt();
    if (season > 0 && episode > 0)
        return QStringLiteral("S%1:E%2").arg(season, 2, 10, QLatin1Char('0')).arg(episode, 2, 10, QLatin1Char('0'));
    if (episode > 0)
        return QStringLiteral("Episode %1").arg(episode);
    return QStringLiteral("Episode");
}

MovieItem mediaItemFromJson(const JellyfinApiFacade *api, const QJsonObject &object)
{
    const QString itemId = object.value(QStringLiteral("Id")).toString();
    const QString itemType = object.value(QStringLiteral("Type")).toString();
    const QString posterTag = object.value(QStringLiteral("ImageTags")).toObject().value(QStringLiteral("Primary")).toString();
    QString subtitle;
    bool playable = itemType == QStringLiteral("Movie") || itemType == QStringLiteral("Episode");

    if (itemType == QStringLiteral("Series")) {
        const int year = object.value(QStringLiteral("ProductionYear")).toInt();
        subtitle = year > 0 ? QString::number(year) : QStringLiteral("Series");
        playable = false;
    } else if (itemType == QStringLiteral("Season")) {
        subtitle = QStringLiteral("Season");
        playable = false;
    } else if (itemType == QStringLiteral("Episode")) {
        subtitle = episodeSubtitle(object);
    } else if (object.value(QStringLiteral("ProductionYear")).toInt() > 0) {
        subtitle = QString::number(object.value(QStringLiteral("ProductionYear")).toInt());
    }

    const int indexNumber = object.value(QStringLiteral("IndexNumber")).toInt();
    const int parentIndexNumber = object.value(QStringLiteral("ParentIndexNumber")).toInt();
    const qint64 resumeTicks = object.value(QStringLiteral("UserData"))
                                  .toObject()
                                  .value(QStringLiteral("PlaybackPositionTicks"))
                                  .toVariant()
                                  .toLongLong();

    return {
        itemId,
        object.value(QStringLiteral("Name")).toString(),
        object.value(QStringLiteral("Overview")).toString(),
        api->buildImageUrl(itemId, posterTag),
        posterTag,
        itemType,
        object.value(QStringLiteral("SeriesId")).toString(),
        subtitle,
        object.value(QStringLiteral("Path")).toString(),
        object.value(QStringLiteral("ProductionYear")).toInt(),
        itemType == QStringLiteral("Episode") ? parentIndexNumber : indexNumber,
        itemType == QStringLiteral("Episode") ? indexNumber : 0,
        resumeTicks,
        playable,
    };
}

}

JellyfinApiFacade::JellyfinApiFacade(QNetworkAccessManager *networkAccessManager, QObject *parent)
    : QObject(parent)
    , m_networkAccessManager(networkAccessManager)
    , m_rest(networkAccessManager, this)
{
    m_requestFactory.setTransferTimeout(std::chrono::seconds(30));

    QHttpHeaders headers;
    headers.append(QHttpHeaders::WellKnownHeader::Accept, QStringLiteral("application/json"));
    m_requestFactory.setCommonHeaders(headers);
}

void JellyfinApiFacade::setServerUrl(const QString &serverUrl)
{
    m_serverUrl = serverUrl;
    while (m_serverUrl.endsWith(QLatin1Char('/')))
        m_serverUrl.chop(1);
    m_requestFactory.setBaseUrl(QUrl(m_serverUrl));
}

QString JellyfinApiFacade::serverUrl() const
{
    return m_serverUrl;
}

void JellyfinApiFacade::setDeviceIdentity(const QString &deviceId, const QString &deviceName, const QString &clientVersion)
{
    m_deviceId = deviceId;
    m_deviceName = deviceName;
    m_clientVersion = clientVersion;
}

QString JellyfinApiFacade::deviceId() const
{
    return m_deviceId;
}

void JellyfinApiFacade::setSession(const AuthSession &session)
{
    m_session = session;
}

AuthSession JellyfinApiFacade::session() const
{
    return m_session;
}

QString JellyfinApiFacade::buildImageUrl(const QString &itemId, const QString &tag, int maxWidth,
                                         int quality, const QString &format) const
{
    QUrl url(QStringLiteral("%1/Items/%2/Images/Primary").arg(m_serverUrl, itemId));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("maxWidth"), QString::number(maxWidth));
    query.addQueryItem(QStringLiteral("quality"), QString::number(quality));
    query.addQueryItem(QStringLiteral("format"), format);
    query.addQueryItem(QStringLiteral("api_key"), m_session.accessToken);
    if (!tag.isEmpty())
        query.addQueryItem(QStringLiteral("tag"), tag);
    url.setQuery(query);
    return url.toString(QUrl::FullyEncoded);
}

void JellyfinApiFacade::prefetchImages(const QStringList &urls, int maxConcurrent)
{
    if (!m_networkAccessManager)
        return;

    m_prefetchMaxConcurrent = std::max(1, maxConcurrent);
    for (const QString &url : urls) {
        if (url.isEmpty() || m_prefetchSeen.contains(url))
            continue;
        m_prefetchSeen.insert(url);
        m_prefetchQueue.push_back(url);
    }

    pumpImagePrefetch();
}

QCoro::Task<void> JellyfinApiFacade::probeServer()
{
    co_await requestJson(HttpMethod::Get, QStringLiteral("/System/Info/Public"));
}

QCoro::Task<AuthSession> JellyfinApiFacade::authenticateByName(const QString &username, const QString &password)
{
    const QJsonDocument response =
        co_await requestJson(HttpMethod::Post, QStringLiteral("/Users/AuthenticateByName"), {},
                             QJsonDocument(QJsonObject{
                                 {QStringLiteral("Username"), username},
                                 {QStringLiteral("Pw"), password},
                             }));

    const QJsonObject object = response.object();
    const QJsonObject user = object.value(QStringLiteral("User")).toObject();
    const AuthSession session{
        requireString(user, QStringLiteral("Id")),
        requireString(user, QStringLiteral("Name")),
        requireString(object, QStringLiteral("AccessToken")),
        object.value(QStringLiteral("ServerId")).toString(),
    };
    setSession(session);
    co_return session;
}

QCoro::Task<bool> JellyfinApiFacade::quickConnectEnabled()
{
    const QByteArray response = co_await requestBytes(HttpMethod::Get, QStringLiteral("/QuickConnect/Enabled"));
    co_return response.trimmed() == QByteArrayLiteral("true");
}

QCoro::Task<QJsonObject> JellyfinApiFacade::initiateQuickConnect()
{
    const QJsonDocument response = co_await requestJson(HttpMethod::Post, QStringLiteral("/QuickConnect/Initiate"));
    co_return response.object();
}

QCoro::Task<QJsonObject> JellyfinApiFacade::pollQuickConnect(const QString &secret)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("secret"), secret);
    const QJsonDocument response = co_await requestJson(HttpMethod::Get, QStringLiteral("/QuickConnect/Connect"), query);
    co_return response.object();
}

QCoro::Task<AuthSession> JellyfinApiFacade::authenticateWithQuickConnect(const QString &secret)
{
    const QJsonDocument response =
        co_await requestJson(HttpMethod::Post, QStringLiteral("/Users/AuthenticateWithQuickConnect"), {},
                             QJsonDocument(QJsonObject{
                                 {QStringLiteral("Secret"), secret},
                             }));

    const QJsonObject object = response.object();
    const QJsonObject user = object.value(QStringLiteral("User")).toObject();
    const AuthSession session{
        requireString(user, QStringLiteral("Id")),
        requireString(user, QStringLiteral("Name")),
        requireString(object, QStringLiteral("AccessToken")),
        object.value(QStringLiteral("ServerId")).toString(),
    };
    setSession(session);
    co_return session;
}

QCoro::Task<std::vector<LibraryItem>> JellyfinApiFacade::fetchLibraries()
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), m_session.userId);
    query.addQueryItem(QStringLiteral("includeHidden"), QStringLiteral("false"));
    query.addQueryItem(QStringLiteral("presetViews"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary"));
    query.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("1"));

    const QJsonArray items =
        (co_await requestJson(HttpMethod::Get, QStringLiteral("/UserViews"), query)).object().value(QStringLiteral("Items")).toArray();

    std::vector<LibraryItem> libraries;
    libraries.reserve(items.size());
    for (const auto &value : items) {
        const auto object = value.toObject();
        const QString itemId = object.value(QStringLiteral("Id")).toString();
        const QString imageTag = object.value(QStringLiteral("ImageTags")).toObject().value(QStringLiteral("Primary")).toString();
        libraries.push_back({
            itemId,
            object.value(QStringLiteral("Name")).toString(),
            object.value(QStringLiteral("CollectionType")).toString(),
            buildImageUrl(itemId, imageTag),
            imageTag,
        });
    }

    co_return libraries;
}

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchMovies(const QString &libraryId)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), m_session.userId);
    query.addQueryItem(QStringLiteral("parentId"), libraryId);
    query.addQueryItem(QStringLiteral("recursive"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("includeItemTypes"), QStringLiteral("Movie"));
    query.addQueryItem(QStringLiteral("fields"), QStringLiteral("Overview,ProductionYear,ImageTags,UserData,Path"));
    query.addQueryItem(QStringLiteral("sortBy"), QStringLiteral("SortName"));
    query.addQueryItem(QStringLiteral("sortOrder"), QStringLiteral("Ascending"));
    query.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary"));
    query.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("500"));

    const QJsonArray items =
        (co_await requestJson(HttpMethod::Get, QStringLiteral("/Items"), query)).object().value(QStringLiteral("Items")).toArray();

    std::vector<MovieItem> movies;
    movies.reserve(items.size());
    for (const auto &value : items) {
        const auto object = value.toObject();
        if (object.value(QStringLiteral("Type")).toString() != QStringLiteral("Movie"))
            continue;

        const QString itemId = object.value(QStringLiteral("Id")).toString();
        const QString posterTag = object.value(QStringLiteral("ImageTags")).toObject().value(QStringLiteral("Primary")).toString();
        const qint64 resumeTicks = object.value(QStringLiteral("UserData"))
                                      .toObject()
                                      .value(QStringLiteral("PlaybackPositionTicks"))
                                      .toVariant()
                                      .toLongLong();
        movies.push_back({
            itemId,
            object.value(QStringLiteral("Name")).toString(),
            object.value(QStringLiteral("Overview")).toString(),
            buildImageUrl(itemId, posterTag),
            posterTag,
            QStringLiteral("Movie"),
            {},
            object.value(QStringLiteral("ProductionYear")).toInt() > 0
                ? QString::number(object.value(QStringLiteral("ProductionYear")).toInt())
                : QStringLiteral("Movie"),
            object.value(QStringLiteral("Path")).toString(),
            object.value(QStringLiteral("ProductionYear")).toInt(),
            0,
            0,
            resumeTicks,
            true,
        });
    }

    co_return movies;
}

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchSeries(const QString &libraryId)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), m_session.userId);
    query.addQueryItem(QStringLiteral("parentId"), libraryId);
    query.addQueryItem(QStringLiteral("recursive"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("includeItemTypes"), QStringLiteral("Series"));
    query.addQueryItem(QStringLiteral("fields"), QStringLiteral("Overview,ProductionYear,ImageTags,UserData,Path"));
    query.addQueryItem(QStringLiteral("sortBy"), QStringLiteral("SortName"));
    query.addQueryItem(QStringLiteral("sortOrder"), QStringLiteral("Ascending"));
    query.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary"));
    query.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("500"));

    const QJsonArray items =
        (co_await requestJson(HttpMethod::Get, QStringLiteral("/Items"), query)).object().value(QStringLiteral("Items")).toArray();

    std::vector<MovieItem> series;
    series.reserve(items.size());
    for (const auto &value : items) {
        const auto object = value.toObject();
        if (object.value(QStringLiteral("Type")).toString() == QStringLiteral("Series"))
            series.push_back(mediaItemFromJson(this, object));
    }

    co_return series;
}

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchSeasons(const QString &seriesId)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), m_session.userId);
    query.addQueryItem(QStringLiteral("fields"), QStringLiteral("Overview,ImageTags,UserData,Path"));
    query.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary"));
    query.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("1"));

    const QJsonArray items =
        (co_await requestJson(HttpMethod::Get, QStringLiteral("/Shows/%1/Seasons").arg(seriesId), query))
            .object()
            .value(QStringLiteral("Items"))
            .toArray();

    std::vector<MovieItem> seasons;
    seasons.reserve(items.size());
    for (const auto &value : items) {
        const auto object = value.toObject();
        if (object.value(QStringLiteral("Type")).toString() == QStringLiteral("Season"))
            seasons.push_back(mediaItemFromJson(this, object));
    }

    co_return seasons;
}

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchEpisodes(const QString &seriesId, const QString &seasonId)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), m_session.userId);
    if (!seasonId.isEmpty())
        query.addQueryItem(QStringLiteral("seasonId"), seasonId);
    query.addQueryItem(QStringLiteral("fields"), QStringLiteral("Overview,ImageTags,UserData,Path"));

    const QJsonArray items =
        (co_await requestJson(HttpMethod::Get, QStringLiteral("/Shows/%1/Episodes").arg(seriesId), query))
            .object()
            .value(QStringLiteral("Items"))
            .toArray();

    std::vector<MovieItem> episodes;
    episodes.reserve(items.size());
    for (const auto &value : items) {
        const auto object = value.toObject();
        if (object.value(QStringLiteral("Type")).toString() == QStringLiteral("Episode"))
            episodes.push_back(mediaItemFromJson(this, object));
    }

    co_return episodes;
}

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchResumeItems(int limit)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("UserId"), m_session.userId);
    query.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    query.addQueryItem(QStringLiteral("recursive"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("fields"), QStringLiteral("Overview,ProductionYear,ImageTags,UserData,Path"));
    query.addQueryItem(QStringLiteral("includeItemTypes"), QStringLiteral("Movie,Episode"));
    query.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary"));
    query.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("mediaTypes"), QStringLiteral("Video"));

    const QJsonArray items =
        itemsArrayFromDocument(co_await requestJson(HttpMethod::Get,
                                                    QStringLiteral("/Users/%1/Items/Resume").arg(m_session.userId),
                                                    query));

    std::vector<MovieItem> result;
    result.reserve(items.size());
    for (const auto &value : items) {
        auto item = mediaItemFromJson(this, value.toObject());
        if (item.playable && item.resumeTicks > 0)
            result.push_back(item);
    }
    co_return result;
}

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchNextUpEpisodes(int limit)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("UserId"), m_session.userId);
    query.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    query.addQueryItem(QStringLiteral("fields"), QStringLiteral("Overview,ImageTags,UserData,Path"));
    query.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary"));
    query.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("enableResumable"), QStringLiteral("false"));

    const QJsonArray items =
        itemsArrayFromDocument(co_await requestJson(HttpMethod::Get, QStringLiteral("/Shows/NextUp"), query));

    std::vector<MovieItem> result;
    result.reserve(items.size());
    for (const auto &value : items) {
        auto item = mediaItemFromJson(this, value.toObject());
        if (item.itemType == QStringLiteral("Episode") && item.playable)
            result.push_back(item);
    }
    co_return result;
}

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchLatestItems(const QString &parentId, int limit)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    query.addQueryItem(QStringLiteral("fields"), QStringLiteral("Overview,ProductionYear,ImageTags,UserData,Path"));
    query.addQueryItem(QStringLiteral("includeItemTypes"), QStringLiteral("Movie,Series,Episode"));
    query.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary"));
    query.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("groupItems"), QStringLiteral("true"));
    if (!parentId.isEmpty())
        query.addQueryItem(QStringLiteral("parentId"), parentId);

    const QJsonDocument doc =
        co_await requestJson(HttpMethod::Get,
                             QStringLiteral("/Users/%1/Items/Latest").arg(m_session.userId),
                             query);

    const QJsonArray items = itemsArrayFromDocument(doc);

    std::vector<MovieItem> result;
    result.reserve(items.size());
    for (const auto &value : items)
        result.push_back(mediaItemFromJson(this, value.toObject()));
    co_return result;
}

QCoro::Task<PlaybackSession> JellyfinApiFacade::negotiateDirectPlay(const MovieItem &movie)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), m_session.userId);

    const QJsonObject body = {
        {QStringLiteral("UserId"), m_session.userId},
        {QStringLiteral("MaxStreamingBitrate"), 140000000},
        {QStringLiteral("StartTimeTicks"), movie.resumeTicks},
        {QStringLiteral("AutoOpenLiveStream"), true},
        {QStringLiteral("EnableDirectPlay"), true},
        {QStringLiteral("EnableDirectStream"), true},
        {QStringLiteral("EnableTranscoding"), false},
        {QStringLiteral("AllowVideoStreamCopy"), true},
        {QStringLiteral("AllowAudioStreamCopy"), true},
        {QStringLiteral("DeviceProfile"), buildDeviceProfile()},
    };

    const QJsonObject playbackResponse =
        (co_await requestJson(HttpMethod::Post,
                              QStringLiteral("/Items/%1/PlaybackInfo").arg(movie.id),
                              query,
                              QJsonDocument(body)))
            .object();

    co_return buildPlaybackSession(movie, playbackResponse);
}

QCoro::Task<void> JellyfinApiFacade::postCapabilities()
{
    const QJsonObject body = {
        {QStringLiteral("PlayableMediaTypes"), QJsonArray{QStringLiteral("Video"), QStringLiteral("Audio")}},
        {QStringLiteral("SupportedCommands"),
         QJsonArray{
             QStringLiteral("MoveUp"),
             QStringLiteral("MoveDown"),
             QStringLiteral("MoveLeft"),
             QStringLiteral("MoveRight"),
             QStringLiteral("Select"),
             QStringLiteral("Back"),
             QStringLiteral("SetAudioStreamIndex"),
             QStringLiteral("SetSubtitleStreamIndex"),
             QStringLiteral("ToggleOsd"),
         }},
        {QStringLiteral("SupportsMediaControl"), true},
        {QStringLiteral("SupportsPersistentIdentifier"), true},
        {QStringLiteral("DeviceProfile"), buildDeviceProfile()},
    };

    co_await requestNoContent(HttpMethod::Post, QStringLiteral("/Sessions/Capabilities/Full"), QJsonDocument(body));
}

QCoro::Task<void> JellyfinApiFacade::reportPlaybackStart(const PlaybackSession &session)
{
    const QJsonObject body = {
        {QStringLiteral("CanSeek"), true},
        {QStringLiteral("ItemId"), session.itemId},
        {QStringLiteral("MediaSourceId"), session.mediaSourceId},
        {QStringLiteral("PlayMethod"), QStringLiteral("DirectPlay")},
        {QStringLiteral("PlaySessionId"), session.playSessionId},
        {QStringLiteral("PositionTicks"), 0},
    };

    co_await requestNoContent(HttpMethod::Post, QStringLiteral("/Sessions/Playing"), QJsonDocument(body));
}

QCoro::Task<void> JellyfinApiFacade::reportPlaybackProgress(const PlaybackSession &session, qint64 positionTicks, bool paused)
{
    const QJsonObject body = {
        {QStringLiteral("CanSeek"), true},
        {QStringLiteral("ItemId"), session.itemId},
        {QStringLiteral("MediaSourceId"), session.mediaSourceId},
        {QStringLiteral("PlayMethod"), QStringLiteral("DirectPlay")},
        {QStringLiteral("PlaySessionId"), session.playSessionId},
        {QStringLiteral("PositionTicks"), positionTicks},
        {QStringLiteral("IsPaused"), paused},
    };

    co_await requestNoContent(HttpMethod::Post, QStringLiteral("/Sessions/Playing/Progress"), QJsonDocument(body));
}

QCoro::Task<void> JellyfinApiFacade::reportPlaybackStopped(const PlaybackSession &session, qint64 positionTicks, bool failed)
{
    const QJsonObject body = {
        {QStringLiteral("ItemId"), session.itemId},
        {QStringLiteral("MediaSourceId"), session.mediaSourceId},
        {QStringLiteral("PlaySessionId"), session.playSessionId},
        {QStringLiteral("PositionTicks"), positionTicks},
        {QStringLiteral("Failed"), failed},
    };

    co_await requestNoContent(HttpMethod::Post, QStringLiteral("/Sessions/Playing/Stopped"), QJsonDocument(body));
}

QNetworkRequest JellyfinApiFacade::createRequest(const QString &path, const QUrlQuery &query) const
{
    QNetworkRequest request = query.isEmpty() ? m_requestFactory.createRequest(path)
                                              : m_requestFactory.createRequest(path, query);
    request.setRawHeader("X-Emby-Authorization", createAuthorizationHeader().toUtf8());
    return request;
}

QString JellyfinApiFacade::createAuthorizationHeader(const QString &tokenOverride) const
{
    QStringList parts{
        QStringLiteral("Client=\"Jellyfin Native\""),
        QStringLiteral("Device=\"%1\"").arg(m_deviceName),
        QStringLiteral("DeviceId=\"%1\"").arg(m_deviceId),
        QStringLiteral("Version=\"%1\"").arg(m_clientVersion),
    };

    const QString token = tokenOverride.isEmpty() ? m_session.accessToken : tokenOverride;
    if (!token.isEmpty())
        parts.push_back(QStringLiteral("Token=\"%1\"").arg(token));
    return QStringLiteral("MediaBrowser %1").arg(parts.join(QStringLiteral(", ")));
}

QCoro::Task<QJsonDocument> JellyfinApiFacade::requestJson(HttpMethod method, const QString &path, const QUrlQuery &query,
                                                          const QJsonDocument &body)
{
    const QByteArray payload = co_await requestBytes(method, path, query, body);
    if (payload.isEmpty())
        co_return QJsonDocument();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        throw std::runtime_error(parseError.errorString().toStdString());
    co_return document;
}

QCoro::Task<void> JellyfinApiFacade::requestNoContent(HttpMethod method, const QString &path, const QJsonDocument &body)
{
    co_await requestBytes(method, path, {}, body);
}

QCoro::Task<QByteArray> JellyfinApiFacade::requestBytes(HttpMethod method, const QString &path, const QUrlQuery &query,
                                                        const QJsonDocument &body)
{
    const QNetworkRequest request = createRequest(path, query);
    QNetworkReply *reply = nullptr;

    if (isQuickConnectPath(path)) {
        qInfo() << "api:" << (method == HttpMethod::Get ? "GET" : "POST")
                << request.url().toString(QUrl::FullyEncoded)
                << "deviceId" << m_deviceId;
    }

    switch (method) {
    case HttpMethod::Get:
        reply = m_rest.get(request);
        break;
    case HttpMethod::Post:
        reply = body.isNull() ? m_rest.post(request, QByteArray{})
                              : m_rest.post(request, body);
        break;
    }

    reply = co_await QCoro::waitFor(reply);
    const QByteArray payload = reply ? reply->readAll() : QByteArray{};
    const QString errorText = reply ? reply->errorString() : QStringLiteral("Network reply disappeared");
    const int statusCode =
        reply ? reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() : 500;
    const auto networkError = reply ? reply->error() : QNetworkReply::UnknownNetworkError;
    if (reply)
        reply->deleteLater();

    if (networkError != QNetworkReply::NoError || statusCode >= 400) {
        const QString details = payload.isEmpty() ? errorText : QString::fromUtf8(payload);
        if (isQuickConnectPath(path))
            qWarning() << "api:" << path << "failed" << statusCode << details;
        throw std::runtime_error(QStringLiteral("%1 (%2)").arg(details).arg(statusCode).toStdString());
    }

    if (isQuickConnectPath(path)) {
        qInfo() << "api:" << path << "ok" << statusCode
                << QString::fromUtf8(payload.left(256));
    }

    co_return payload;
}

QJsonObject JellyfinApiFacade::buildDeviceProfile() const
{
    // Unrestricted direct play — mpv handles any container/codec.
    // Matches jellyfin-mpv-shim's approach so the server always marks
    // SupportsDirectPlay / SupportsDirectStream as true.
    return {
        {QStringLiteral("Name"), QStringLiteral("JellyfinNativeWebOS")},
        {QStringLiteral("MaxStreamingBitrate"), 140000000},
        {QStringLiteral("MaxStaticBitrate"), 140000000},
        {QStringLiteral("MusicStreamingTranscodingBitrate"), 1280000},
        {QStringLiteral("DirectPlayProfiles"),
         QJsonArray{
             QJsonObject{{QStringLiteral("Type"), QStringLiteral("Video")}},
             QJsonObject{{QStringLiteral("Type"), QStringLiteral("Audio")}},
         }},
        {QStringLiteral("TranscodingProfiles"), QJsonArray{}},
        {QStringLiteral("ContainerProfiles"), QJsonArray{}},
        {QStringLiteral("CodecProfiles"), QJsonArray{}},
        {QStringLiteral("SubtitleProfiles"), QJsonArray{}},
        {QStringLiteral("ResponseProfiles"), QJsonArray{}},
    };
}

PlaybackSession JellyfinApiFacade::buildPlaybackSession(const MovieItem &movie, const QJsonObject &playbackResponse) const
{
    const QJsonArray mediaSources = playbackResponse.value(QStringLiteral("MediaSources")).toArray();
    if (mediaSources.isEmpty())
        throw std::runtime_error("No media sources returned by Jellyfin");

    // Pick the best source: prefer highest bitrate that supports direct play,
    // matching jellyfin-mpv-shim's get_best_media_source logic.
    QJsonObject selectedSource;
    qint64 bestWeight = -1;
    for (const auto &value : mediaSources) {
        const auto candidate = value.toObject();
        const qint64 weight =
            (candidate.value(QStringLiteral("SupportsDirectPlay")).toBool() ? 50000000LL : 0) +
            (candidate.value(QStringLiteral("SupportsDirectStream")).toBool() ? 25000000LL : 0) +
            candidate.value(QStringLiteral("Bitrate")).toInteger(0) / 1000;
        if (weight > bestWeight) {
            bestWeight = weight;
            selectedSource = candidate;
        }
    }
    if (selectedSource.isEmpty())
        throw std::runtime_error("No playable media source available");

    const QString mediaSourceId = requireString(selectedSource, QStringLiteral("Id"));
    const QString container = cleanContainerName(selectedSource.value(QStringLiteral("Container")).toString());

    // Build URL matching jellyfin-mpv-shim: /Videos/{id}/stream?static=true&MediaSourceId=...&api_key=...
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("static"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("MediaSourceId"), mediaSourceId);
    query.addQueryItem(QStringLiteral("api_key"), m_session.accessToken);

    const QString url = QStringLiteral("%1/Videos/%2/stream?%3")
                            .arg(m_serverUrl, movie.id, query.toString(QUrl::FullyEncoded));
    return {
        movie.id,
        movie.title,
        url,
        mediaSourceId,
        playbackResponse.value(QStringLiteral("PlaySessionId")).toString(),
        container,
    };
}

void JellyfinApiFacade::pumpImagePrefetch()
{
    while (m_prefetchInFlight < m_prefetchMaxConcurrent && !m_prefetchQueue.isEmpty()) {
        const QString url = m_prefetchQueue.takeFirst();
        QNetworkRequest request{QUrl(url)};
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);

        auto *reply = m_networkAccessManager->get(request);
        ++m_prefetchInFlight;
        connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
            if (reply)
                reply->readAll();
            if (reply)
                reply->deleteLater();

            m_prefetchSeen.remove(url);
            m_prefetchInFlight = std::max(0, m_prefetchInFlight - 1);
            pumpImagePrefetch();
        });
    }
}

} // namespace JellyfinNative
