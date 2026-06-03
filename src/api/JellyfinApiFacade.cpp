#include "JellyfinApiFacade.h"

#include "../diagnostics/Diagnostics.h"

#include <QCoroNetwork>

#include <QHttpHeaders>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <cmath>
#include <limits>
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

QString detailItemFields()
{
    return QStringLiteral("Overview,ProductionYear,PremiereDate,EndDate,ImageTags,BackdropImageTags,"
                          "UserData,Path,RunTimeTicks,SeriesInfo,Genres,Tags,Studios,OfficialRating,"
                          "CommunityRating,CriticRating,People,PrimaryImageAspectRatio");
}

QString diagnosticUrl(QString url)
{
    static const QRegularExpression secretQuery(QStringLiteral("([?&](?:api_key|access_token|token)=)[^&]+"),
                                                QRegularExpression::CaseInsensitiveOption);
    url.replace(secretQuery, QStringLiteral("\\1<redacted>"));
    return url.left(160);
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

QString firstString(const QJsonArray &array)
{
    for (const QJsonValue &value : array) {
        const QString stringValue = value.toString();
        if (!stringValue.isEmpty())
            return stringValue;
    }
    return {};
}

QStringList stringsFromJsonArray(const QJsonArray &array)
{
    QStringList result;
    result.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QString stringValue = value.toString();
        if (!stringValue.isEmpty())
            result.push_back(stringValue);
    }
    return result;
}

QStringList studioNamesFromJsonArray(const QJsonArray &array)
{
    QStringList result;
    result.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QString name = value.toObject().value(QStringLiteral("Name")).toString();
        if (!name.isEmpty())
            result.push_back(name);
    }
    return result;
}

std::vector<PersonItem> peopleFromApiJson(const JellyfinApiFacade *api, const QJsonArray &array)
{
    std::vector<PersonItem> people;
    people.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        PersonItem person;
        person.id = object.value(QStringLiteral("Id")).toString();
        person.name = object.value(QStringLiteral("Name")).toString();
        person.type = object.value(QStringLiteral("Type")).toString();
        person.role = object.value(QStringLiteral("Role")).toString();
        person.imageTag = object.value(QStringLiteral("PrimaryImageTag")).toString();
        if (!person.id.isEmpty() && !person.imageTag.isEmpty())
            person.imageUrl = api->buildImageUrl(person.id, person.imageTag, 360, 80);
        if (!person.id.isEmpty() || !person.name.isEmpty())
            people.push_back(person);
    }
    return people;
}

MovieItem mediaItemFromJson(const JellyfinApiFacade *api, const QJsonObject &object)
{
    const QString itemId = object.value(QStringLiteral("Id")).toString();
    const QString itemType = object.value(QStringLiteral("Type")).toString();
    const QString seriesId = object.value(QStringLiteral("SeriesId")).toString();
    const QString seriesPrimaryImageTag = object.value(QStringLiteral("SeriesPrimaryImageTag")).toString();
    const QJsonObject imageTags = object.value(QStringLiteral("ImageTags")).toObject();
    const QString posterTag = imageTags.value(QStringLiteral("Primary")).toString();
    const QString logoTag = imageTags.value(QStringLiteral("Logo")).toString();
    const QString bannerTag = imageTags.value(QStringLiteral("Banner")).toString();
    const QString thumbTag = imageTags.value(QStringLiteral("Thumb")).toString();
    const QString backdropTag = firstString(object.value(QStringLiteral("BackdropImageTags")).toArray());
    QString subtitle;
    bool playable = itemType == QStringLiteral("Movie") ||
                    itemType == QStringLiteral("Episode") ||
                    itemType == QStringLiteral("MusicVideo") ||
                    itemType == QStringLiteral("Video");

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
    const QJsonObject userData = object.value(QStringLiteral("UserData")).toObject();
    const qint64 resumeTicks = userData.value(QStringLiteral("PlaybackPositionTicks")).toVariant().toLongLong();
    const qint64 runtimeTicks = object.value(QStringLiteral("RunTimeTicks")).toVariant().toLongLong();

    return {
        itemId,
        object.value(QStringLiteral("Name")).toString(),
        object.value(QStringLiteral("Overview")).toString(),
        posterTag.isEmpty() ? QString() : api->buildImageUrl(itemId, posterTag),
        posterTag,
        itemType,
        seriesId,
        object.value(QStringLiteral("SeriesName")).toString(),
        !seriesId.isEmpty() && !seriesPrimaryImageTag.isEmpty()
            ? api->buildImageUrl(seriesId, seriesPrimaryImageTag, 360, 80)
            : QString(),
        subtitle,
        object.value(QStringLiteral("Path")).toString(),
        object.value(QStringLiteral("ProductionYear")).toInt(),
        itemType == QStringLiteral("Episode") ? parentIndexNumber : indexNumber,
        itemType == QStringLiteral("Episode") ? indexNumber : 0,
        resumeTicks,
        runtimeTicks,
        playable,
        userData.value(QStringLiteral("IsFavorite")).toBool(false),
        userData.value(QStringLiteral("Played")).toBool(false),
        backdropTag.isEmpty() ? QString() : api->buildImageUrl(itemId, backdropTag, 1920, 82, QStringLiteral("webp"), QStringLiteral("Backdrop")),
        logoTag.isEmpty() ? QString() : api->buildImageUrl(itemId, logoTag, 720, 90, QStringLiteral("png"), QStringLiteral("Logo")),
        bannerTag.isEmpty() ? QString() : api->buildImageUrl(itemId, bannerTag, 1000, 86, QStringLiteral("webp"), QStringLiteral("Banner")),
        thumbTag.isEmpty() ? QString() : api->buildImageUrl(itemId, thumbTag, 720, 82, QStringLiteral("webp"), QStringLiteral("Thumb")),
        stringsFromJsonArray(object.value(QStringLiteral("Genres")).toArray()),
        stringsFromJsonArray(object.value(QStringLiteral("Tags")).toArray()),
        studioNamesFromJsonArray(object.value(QStringLiteral("Studios")).toArray()),
        object.value(QStringLiteral("OfficialRating")).toString(),
        object.value(QStringLiteral("CommunityRating")).toDouble(),
        object.value(QStringLiteral("CriticRating")).toDouble(),
        object.value(QStringLiteral("PremiereDate")).toString(),
        object.value(QStringLiteral("EndDate")).toString(),
        peopleFromApiJson(api, object.value(QStringLiteral("People")).toArray()),
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

void JellyfinApiFacade::setAcceptLanguage(const QString &bcp47Tag)
{
    if (bcp47Tag.isEmpty())
        return;
    QHttpHeaders headers;
    headers.append(QHttpHeaders::WellKnownHeader::Accept, QStringLiteral("application/json"));
    headers.append(QHttpHeaders::WellKnownHeader::AcceptLanguage, bcp47Tag);
    m_requestFactory.setCommonHeaders(headers);
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
                                         int quality, const QString &format, const QString &imageType) const
{
    QUrl url(QStringLiteral("%1/Items/%2/Images/%3").arg(m_serverUrl, itemId, imageType));
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

void JellyfinApiFacade::cancelPrefetches()
{
    Diagnostics::logEvent(QStringLiteral("network"), QStringLiteral("prefetch_cancel"),
                          {{QStringLiteral("queued"), m_prefetchQueue.size()},
                           {QStringLiteral("inFlight"), m_prefetchReplies.size()}});
    m_prefetchQueue.clear();
    m_prefetchSeen.clear();

    const auto replies = m_prefetchReplies;
    for (QNetworkReply *reply : replies) {
        if (reply)
            reply->abort();
    }
}

QCoro::Task<void> JellyfinApiFacade::probeServer()
{
    co_await requestJson(HttpMethod::Get, QStringLiteral("/System/Info/Public"));
}

QCoro::Task<AuthSession> JellyfinApiFacade::authenticateByName(QString username, QString password)
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

QCoro::Task<QJsonObject> JellyfinApiFacade::pollQuickConnect(QString secret)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("secret"), secret);
    const QJsonDocument response = co_await requestJson(HttpMethod::Get, QStringLiteral("/QuickConnect/Connect"), query);
    co_return response.object();
}

QCoro::Task<AuthSession> JellyfinApiFacade::authenticateWithQuickConnect(QString secret)
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

QCoro::Task<QJsonObject> JellyfinApiFacade::fetchUserConfiguration()
{
    if (m_session.userId.isEmpty())
        co_return QJsonObject();

    const QJsonDocument response =
        co_await requestJson(HttpMethod::Get, QStringLiteral("/Users/%1").arg(m_session.userId));
    co_return response.object().value(QStringLiteral("Configuration")).toObject();
}

QCoro::Task<void> JellyfinApiFacade::updateUserConfiguration(QJsonObject configuration)
{
    if (m_session.userId.isEmpty())
        co_return;

    co_await requestNoContent(HttpMethod::Post,
                              QStringLiteral("/Users/%1/Configuration").arg(m_session.userId),
                              QJsonDocument(configuration));
}

QCoro::Task<QJsonArray> JellyfinApiFacade::fetchCultures()
{
    const QJsonDocument response =
        co_await requestJson(HttpMethod::Get, QStringLiteral("/Localization/Options"));
    co_return response.isArray() ? response.array()
                                 : response.object().value(QStringLiteral("Items")).toArray();
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

QCoro::Task<PagedMovieItems> JellyfinApiFacade::fetchLibraryPage(QString libraryId, QString collectionType,
                                                                 int startIndex, int limit)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), m_session.userId);
    query.addQueryItem(QStringLiteral("parentId"), libraryId);
    query.addQueryItem(QStringLiteral("recursive"), QStringLiteral("true"));
    if (collectionType == QStringLiteral("movies")) {
        query.addQueryItem(QStringLiteral("includeItemTypes"), QStringLiteral("Movie"));
    } else if (collectionType == QStringLiteral("tvshows")) {
        query.addQueryItem(QStringLiteral("includeItemTypes"), QStringLiteral("Series"));
    } else {
        query.addQueryItem(QStringLiteral("includeItemTypes"), QStringLiteral("Movie,Series,Episode,MusicVideo,Video"));
        query.addQueryItem(QStringLiteral("mediaTypes"), QStringLiteral("Video"));
    }
    query.addQueryItem(QStringLiteral("fields"), detailItemFields());
    query.addQueryItem(QStringLiteral("sortBy"), QStringLiteral("SortName"));
    query.addQueryItem(QStringLiteral("sortOrder"), QStringLiteral("Ascending"));
    query.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary,Backdrop,Logo,Banner,Thumb"));
    query.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("3"));
    query.addQueryItem(QStringLiteral("startIndex"), QString::number(std::max(0, startIndex)));
    query.addQueryItem(QStringLiteral("limit"), QString::number(std::clamp(limit, 1, 100)));

    const QJsonObject response =
        (co_await requestJson(HttpMethod::Get, QStringLiteral("/Items"), query)).object();
    const QJsonArray items = response.value(QStringLiteral("Items")).toArray();

    std::vector<MovieItem> movies;
    movies.reserve(items.size());
    for (const auto &value : items) {
        const auto object = value.toObject();
        const QString itemType = object.value(QStringLiteral("Type")).toString();
        if (itemType != QStringLiteral("Movie") &&
            itemType != QStringLiteral("Series") &&
            itemType != QStringLiteral("Episode") &&
            itemType != QStringLiteral("MusicVideo") &&
            itemType != QStringLiteral("Video"))
            continue;

        movies.push_back(mediaItemFromJson(this, object));
    }

    co_return PagedMovieItems{
        movies,
        response.value(QStringLiteral("TotalRecordCount")).toInt(0),
        std::max(0, startIndex),
        std::clamp(limit, 1, 100),
    };
}

QCoro::Task<PagedMovieItems> JellyfinApiFacade::fetchMoviesPage(QString libraryId, int startIndex, int limit)
{
    co_return co_await fetchLibraryPage(libraryId, QStringLiteral("movies"), startIndex, limit);
}

QCoro::Task<PagedMovieItems> JellyfinApiFacade::fetchSeriesPage(QString libraryId, int startIndex, int limit)
{
    co_return co_await fetchLibraryPage(libraryId, QStringLiteral("tvshows"), startIndex, limit);
}

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchMovies(QString libraryId)
{
    co_return (co_await fetchMoviesPage(libraryId, 0, 100)).items;
}

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchSeries(QString libraryId)
{
    co_return (co_await fetchSeriesPage(libraryId, 0, 100)).items;
}

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchSeasons(QString seriesId)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), m_session.userId);
    query.addQueryItem(QStringLiteral("fields"), detailItemFields());
    query.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary,Backdrop,Logo,Banner,Thumb"));
    query.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("3"));

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

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchEpisodes(QString seriesId, QString seasonId)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), m_session.userId);
    if (!seasonId.isEmpty())
        query.addQueryItem(QStringLiteral("seasonId"), seasonId);
    query.addQueryItem(QStringLiteral("fields"), detailItemFields());
    query.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary,Backdrop,Logo,Banner,Thumb"));
    query.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("3"));

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
    query.addQueryItem(QStringLiteral("fields"), detailItemFields());
    query.addQueryItem(QStringLiteral("includeItemTypes"), QStringLiteral("Movie,Episode"));
    query.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary,Backdrop,Logo,Banner,Thumb"));
    query.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("3"));
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
    query.addQueryItem(QStringLiteral("fields"), detailItemFields());
    query.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary,Backdrop,Logo,Banner,Thumb"));
    query.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("3"));
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

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchLatestItems(QString parentId, int limit)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    query.addQueryItem(QStringLiteral("fields"), detailItemFields());
    query.addQueryItem(QStringLiteral("includeItemTypes"), QStringLiteral("Movie,Series,Episode"));
    query.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary,Backdrop,Logo,Banner,Thumb"));
    query.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("3"));
    query.addQueryItem(QStringLiteral("groupItems"), QStringLiteral("false"));
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

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::searchItems(QString searchTerm, int limit)
{
    searchTerm = searchTerm.trimmed();
    if (searchTerm.isEmpty())
        co_return std::vector<MovieItem>{};

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), m_session.userId);
    query.addQueryItem(QStringLiteral("recursive"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("searchTerm"), searchTerm);
    query.addQueryItem(QStringLiteral("includeItemTypes"), QStringLiteral("Movie,Series,Episode"));
    query.addQueryItem(QStringLiteral("mediaTypes"), QStringLiteral("Video"));
    query.addQueryItem(QStringLiteral("fields"), detailItemFields());
    query.addQueryItem(QStringLiteral("sortBy"), QStringLiteral("SortName"));
    query.addQueryItem(QStringLiteral("sortOrder"), QStringLiteral("Ascending"));
    query.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary,Backdrop,Logo,Banner,Thumb"));
    query.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("3"));
    query.addQueryItem(QStringLiteral("limit"), QString::number(std::clamp(limit, 1, 200)));

    const QJsonArray items =
        (co_await requestJson(HttpMethod::Get, QStringLiteral("/Items"), query)).object().value(QStringLiteral("Items")).toArray();

    std::vector<MovieItem> result;
    result.reserve(items.size());
    for (const auto &value : items) {
        const auto object = value.toObject();
        const QString itemType = object.value(QStringLiteral("Type")).toString();
        if (itemType == QStringLiteral("Movie") ||
            itemType == QStringLiteral("Series") ||
            itemType == QStringLiteral("Episode")) {
            result.push_back(mediaItemFromJson(this, object));
        }
    }

    co_return result;
}

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchSimilarItems(QString itemId, int limit)
{
    if (itemId.isEmpty())
        co_return std::vector<MovieItem>{};

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), m_session.userId);
    query.addQueryItem(QStringLiteral("limit"), QString::number(std::clamp(limit, 1, 60)));
    query.addQueryItem(QStringLiteral("fields"), detailItemFields());
    query.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary,Backdrop,Logo,Banner,Thumb"));
    query.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("3"));

    const QJsonArray items =
        itemsArrayFromDocument(co_await requestJson(HttpMethod::Get,
                                                    QStringLiteral("/Items/%1/Similar").arg(itemId),
                                                    query));

    std::vector<MovieItem> result;
    result.reserve(items.size());
    for (const QJsonValue &value : items) {
        const QJsonObject object = value.toObject();
        const QString itemType = object.value(QStringLiteral("Type")).toString();
        if (itemType == QStringLiteral("Movie") ||
            itemType == QStringLiteral("Series") ||
            itemType == QStringLiteral("Episode"))
            result.push_back(mediaItemFromJson(this, object));
    }
    co_return result;
}

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchItemsByPerson(QString personId, int limit)
{
    if (personId.isEmpty())
        co_return std::vector<MovieItem>{};

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), m_session.userId);
    query.addQueryItem(QStringLiteral("recursive"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("personIds"), personId);
    query.addQueryItem(QStringLiteral("includeItemTypes"), QStringLiteral("Movie,Series,Episode"));
    query.addQueryItem(QStringLiteral("mediaTypes"), QStringLiteral("Video"));
    query.addQueryItem(QStringLiteral("fields"), detailItemFields());
    query.addQueryItem(QStringLiteral("sortBy"), QStringLiteral("SortName"));
    query.addQueryItem(QStringLiteral("sortOrder"), QStringLiteral("Ascending"));
    query.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary,Backdrop,Logo,Banner,Thumb"));
    query.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("3"));
    query.addQueryItem(QStringLiteral("limit"), QString::number(std::clamp(limit, 1, 200)));

    const QJsonArray items =
        (co_await requestJson(HttpMethod::Get, QStringLiteral("/Items"), query)).object().value(QStringLiteral("Items")).toArray();

    std::vector<MovieItem> result;
    result.reserve(items.size());
    for (const QJsonValue &value : items) {
        const QJsonObject object = value.toObject();
        const QString itemType = object.value(QStringLiteral("Type")).toString();
        if (itemType == QStringLiteral("Movie") ||
            itemType == QStringLiteral("Series") ||
            itemType == QStringLiteral("Episode"))
            result.push_back(mediaItemFromJson(this, object));
    }
    co_return result;
}

QCoro::Task<void> JellyfinApiFacade::setItemFavorite(QString itemId, bool favorite)
{
    if (itemId.isEmpty())
        co_return;

    const QString path = QStringLiteral("/Users/%1/FavoriteItems/%2").arg(m_session.userId, itemId);
    co_await requestNoContent(favorite ? HttpMethod::Post : HttpMethod::Delete, path, QJsonDocument());
}

QCoro::Task<void> JellyfinApiFacade::setItemPlayed(QString itemId, bool played)
{
    if (itemId.isEmpty())
        co_return;

    const QString path = QStringLiteral("/Users/%1/PlayedItems/%2").arg(m_session.userId, itemId);
    co_await requestNoContent(played ? HttpMethod::Post : HttpMethod::Delete, path, QJsonDocument());
}

QCoro::Task<std::vector<MediaSegment>> JellyfinApiFacade::fetchMediaSegments(QString itemId)
{
    Diagnostics::Task task(QStringLiteral("api_fetch_media_segments"), {{QStringLiteral("itemId"), itemId}});
    std::vector<MediaSegment> result;
    try {
        const QJsonDocument doc =
            co_await requestJson(HttpMethod::Get,
                                 QStringLiteral("/MediaSegments/%1").arg(itemId));
        const QJsonArray items = itemsArrayFromDocument(doc);
        result.reserve(items.size());
        for (const auto &value : items) {
            const QJsonObject object = value.toObject();
            MediaSegment segment;
            segment.id = object.value(QStringLiteral("Id")).toString();
            segment.type = object.value(QStringLiteral("Type")).toString();
            // Jellyfin returns ticks as a JSON number; toVariant().toLongLong() handles ints and doubles.
            segment.startTicks = object.value(QStringLiteral("StartTicks")).toVariant().toLongLong();
            segment.endTicks = object.value(QStringLiteral("EndTicks")).toVariant().toLongLong();
            if (segment.endTicks > segment.startTicks)
                result.push_back(segment);
        }
    } catch (const std::exception &e) {
        // Servers without the media-segments endpoint return 404. Treat any
        // failure as "no segments" — playback should keep working.
        qInfo() << "api: media segments unavailable for" << itemId << ":" << e.what();
    }
    co_return result;
}

QCoro::Task<TrickplayInfo> JellyfinApiFacade::fetchTrickplay(QString itemId, QString mediaSourceId, int preferredWidth)
{
    Diagnostics::Task task(QStringLiteral("api_fetch_trickplay"), {{QStringLiteral("itemId"), itemId}});
    TrickplayInfo best;
    try {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("fields"), QStringLiteral("Trickplay"));
        const QJsonDocument doc =
            co_await requestJson(HttpMethod::Get,
                                 QStringLiteral("/Users/%1/Items/%2").arg(m_session.userId, itemId),
                                 query);
        const QJsonObject trickplay = doc.object().value(QStringLiteral("Trickplay")).toObject();
        // Trickplay = { "<mediaSourceId>": { "<width>": TrickplayInfo, ... }, ... }
        QJsonObject widths;
        if (!mediaSourceId.isEmpty() && trickplay.contains(mediaSourceId)) {
            widths = trickplay.value(mediaSourceId).toObject();
        } else if (!trickplay.isEmpty()) {
            widths = trickplay.constBegin().value().toObject();
        }
        int bestDiff = std::numeric_limits<int>::max();
        for (auto it = widths.begin(); it != widths.end(); ++it) {
            const QJsonObject info = it.value().toObject();
            const int width = info.value(QStringLiteral("Width")).toInt();
            const int diff = std::abs(width - preferredWidth);
            if (diff < bestDiff) {
                bestDiff = diff;
                best.width = width;
                best.height = info.value(QStringLiteral("Height")).toInt();
                best.tileWidth = info.value(QStringLiteral("TileWidth")).toInt();
                best.tileHeight = info.value(QStringLiteral("TileHeight")).toInt();
                best.thumbnailCount = info.value(QStringLiteral("ThumbnailCount")).toInt();
                best.intervalMs = info.value(QStringLiteral("Interval")).toInt();
                best.bandwidth = info.value(QStringLiteral("Bandwidth")).toInt();
            }
        }
    } catch (const std::exception &e) {
        qInfo() << "api: trickplay fetch failed for" << itemId << ":" << e.what();
    }
    co_return best;
}

QString JellyfinApiFacade::trickplayTileUrl(const QString &itemId, int width, int tileIndex) const
{
    if (m_serverUrl.isEmpty() || itemId.isEmpty() || width <= 0)
        return {};
    return QStringLiteral("%1/Videos/%2/Trickplay/%3/%4.jpg?api_key=%5")
        .arg(m_serverUrl, itemId, QString::number(width), QString::number(tileIndex), m_session.accessToken);
}

QCoro::Task<QJsonArray> JellyfinApiFacade::fetchSyncPlayGroups()
{
    Diagnostics::Task task(QStringLiteral("api_syncplay_list"));
    try {
        const QJsonDocument doc =
            co_await requestJson(HttpMethod::Get, QStringLiteral("/SyncPlay/List"));
        if (doc.isArray())
            co_return doc.array();
        co_return doc.object().value(QStringLiteral("Items")).toArray();
    } catch (const std::exception &e) {
        qInfo() << "api: syncplay list failed:" << e.what();
        co_return QJsonArray();
    }
}

QCoro::Task<void> JellyfinApiFacade::createSyncPlayGroup(QString name)
{
    const QJsonObject body = {{QStringLiteral("GroupName"), name}};
    co_await requestNoContent(HttpMethod::Post, QStringLiteral("/SyncPlay/New"), QJsonDocument(body));
}

QCoro::Task<void> JellyfinApiFacade::joinSyncPlayGroup(QString groupId)
{
    const QJsonObject body = {{QStringLiteral("GroupId"), groupId}};
    co_await requestNoContent(HttpMethod::Post, QStringLiteral("/SyncPlay/Join"), QJsonDocument(body));
}

QCoro::Task<void> JellyfinApiFacade::leaveSyncPlayGroup()
{
    co_await requestNoContent(HttpMethod::Post, QStringLiteral("/SyncPlay/Leave"), QJsonDocument());
}

QCoro::Task<void> JellyfinApiFacade::syncPlayRequestPlay()
{
    co_await requestNoContent(HttpMethod::Post, QStringLiteral("/SyncPlay/Unpause"), QJsonDocument());
}

QCoro::Task<void> JellyfinApiFacade::syncPlayRequestPause()
{
    co_await requestNoContent(HttpMethod::Post, QStringLiteral("/SyncPlay/Pause"), QJsonDocument());
}

QCoro::Task<void> JellyfinApiFacade::syncPlayRequestSeek(qint64 positionTicks)
{
    const QJsonObject body = {{QStringLiteral("PositionTicks"), QJsonValue(static_cast<qint64>(positionTicks))}};
    co_await requestNoContent(HttpMethod::Post, QStringLiteral("/SyncPlay/Seek"), QJsonDocument(body));
}

QCoro::Task<PlaybackSession> JellyfinApiFacade::negotiateDirectPlay(MovieItem movie)
{
    Diagnostics::Task task(QStringLiteral("api_negotiate_direct_play"), {{QStringLiteral("itemId"), movie.id}, {QStringLiteral("title"), movie.title}});
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

QCoro::Task<void> JellyfinApiFacade::reportPlaybackStart(PlaybackSession session)
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

QCoro::Task<void> JellyfinApiFacade::reportPlaybackProgress(PlaybackSession session, qint64 positionTicks, bool paused)
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

QCoro::Task<void> JellyfinApiFacade::reportPlaybackStopped(PlaybackSession session, qint64 positionTicks, bool failed)
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

QCoro::Task<QJsonDocument> JellyfinApiFacade::requestJson(HttpMethod method, QString path, QUrlQuery query,
                                                          QJsonDocument body)
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

QCoro::Task<void> JellyfinApiFacade::requestNoContent(HttpMethod method, QString path, QJsonDocument body)
{
    co_await requestBytes(method, path, {}, body);
}

QCoro::Task<QByteArray> JellyfinApiFacade::requestBytes(HttpMethod method, QString path, QUrlQuery query,
                                                         QJsonDocument body)
{
    const QNetworkRequest request = createRequest(path, query);
    const QString methodName = method == HttpMethod::Get ? QStringLiteral("GET")
                             : method == HttpMethod::Post ? QStringLiteral("POST")
                                                          : QStringLiteral("DELETE");
    Diagnostics::NetworkRequest diagnosticsRequest(methodName, request.url().toString(QUrl::FullyEncoded));
    QNetworkReply *reply = nullptr;

    if (isQuickConnectPath(path)) {
        qInfo() << "api:" << methodName << request.url().toString(QUrl::FullyEncoded)
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
    case HttpMethod::Delete:
        reply = m_rest.deleteResource(request);
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
    diagnosticsRequest.finish(statusCode, networkError == QNetworkReply::NoError ? QString() : errorText);

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
        movie.resumeTicks,
    };
}

void JellyfinApiFacade::pumpImagePrefetch()
{
    while (m_prefetchInFlight < m_prefetchMaxConcurrent && !m_prefetchQueue.isEmpty()) {
        const QString url = m_prefetchQueue.takeFirst();
        Diagnostics::logEvent(QStringLiteral("network"), QStringLiteral("prefetch_begin"), {{QStringLiteral("url"), diagnosticUrl(url)}});
        QNetworkRequest request{QUrl(url)};
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);

        auto *reply = m_networkAccessManager->get(request);
        m_prefetchReplies.insert(reply);
        ++m_prefetchInFlight;
        connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
            if (reply)
                reply->readAll();
            if (reply)
                reply->deleteLater();

            m_prefetchSeen.remove(url);
            m_prefetchReplies.remove(reply);
            m_prefetchInFlight = std::max(0, m_prefetchInFlight - 1);
            Diagnostics::logEvent(QStringLiteral("network"), QStringLiteral("prefetch_end"), {{QStringLiteral("url"), diagnosticUrl(url)}, {QStringLiteral("inFlight"), m_prefetchInFlight}});
            pumpImagePrefetch();
        });
    }
}

} // namespace JellyfinNative
