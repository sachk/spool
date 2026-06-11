#include "JellyfinApiFacade.h"

#include "../diagnostics/Diagnostics.h"
#include "PlaybackNegotiation.h"

#include <QCoroNetwork>
#include <QCoroTimer>

#include <QHttpHeaders>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QNetworkReply>
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
                          "CommunityRating,CriticRating,People,PrimaryImageAspectRatio,MediaSources");
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

QString includeItemTypesForCollection(QString collectionType)
{
    if (collectionType == QStringLiteral("movies"))
        return QStringLiteral("Movie");
    if (collectionType == QStringLiteral("tvshows"))
        return QStringLiteral("Series");
    return QStringLiteral("Movie,Series,Episode,MusicVideo,Video");
}

QStringList queryStringList(const QVariantMap &options, const QString &key)
{
    QStringList result;
    const QVariant value = options.value(key);
    if (value.typeId() == QMetaType::QStringList) {
        result = value.toStringList();
    } else if (value.typeId() == QMetaType::QVariantList) {
        const QVariantList list = value.toList();
        result.reserve(list.size());
        for (const QVariant &item : list) {
            const QString text = item.toString();
            if (!text.isEmpty())
                result.push_back(text);
        }
    } else {
        const QString text = value.toString();
        if (!text.isEmpty())
            result = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
    }
    result.removeAll(QString());
    return result;
}

void addJoinedQueryItem(QUrlQuery &query, const QVariantMap &options, const QString &key,
                        const QString &queryKey, QLatin1Char delimiter)
{
    const QStringList values = queryStringList(options, key);
    if (!values.isEmpty())
        query.addQueryItem(queryKey, values.join(delimiter));
}

void addOptionalBoolQueryItem(QUrlQuery &query, const QVariantMap &options,
                              const QString &key, const QString &queryKey)
{
    if (!options.contains(key))
        return;
    const QVariant value = options.value(key);
    if (!value.isValid() || value.isNull())
        return;
    query.addQueryItem(queryKey, value.toBool() ? QStringLiteral("true") : QStringLiteral("false"));
}

void addLibraryQueryOptions(QUrlQuery &query, const QVariantMap &options)
{
    const QString sortBy = options.value(QStringLiteral("sortBy"), QStringLiteral("SortName")).toString();
    const QString sortOrder = options.value(QStringLiteral("sortOrder"), QStringLiteral("Ascending")).toString();
    query.addQueryItem(QStringLiteral("sortBy"), sortBy.isEmpty() ? QStringLiteral("SortName") : sortBy);
    query.addQueryItem(QStringLiteral("sortOrder"), sortOrder.isEmpty() ? QStringLiteral("Ascending") : sortOrder);

    addJoinedQueryItem(query, options, QStringLiteral("filters"), QStringLiteral("filters"), QLatin1Char(','));
    addJoinedQueryItem(query, options, QStringLiteral("genres"), QStringLiteral("genres"), QLatin1Char('|'));
    addJoinedQueryItem(query, options, QStringLiteral("officialRatings"), QStringLiteral("officialRatings"), QLatin1Char('|'));
    addJoinedQueryItem(query, options, QStringLiteral("tags"), QStringLiteral("tags"), QLatin1Char('|'));
    addJoinedQueryItem(query, options, QStringLiteral("years"), QStringLiteral("years"), QLatin1Char(','));
    addJoinedQueryItem(query, options, QStringLiteral("studioIds"), QStringLiteral("studioIds"), QLatin1Char('|'));
    addJoinedQueryItem(query, options, QStringLiteral("seriesStatus"), QStringLiteral("seriesStatus"), QLatin1Char(','));
    addJoinedQueryItem(query, options, QStringLiteral("videoTypes"), QStringLiteral("videoTypes"), QLatin1Char(','));

    addOptionalBoolQueryItem(query, options, QStringLiteral("isHd"), QStringLiteral("isHd"));
    addOptionalBoolQueryItem(query, options, QStringLiteral("is4K"), QStringLiteral("is4K"));
    addOptionalBoolQueryItem(query, options, QStringLiteral("is3D"), QStringLiteral("is3D"));
    addOptionalBoolQueryItem(query, options, QStringLiteral("hasSubtitles"), QStringLiteral("hasSubtitles"));
    addOptionalBoolQueryItem(query, options, QStringLiteral("hasTrailer"), QStringLiteral("hasTrailer"));
    addOptionalBoolQueryItem(query, options, QStringLiteral("hasSpecialFeature"), QStringLiteral("hasSpecialFeature"));
    addOptionalBoolQueryItem(query, options, QStringLiteral("hasThemeSong"), QStringLiteral("hasThemeSong"));
    addOptionalBoolQueryItem(query, options, QStringLiteral("hasThemeVideo"), QStringLiteral("hasThemeVideo"));
    addOptionalBoolQueryItem(query, options, QStringLiteral("isMissing"), QStringLiteral("isMissing"));
    addOptionalBoolQueryItem(query, options, QStringLiteral("isUnaired"), QStringLiteral("isUnaired"));

    const QVariant specialEpisode = options.value(QStringLiteral("specialEpisode"));
    if (specialEpisode.isValid() && !specialEpisode.isNull() && specialEpisode.toBool())
        query.addQueryItem(QStringLiteral("parentIndexNumber"), QStringLiteral("0"));

    const QString alphabet = options.value(QStringLiteral("alphabet")).toString();
    if (!alphabet.isEmpty()) {
        if (alphabet == QStringLiteral("#"))
            query.addQueryItem(QStringLiteral("nameLessThan"), QStringLiteral("A"));
        else
            query.addQueryItem(QStringLiteral("nameStartsWith"), alphabet);
    }
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

std::vector<MediaStreamInfo> mediaStreamsFromApiJson(const QJsonArray &array)
{
    std::vector<MediaStreamInfo> streams;
    streams.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        MediaStreamInfo stream;
        stream.index = object.value(QStringLiteral("Index")).toInt(-1);
        stream.type = object.value(QStringLiteral("Type")).toString();
        stream.codec = object.value(QStringLiteral("Codec")).toString();
        stream.profile = object.value(QStringLiteral("Profile")).toString();
        stream.displayTitle = object.value(QStringLiteral("DisplayTitle")).toString();
        stream.title = object.value(QStringLiteral("Title")).toString();
        stream.language = object.value(QStringLiteral("Language")).toString();
        stream.pixelFormat = object.value(QStringLiteral("PixelFormat")).toString();
        stream.videoRange = object.value(QStringLiteral("VideoRange")).toString();
        stream.colorPrimaries = object.value(QStringLiteral("ColorPrimaries")).toString();
        stream.colorTransfer = object.value(QStringLiteral("ColorTransfer")).toString();
        stream.colorSpace = object.value(QStringLiteral("ColorSpace")).toString();
        stream.aspectRatio = object.value(QStringLiteral("AspectRatio")).toString();
        stream.width = object.value(QStringLiteral("Width")).toInt();
        stream.height = object.value(QStringLiteral("Height")).toInt();
        stream.frameRate = object.value(QStringLiteral("AverageFrameRate")).toDouble();
        if (stream.frameRate <= 0.0)
            stream.frameRate = object.value(QStringLiteral("RealFrameRate")).toDouble();
        stream.bitRate = object.value(QStringLiteral("BitRate")).toInt();
        stream.bitDepth = object.value(QStringLiteral("BitDepth")).toInt();
        stream.channels = object.value(QStringLiteral("Channels")).toInt();
        stream.sampleRate = object.value(QStringLiteral("SampleRate")).toInt();
        stream.isDefault = object.value(QStringLiteral("IsDefault")).toBool(false);
        stream.isForced = object.value(QStringLiteral("IsForced")).toBool(false);
        stream.isExternal = object.value(QStringLiteral("IsExternal")).toBool(false);
        stream.isInterlaced = object.value(QStringLiteral("IsInterlaced")).toBool(false);
        if (!stream.type.isEmpty() || !stream.codec.isEmpty())
            streams.push_back(stream);
    }
    return streams;
}

std::vector<MediaSourceInfo> mediaSourcesFromApiJson(const QJsonArray &array)
{
    std::vector<MediaSourceInfo> sources;
    sources.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        MediaSourceInfo source;
        source.id = object.value(QStringLiteral("Id")).toString();
        source.name = object.value(QStringLiteral("Name")).toString();
        source.path = object.value(QStringLiteral("Path")).toString();
        source.container = cleanContainerName(object.value(QStringLiteral("Container")).toString());
        source.protocol = object.value(QStringLiteral("Protocol")).toString();
        source.videoType = object.value(QStringLiteral("VideoType")).toString();
        source.size = object.value(QStringLiteral("Size")).toVariant().toLongLong();
        source.bitRate = object.value(QStringLiteral("Bitrate")).toInt();
        if (source.bitRate <= 0)
            source.bitRate = object.value(QStringLiteral("BitRate")).toInt();
        source.runtimeTicks = object.value(QStringLiteral("RunTimeTicks")).toVariant().toLongLong();
        source.streams = mediaStreamsFromApiJson(object.value(QStringLiteral("MediaStreams")).toArray());
        if (!source.id.isEmpty() || !source.container.isEmpty() || !source.streams.empty())
            sources.push_back(source);
    }
    return sources;
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

    MovieItem item;
    item.id = itemId;
    item.title = object.value(QStringLiteral("Name")).toString();
    item.overview = object.value(QStringLiteral("Overview")).toString();
    item.posterUrl = posterTag.isEmpty() ? QString() : api->buildImageUrl(itemId, posterTag);
    item.posterTag = posterTag;
    item.itemType = itemType;
    item.seriesId = seriesId;
    item.seriesName = object.value(QStringLiteral("SeriesName")).toString();
    item.seriesPosterUrl = !seriesId.isEmpty() && !seriesPrimaryImageTag.isEmpty()
        ? api->buildImageUrl(seriesId, seriesPrimaryImageTag, 360, 80)
        : QString();
    item.subtitle = subtitle;
    item.path = object.value(QStringLiteral("Path")).toString();
    item.year = object.value(QStringLiteral("ProductionYear")).toInt();
    item.seasonNumber = itemType == QStringLiteral("Episode") ? parentIndexNumber : indexNumber;
    item.episodeNumber = itemType == QStringLiteral("Episode") ? indexNumber : 0;
    item.resumeTicks = resumeTicks;
    item.runtimeTicks = runtimeTicks;
    item.playable = playable;
    item.favorite = userData.value(QStringLiteral("IsFavorite")).toBool(false);
    item.played = userData.value(QStringLiteral("Played")).toBool(false);
    item.backdropUrl = backdropTag.isEmpty()
        ? QString()
        : api->buildImageUrl(itemId, backdropTag, 1920, 82, QStringLiteral("webp"), QStringLiteral("Backdrop"));
    item.logoUrl = logoTag.isEmpty()
        ? QString()
        : api->buildImageUrl(itemId, logoTag, 720, 90, QStringLiteral("png"), QStringLiteral("Logo"));
    item.bannerUrl = bannerTag.isEmpty()
        ? QString()
        : api->buildImageUrl(itemId, bannerTag, 1000, 86, QStringLiteral("webp"), QStringLiteral("Banner"));
    item.thumbUrl = thumbTag.isEmpty()
        ? QString()
        : api->buildImageUrl(itemId, thumbTag, 720, 82, QStringLiteral("webp"), QStringLiteral("Thumb"));
    item.genres = stringsFromJsonArray(object.value(QStringLiteral("Genres")).toArray());
    item.tags = stringsFromJsonArray(object.value(QStringLiteral("Tags")).toArray());
    item.studios = studioNamesFromJsonArray(object.value(QStringLiteral("Studios")).toArray());
    item.officialRating = object.value(QStringLiteral("OfficialRating")).toString();
    item.communityRating = object.value(QStringLiteral("CommunityRating")).toDouble();
    item.criticRating = object.value(QStringLiteral("CriticRating")).toDouble();
    item.premiereDate = object.value(QStringLiteral("PremiereDate")).toString();
    item.endDate = object.value(QStringLiteral("EndDate")).toString();
    item.people = peopleFromApiJson(api, object.value(QStringLiteral("People")).toArray());
    item.mediaSources = mediaSourcesFromApiJson(object.value(QStringLiteral("MediaSources")).toArray());
    return item;
}

}

JellyfinApiFacade::JellyfinApiFacade(QNetworkAccessManager *networkAccessManager, QObject *parent)
    : QObject(parent)
    , m_networkAccessManager(networkAccessManager)
    , m_rest(networkAccessManager, this)
{
    m_requestFactory.setTransferTimeout(
        std::chrono::milliseconds(HttpRequestPolicy::transferTimeoutMs()));

    QHttpHeaders headers;
    headers.append(QHttpHeaders::WellKnownHeader::Accept, QStringLiteral("application/json"));
    m_requestFactory.setCommonHeaders(headers);
}

JellyfinApiFacade::~JellyfinApiFacade()
{
    cancelRequests();
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
    m_authExpirationReported = false;
}

void JellyfinApiFacade::setPlaybackPreferences(qint64 maxStreamingBitrate,
                                               bool preferRemux)
{
    m_maxStreamingBitrate =
        std::clamp<qint64>(maxStreamingBitrate, 1'000'000, 1'000'000'000);
    m_preferRemux = preferRemux;
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

void JellyfinApiFacade::cancelRequests()
{
    if (m_shuttingDown)
        return;
    m_shuttingDown = true;
    cancelPrefetches();
    const auto replies = m_activeReplies;
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
                                                                 int startIndex, int limit,
                                                                 QVariantMap queryOptions)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), m_session.userId);
    query.addQueryItem(QStringLiteral("parentId"), libraryId);
    query.addQueryItem(QStringLiteral("recursive"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("includeItemTypes"), includeItemTypesForCollection(collectionType));
    if (collectionType != QStringLiteral("movies") && collectionType != QStringLiteral("tvshows"))
        query.addQueryItem(QStringLiteral("mediaTypes"), QStringLiteral("Video"));
    query.addQueryItem(QStringLiteral("fields"), detailItemFields());
    addLibraryQueryOptions(query, queryOptions);
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

QCoro::Task<QVariantMap> JellyfinApiFacade::fetchLibraryFilterOptions(QString libraryId, QString collectionType)
{
    QVariantMap options;
    if (libraryId.isEmpty())
        co_return options;

    const QString includeItemTypes = includeItemTypesForCollection(collectionType);

    QUrlQuery filterQuery;
    filterQuery.addQueryItem(QStringLiteral("userId"), m_session.userId);
    filterQuery.addQueryItem(QStringLiteral("parentId"), libraryId);
    filterQuery.addQueryItem(QStringLiteral("includeItemTypes"), includeItemTypes);

    const QJsonObject filters =
        (co_await requestJson(HttpMethod::Get, QStringLiteral("/Items/Filters"), filterQuery)).object();
    options.insert(QStringLiteral("genres"),
                   stringsFromJsonArray(filters.value(QStringLiteral("Genres")).toArray()));
    options.insert(QStringLiteral("officialRatings"),
                   stringsFromJsonArray(filters.value(QStringLiteral("OfficialRatings")).toArray()));
    options.insert(QStringLiteral("tags"),
                   stringsFromJsonArray(filters.value(QStringLiteral("Tags")).toArray()));

    QVariantList years;
    const QJsonArray yearsArray = filters.value(QStringLiteral("Years")).toArray();
    years.reserve(yearsArray.size());
    for (const QJsonValue &value : yearsArray) {
        const int year = value.toInt();
        if (year > 0)
            years.push_back(year);
    }
    options.insert(QStringLiteral("years"), years);

    QUrlQuery studiosQuery;
    studiosQuery.addQueryItem(QStringLiteral("userId"), m_session.userId);
    studiosQuery.addQueryItem(QStringLiteral("parentId"), libraryId);
    studiosQuery.addQueryItem(QStringLiteral("includeItemTypes"), includeItemTypes);
    studiosQuery.addQueryItem(QStringLiteral("sortBy"), QStringLiteral("SortName"));
    studiosQuery.addQueryItem(QStringLiteral("sortOrder"), QStringLiteral("Ascending"));

    const QJsonArray studioItems =
        (co_await requestJson(HttpMethod::Get, QStringLiteral("/Studios"), studiosQuery))
            .object()
            .value(QStringLiteral("Items"))
            .toArray();
    QVariantList studios;
    studios.reserve(studioItems.size());
    for (const QJsonValue &value : studioItems) {
        const QJsonObject studio = value.toObject();
        const QString id = studio.value(QStringLiteral("Id")).toString();
        const QString name = studio.value(QStringLiteral("Name")).toString();
        if (!id.isEmpty() && !name.isEmpty())
            studios.push_back(QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("name"), name}});
    }
    options.insert(QStringLiteral("studios"), studios);

    co_return options;
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
        if (object.value(QStringLiteral("Type")).toString() == QStringLiteral("Season")) {
            auto season = mediaItemFromJson(this, object);
            if (season.seriesId.isEmpty())
                season.seriesId = seriesId;
            seasons.push_back(season);
        }
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

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchSearchSuggestions(int limit)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), m_session.userId);
    query.addQueryItem(QStringLiteral("recursive"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("includeItemTypes"), QStringLiteral("Movie,Series"));
    query.addQueryItem(QStringLiteral("mediaTypes"), QStringLiteral("Video"));
    query.addQueryItem(QStringLiteral("fields"), detailItemFields());
    query.addQueryItem(QStringLiteral("sortBy"), QStringLiteral("IsFavoriteOrLiked,Random"));
    query.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary,Backdrop,Logo,Banner,Thumb"));
    query.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("3"));
    query.addQueryItem(QStringLiteral("enableTotalRecordCount"), QStringLiteral("false"));
    query.addQueryItem(QStringLiteral("limit"), QString::number(std::clamp(limit, 1, 60)));

    const QJsonArray items =
        (co_await requestJson(HttpMethod::Get, QStringLiteral("/Items"), query)).object().value(QStringLiteral("Items")).toArray();

    std::vector<MovieItem> result;
    result.reserve(items.size());
    for (const auto &value : items) {
        const auto object = value.toObject();
        const QString itemType = object.value(QStringLiteral("Type")).toString();
        if (itemType == QStringLiteral("Movie") || itemType == QStringLiteral("Series"))
            result.push_back(mediaItemFromJson(this, object));
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

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchItemsByGenre(QString genre, int limit)
{
    if (genre.trimmed().isEmpty())
        co_return std::vector<MovieItem>{};

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), m_session.userId);
    query.addQueryItem(QStringLiteral("recursive"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("genres"), genre);
    query.addQueryItem(QStringLiteral("includeItemTypes"), QStringLiteral("Movie,Series"));
    query.addQueryItem(QStringLiteral("mediaTypes"), QStringLiteral("Video"));
    query.addQueryItem(QStringLiteral("fields"), detailItemFields());
    query.addQueryItem(QStringLiteral("sortBy"), QStringLiteral("SortName"));
    query.addQueryItem(QStringLiteral("sortOrder"), QStringLiteral("Ascending"));
    query.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary,Backdrop,Logo,Banner,Thumb"));
    query.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("3"));
    query.addQueryItem(QStringLiteral("limit"), QString::number(std::clamp(limit, 1, 400)));

    const QJsonArray items =
        (co_await requestJson(HttpMethod::Get, QStringLiteral("/Items"), query)).object().value(QStringLiteral("Items")).toArray();

    std::vector<MovieItem> result;
    result.reserve(items.size());
    for (const QJsonValue &value : items) {
        const QJsonObject object = value.toObject();
        const QString itemType = object.value(QStringLiteral("Type")).toString();
        if (itemType == QStringLiteral("Movie") || itemType == QStringLiteral("Series"))
            result.push_back(mediaItemFromJson(this, object));
    }
    co_return result;
}

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchItemsByStudio(QString studio, int limit)
{
    if (studio.trimmed().isEmpty())
        co_return std::vector<MovieItem>{};

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), m_session.userId);
    query.addQueryItem(QStringLiteral("recursive"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("studios"), studio);
    query.addQueryItem(QStringLiteral("includeItemTypes"), QStringLiteral("Movie,Series"));
    query.addQueryItem(QStringLiteral("mediaTypes"), QStringLiteral("Video"));
    query.addQueryItem(QStringLiteral("fields"), detailItemFields());
    query.addQueryItem(QStringLiteral("sortBy"), QStringLiteral("SortName"));
    query.addQueryItem(QStringLiteral("sortOrder"), QStringLiteral("Ascending"));
    query.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary,Backdrop,Logo,Banner,Thumb"));
    query.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("3"));
    query.addQueryItem(QStringLiteral("limit"), QString::number(std::clamp(limit, 1, 400)));

    const QJsonArray items =
        (co_await requestJson(HttpMethod::Get, QStringLiteral("/Items"), query)).object().value(QStringLiteral("Items")).toArray();

    std::vector<MovieItem> result;
    result.reserve(items.size());
    for (const QJsonValue &value : items) {
        const QJsonObject object = value.toObject();
        const QString itemType = object.value(QStringLiteral("Type")).toString();
        if (itemType == QStringLiteral("Movie") || itemType == QStringLiteral("Series"))
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

    QUrl url(m_serverUrl);
    QString path = url.path();
    if (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    url.setPath(QStringLiteral("%1/Videos/%2/Trickplay/%3/%4.jpg")
                    .arg(path, itemId, QString::number(width), QString::number(tileIndex)));

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("api_key"), m_session.accessToken);
    url.setQuery(query);
    return url.toString(QUrl::FullyEncoded);
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

QCoro::Task<QJsonObject> JellyfinApiFacade::fetchUtcTime()
{
    co_return (co_await requestJson(HttpMethod::Get,
                                    QStringLiteral("/GetUtcTime")))
        .object();
}

QCoro::Task<void> JellyfinApiFacade::syncPlayReportPing(qint64 pingMs)
{
    const QJsonObject body = {
        {QStringLiteral("Ping"), std::max<qint64>(0, pingMs)},
    };
    co_await requestNoContent(HttpMethod::Post, QStringLiteral("/SyncPlay/Ping"),
                              QJsonDocument(body));
}

QCoro::Task<void> JellyfinApiFacade::syncPlayReportBuffering(
    bool buffering, qint64 positionTicks, bool playing,
    QString playlistItemId, QDateTime serverTime)
{
    if (playlistItemId.isEmpty()) {
        playlistItemId =
            QStringLiteral("00000000-0000-0000-0000-000000000000");
    }
    const QJsonObject body = {
        {QStringLiteral("When"),
         serverTime.toUTC().toString(Qt::ISODateWithMs)},
        {QStringLiteral("PositionTicks"), positionTicks},
        {QStringLiteral("IsPlaying"), playing},
        {QStringLiteral("PlaylistItemId"), playlistItemId},
    };
    const QString path = buffering ? QStringLiteral("/SyncPlay/Buffering")
                                   : QStringLiteral("/SyncPlay/Ready");
    co_await requestNoContent(HttpMethod::Post, path, QJsonDocument(body));
}

QCoro::Task<PlaybackSession> JellyfinApiFacade::negotiatePlayback(MovieItem movie)
{
    Diagnostics::Task task(QStringLiteral("api_negotiate_playback"), {{QStringLiteral("itemId"), movie.id}, {QStringLiteral("title"), movie.title}});
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), m_session.userId);

    const QJsonObject body = {
        {QStringLiteral("UserId"), m_session.userId},
        {QStringLiteral("MaxStreamingBitrate"), m_maxStreamingBitrate},
        {QStringLiteral("StartTimeTicks"), movie.resumeTicks},
        {QStringLiteral("AutoOpenLiveStream"), true},
        {QStringLiteral("EnableDirectPlay"), true},
        {QStringLiteral("EnableDirectStream"), true},
        {QStringLiteral("EnableTranscoding"), true},
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
        {QStringLiteral("PlayMethod"), session.playMethod},
        {QStringLiteral("PlaySessionId"), session.playSessionId},
        {QStringLiteral("PositionTicks"), session.startTimeTicks},
    };

    co_await requestNoContent(HttpMethod::Post, QStringLiteral("/Sessions/Playing"), QJsonDocument(body));
}

QCoro::Task<void> JellyfinApiFacade::reportPlaybackProgress(PlaybackSession session, qint64 positionTicks, bool paused)
{
    const QJsonObject body = {
        {QStringLiteral("CanSeek"), true},
        {QStringLiteral("ItemId"), session.itemId},
        {QStringLiteral("MediaSourceId"), session.mediaSourceId},
        {QStringLiteral("PlayMethod"), session.playMethod},
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
    const QString methodName = method == HttpMethod::Get ? QStringLiteral("GET")
                             : method == HttpMethod::Post ? QStringLiteral("POST")
                                                          : QStringLiteral("DELETE");
    const HttpOperation operation = operationFor(method, path);
    const int maximumAttempts = HttpRequestPolicy::maximumAttempts(operation);

    for (int attempt = 1; attempt <= maximumAttempts; ++attempt) {
        if (m_shuttingDown)
            throw std::runtime_error("Request canceled during shutdown");

        const QNetworkRequest request = createRequest(path, query);
        Diagnostics::NetworkRequest diagnosticsRequest(
            methodName, request.url().toString(QUrl::FullyEncoded));
        QNetworkReply *reply = nullptr;

        if (isQuickConnectPath(path)) {
            qInfo() << "api:" << methodName
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
        case HttpMethod::Delete:
            reply = m_rest.deleteResource(request);
            break;
        }

        m_activeReplies.insert(reply);
        reply = co_await reply;
        m_activeReplies.remove(reply);
        const QByteArray payload = reply ? reply->readAll() : QByteArray{};
        const QString errorText =
            reply ? reply->errorString() : QStringLiteral("Network reply disappeared");
        const int statusCode =
            reply ? reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() : 500;
        const auto networkError =
            reply ? reply->error() : QNetworkReply::UnknownNetworkError;
        if (reply)
            reply->deleteLater();
        diagnosticsRequest.finish(
            statusCode,
            networkError == QNetworkReply::NoError ? QString() : errorText);

        if (networkError == QNetworkReply::NoError && statusCode < 400) {
            if (isQuickConnectPath(path)) {
                qInfo() << "api:" << path << "ok" << statusCode
                        << QString::fromUtf8(payload.left(256));
            }
            co_return payload;
        }

        const QString details =
            payload.isEmpty() ? errorText : QString::fromUtf8(payload);
        if (statusCode == 401 && shouldExpireSession(path) &&
            !m_authExpirationReported) {
            m_authExpirationReported = true;
            emit authenticationExpired(
                QStringLiteral("Your Jellyfin session has expired. Sign in again."));
        }

        if (!HttpRequestPolicy::shouldRetry(operation, attempt, statusCode,
                                            networkError)) {
            if (isQuickConnectPath(path))
                qWarning() << "api:" << path << "failed" << statusCode << details;
            throw std::runtime_error(
                QStringLiteral("%1 (%2)").arg(details).arg(statusCode).toStdString());
        }

        const int delayMs = HttpRequestPolicy::retryDelayMs(attempt);
        qWarning() << "api:" << methodName << path << "attempt" << attempt
                   << "failed with" << statusCode << errorText
                   << "- retrying in" << delayMs << "ms";
        co_await QCoro::sleepFor(std::chrono::milliseconds(delayMs));
    }

    throw std::runtime_error("HTTP retry policy exhausted");
}

HttpOperation JellyfinApiFacade::operationFor(HttpMethod method,
                                               const QString &path) const
{
    if (path.startsWith(QStringLiteral("/Sessions/Playing")))
        return HttpOperation::PlaybackReport;
    return method == HttpMethod::Get ? HttpOperation::Read
                                     : HttpOperation::Mutation;
}

bool JellyfinApiFacade::shouldExpireSession(const QString &path) const
{
    return !m_session.accessToken.isEmpty() && !isQuickConnectPath(path) &&
           !path.startsWith(QStringLiteral("/Users/Authenticate"));
}

QJsonObject JellyfinApiFacade::buildDeviceProfile() const
{
    return PlaybackNegotiation::buildDeviceProfile(m_maxStreamingBitrate);
}

PlaybackSession JellyfinApiFacade::buildPlaybackSession(const MovieItem &movie, const QJsonObject &playbackResponse) const
{
    const QJsonArray mediaSources = playbackResponse.value(QStringLiteral("MediaSources")).toArray();
    if (mediaSources.isEmpty())
        throw std::runtime_error("No media sources returned by Jellyfin");

    const PlaybackSelection selection =
        PlaybackNegotiation::selectSource(mediaSources, m_preferRemux);
    const QJsonObject selectedSource = selection.source;

    const QString mediaSourceId = requireString(selectedSource, QStringLiteral("Id"));
    const QString container = cleanContainerName(selectedSource.value(QStringLiteral("Container")).toString());

    return {
        movie.id,
        movie.title,
        PlaybackNegotiation::buildUrl(m_serverUrl, movie.id,
                                      m_session.accessToken, selection),
        mediaSourceId,
        playbackResponse.value(QStringLiteral("PlaySessionId")).toString(),
        selection.playMethod,
        container,
        movie.resumeTicks,
    };
}

void JellyfinApiFacade::pumpImagePrefetch()
{
    while (m_prefetchInFlight < m_prefetchMaxConcurrent && !m_prefetchQueue.isEmpty()) {
        const QString url = m_prefetchQueue.takeFirst();
        Diagnostics::logEvent(QStringLiteral("network"), QStringLiteral("prefetch_begin"), {{QStringLiteral("url"), sanitizedDiagnosticUrl(url, 160)}});
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
            Diagnostics::logEvent(QStringLiteral("network"), QStringLiteral("prefetch_end"), {{QStringLiteral("url"), sanitizedDiagnosticUrl(url, 160)}, {QStringLiteral("inFlight"), m_prefetchInFlight}});
            pumpImagePrefetch();
        });
    }
}

} // namespace JellyfinNative
