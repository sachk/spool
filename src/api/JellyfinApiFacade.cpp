#include "JellyfinApiFacade.h"

#include "../common/MetaJson.h"
#include "../common/VariantUtils.h"
#include "../diagnostics/Diagnostics.h"
#include "ItemsQuery.h"
#include "PlaybackNegotiation.h"

#include <QCoroNetwork>
#include <QCoroTimer>

#include <QDebug>
#include <QDir>
#include <QHttpHeaders>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#if QT_CONFIG(ssl)
#include <QSslConfiguration>
#endif
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace JellyfinNative {

namespace {

    QString requireString(const QJsonObject& object, const QString& key)
    {
        const QString value = object.value(key).toString();
        if (value.isEmpty())
            throw std::runtime_error(QStringLiteral("Missing field: %1").arg(key).toStdString());
        return value;
    }

    QString cleanContainerName(const QString& container)
    {
        if (container.contains(QLatin1Char(',')))
            return container.section(QLatin1Char(','), 0, 0).trimmed();
        return container.trimmed();
    }

    bool isQuickConnectPath(const QString& path)
    {
        return path.startsWith(QStringLiteral("/QuickConnect/"))
            || path == QStringLiteral("/Users/AuthenticateWithQuickConnect");
    }

    QJsonArray itemsArrayFromDocument(const QJsonDocument& document)
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

    QString libraryItemFields()
    {
        return QStringLiteral("Overview,ProductionYear,PremiereDate,EndDate,ImageTags,BackdropImageTags,"
                              "UserData,RunTimeTicks,SeriesInfo,Genres,Tags,Studios,OfficialRating,"
                              "CommunityRating,CriticRating,PrimaryImageAspectRatio");
    }

    QString episodeSubtitle(const QJsonObject& object)
    {
        const int season = object.value(QStringLiteral("ParentIndexNumber")).toInt();
        const int episode = object.value(QStringLiteral("IndexNumber")).toInt();
        if (season > 0 && episode > 0)
            return QStringLiteral("S%1:E%2").arg(season, 2, 10, QLatin1Char('0')).arg(episode, 2, 10, QLatin1Char('0'));
        if (episode > 0)
            return QStringLiteral("Episode %1").arg(episode);
        return QStringLiteral("Episode");
    }

    QString firstString(const QJsonArray& array)
    {
        for (const QJsonValue& value : array) {
            const QString stringValue = value.toString();
            if (!stringValue.isEmpty())
                return stringValue;
        }
        return {};
    }

    QString serverPath(QString basePath, const QStringList& segments)
    {
        while (basePath.endsWith(QLatin1Char('/')))
            basePath.chop(1);

        for (const QString& segment : segments) {
            if (segment.isEmpty())
                continue;
            basePath += QLatin1Char('/');
            basePath += QString::fromLatin1(QUrl::toPercentEncoding(segment));
        }
        return basePath.isEmpty() ? QStringLiteral("/") : basePath;
    }

    QUrl serverUrlWithPath(const QString& serverUrl, const QStringList& segments)
    {
        QUrl url(serverUrl);
        url.setPath(serverPath(url.path(), segments), QUrl::StrictMode);
        return url;
    }

    QStringList stringsFromJsonArray(const QJsonArray& array)
    {
        QStringList result;
        result.reserve(array.size());
        for (const QJsonValue& value : array) {
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
        if (collectionType == QStringLiteral("playlists"))
            return QStringLiteral("Playlist");
        if (collectionType == QStringLiteral("boxsets"))
            return QStringLiteral("BoxSet");
        return QStringLiteral("Movie,Series,Episode,MusicVideo,Video");
    }

    QStringList itemTypesList(const QString& types)
    {
        return types.split(QLatin1Char(','), Qt::SkipEmptyParts);
    }

    bool libraryBrowseNeedsVideoMediaType(const QString& collectionType)
    {
        return collectionType != QStringLiteral("movies") && collectionType != QStringLiteral("tvshows")
            && collectionType != QStringLiteral("playlists") && collectionType != QStringLiteral("boxsets");
    }

    bool libraryBrowseUsesDirectChildren(const QString& collectionType)
    {
        return collectionType == QStringLiteral("playlists") || collectionType == QStringLiteral("boxsets");
    }

    void addJoinedQueryItem(QUrlQuery& query, const QVariantMap& options, const QString& key, const QString& queryKey,
        QLatin1Char delimiter)
    {
        const QStringList values = stringListFromVariantMap(options, key);
        if (!values.isEmpty())
            query.addQueryItem(queryKey, values.join(delimiter));
    }

    void addOptionalBoolQueryItem(
        QUrlQuery& query, const QVariantMap& options, const QString& key, const QString& queryKey)
    {
        if (!options.contains(key))
            return;
        const QVariant value = options.value(key);
        if (!value.isValid() || value.isNull())
            return;
        query.addQueryItem(queryKey, value.toBool() ? QStringLiteral("true") : QStringLiteral("false"));
    }

    void addLibraryQueryOptions(QUrlQuery& query, const QVariantMap& options)
    {
        const QString sortBy = options.value(QStringLiteral("sortBy"), QStringLiteral("SortName")).toString();
        const QString sortOrder = options.value(QStringLiteral("sortOrder"), QStringLiteral("Ascending")).toString();
        query.addQueryItem(QStringLiteral("sortBy"), sortBy.isEmpty() ? QStringLiteral("SortName") : sortBy);
        query.addQueryItem(QStringLiteral("sortOrder"), sortOrder.isEmpty() ? QStringLiteral("Ascending") : sortOrder);

        addJoinedQueryItem(query, options, QStringLiteral("filters"), QStringLiteral("filters"), QLatin1Char(','));
        addJoinedQueryItem(query, options, QStringLiteral("genres"), QStringLiteral("genres"), QLatin1Char('|'));
        addJoinedQueryItem(
            query, options, QStringLiteral("officialRatings"), QStringLiteral("officialRatings"), QLatin1Char('|'));
        addJoinedQueryItem(query, options, QStringLiteral("tags"), QStringLiteral("tags"), QLatin1Char('|'));
        addJoinedQueryItem(query, options, QStringLiteral("years"), QStringLiteral("years"), QLatin1Char(','));
        addJoinedQueryItem(query, options, QStringLiteral("studioIds"), QStringLiteral("studioIds"), QLatin1Char('|'));
        addJoinedQueryItem(
            query, options, QStringLiteral("seriesStatus"), QStringLiteral("seriesStatus"), QLatin1Char(','));
        addJoinedQueryItem(
            query, options, QStringLiteral("videoTypes"), QStringLiteral("videoTypes"), QLatin1Char(','));

        addOptionalBoolQueryItem(query, options, QStringLiteral("isHd"), QStringLiteral("isHd"));
        addOptionalBoolQueryItem(query, options, QStringLiteral("is4K"), QStringLiteral("is4K"));
        addOptionalBoolQueryItem(query, options, QStringLiteral("is3D"), QStringLiteral("is3D"));
        addOptionalBoolQueryItem(query, options, QStringLiteral("hasSubtitles"), QStringLiteral("hasSubtitles"));
        addOptionalBoolQueryItem(query, options, QStringLiteral("hasTrailer"), QStringLiteral("hasTrailer"));
        addOptionalBoolQueryItem(
            query, options, QStringLiteral("hasSpecialFeature"), QStringLiteral("hasSpecialFeature"));
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

    QStringList studioNamesFromJsonArray(const QJsonArray& array)
    {
        QStringList result;
        result.reserve(array.size());
        for (const QJsonValue& value : array) {
            const QString name = value.toObject().value(QStringLiteral("Name")).toString();
            if (!name.isEmpty())
                result.push_back(name);
        }
        return result;
    }

    QList<PersonItem> peopleFromApiJson(const JellyfinApiFacade *api, const QJsonArray& array)
    {
        QList<PersonItem> people;
        people.reserve(array.size());
        for (const QJsonValue& value : array) {
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

    QList<MediaStreamInfo> mediaStreamsFromApiJson(const QJsonArray& array)
    {
        QList<MediaStreamInfo> streams;
        streams.reserve(array.size());
        for (const QJsonValue& value : array) {
            const QJsonObject object = value.toObject();
            MediaStreamInfo stream = metaFromJson<MediaStreamInfo>(object, MetaJsonKeyPolicy::PascalCase);
            stream.frameRate = object.value(QStringLiteral("AverageFrameRate")).toDouble();
            if (stream.frameRate <= 0.0)
                stream.frameRate = object.value(QStringLiteral("RealFrameRate")).toDouble();
            if (!stream.type.isEmpty() || !stream.codec.isEmpty())
                streams.push_back(stream);
        }
        return streams;
    }

    MediaSourceInfo mediaSourceFromApiJson(const QJsonObject& object)
    {
        MediaSourceInfo source = metaFromJson<MediaSourceInfo>(object, MetaJsonKeyPolicy::PascalCase);
        source.container = cleanContainerName(object.value(QStringLiteral("Container")).toString());
        source.bitRate = object.value(QStringLiteral("Bitrate")).toInt();
        if (source.bitRate <= 0)
            source.bitRate = object.value(QStringLiteral("BitRate")).toInt();
        source.runtimeTicks = object.value(QStringLiteral("RunTimeTicks")).toVariant().toLongLong();
        source.streams = mediaStreamsFromApiJson(object.value(QStringLiteral("MediaStreams")).toArray());
        return source;
    }

    bool isTrickplayWidthMap(const QJsonObject& object)
    {
        if (object.isEmpty())
            return false;
        const QJsonObject first = object.constBegin().value().toObject();
        return first.contains(QStringLiteral("Width")) || first.contains(QStringLiteral("TileWidth"))
            || first.contains(QStringLiteral("Interval"));
    }

    TrickplayInfo trickplayFromApiJson(const QJsonObject& trickplay, const QString& mediaSourceId, int preferredWidth)
    {
        QJsonObject widths;
        if (isTrickplayWidthMap(trickplay)) {
            widths = trickplay;
        } else if (!mediaSourceId.isEmpty() && trickplay.contains(mediaSourceId)) {
            widths = trickplay.value(mediaSourceId).toObject();
        } else if (!trickplay.isEmpty()) {
            widths = trickplay.constBegin().value().toObject();
        }

        TrickplayInfo best;
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
        return best;
    }

    MovieItem mediaItemFromJson(const JellyfinApiFacade *api, const QJsonObject& object)
    {
        const QString itemId = object.value(QStringLiteral("Id")).toString();
        const QString itemType = object.value(QStringLiteral("Type")).toString();
        const QString seriesId = object.value(QStringLiteral("SeriesId")).toString();
        const QString seasonId = object.value(QStringLiteral("SeasonId")).toString();
        const QString seriesPrimaryImageTag = object.value(QStringLiteral("SeriesPrimaryImageTag")).toString();
        const QJsonObject imageTags = object.value(QStringLiteral("ImageTags")).toObject();
        const QString posterTag = imageTags.value(QStringLiteral("Primary")).toString();
        const QString logoTag = imageTags.value(QStringLiteral("Logo")).toString();
        const QString bannerTag = imageTags.value(QStringLiteral("Banner")).toString();
        const QString thumbTag = imageTags.value(QStringLiteral("Thumb")).toString();
        const QString backdropTag = firstString(object.value(QStringLiteral("BackdropImageTags")).toArray());
        QString subtitle;
        bool playable = itemType == QStringLiteral("Movie") || itemType == QStringLiteral("Episode")
            || itemType == QStringLiteral("MusicVideo") || itemType == QStringLiteral("Video");

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
        item.playlistItemId = object.value(QStringLiteral("PlaylistItemId")).toString();
        item.seriesId = seriesId;
        item.seasonId = seasonId;
        item.seriesName = object.value(QStringLiteral("SeriesName")).toString();
        item.seriesPosterUrl = !seriesId.isEmpty() && !seriesPrimaryImageTag.isEmpty()
            ? api->buildImageUrl(seriesId, seriesPrimaryImageTag, 360, 80)
            : QString();
        item.subtitle = subtitle;
        item.path = object.value(QStringLiteral("Path")).toString();
        item.year = object.value(QStringLiteral("ProductionYear")).toInt();
        item.seasonNumber = itemType == QStringLiteral("Episode") ? parentIndexNumber : indexNumber;
        item.episodeNumber = itemType == QStringLiteral("Episode") ? indexNumber : 0;
        item.resumeTicks = normalizedResumeTicks(resumeTicks, runtimeTicks);
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
        const int landscapeCardWidth = api->landscapeCardImageWidth();
        const int landscapeCardQuality = api->landscapeCardImageQuality();
        if (!thumbTag.isEmpty()) {
            item.landscapeCardUrl = api->buildImageUrl(itemId, thumbTag, landscapeCardWidth, landscapeCardQuality,
                QStringLiteral("webp"), QStringLiteral("Thumb"), landscapeCardWidth, (landscapeCardWidth * 9) / 16);
        } else if (!backdropTag.isEmpty()) {
            item.landscapeCardUrl = api->buildImageUrl(itemId, backdropTag, landscapeCardWidth, landscapeCardQuality,
                QStringLiteral("webp"), QStringLiteral("Backdrop"), landscapeCardWidth, (landscapeCardWidth * 9) / 16);
        }
        item.genres = stringsFromJsonArray(object.value(QStringLiteral("Genres")).toArray());
        item.tags = stringsFromJsonArray(object.value(QStringLiteral("Tags")).toArray());
        item.studios = studioNamesFromJsonArray(object.value(QStringLiteral("Studios")).toArray());
        item.officialRating = object.value(QStringLiteral("OfficialRating")).toString();
        item.communityRating = object.value(QStringLiteral("CommunityRating")).toDouble();
        item.criticRating = object.value(QStringLiteral("CriticRating")).toDouble();
        item.premiereDate = object.value(QStringLiteral("PremiereDate")).toString();
        item.endDate = object.value(QStringLiteral("EndDate")).toString();
        item.people = peopleFromApiJson(api, object.value(QStringLiteral("People")).toArray());
        const QJsonArray sourceArray = object.value(QStringLiteral("MediaSources")).toArray();
        item.mediaSources.reserve(sourceArray.size());
        for (const QJsonValue& sourceValue : sourceArray) {
            MediaSourceInfo source = mediaSourceFromApiJson(sourceValue.toObject());
            if (!source.id.isEmpty() || !source.container.isEmpty() || !source.streams.isEmpty())
                item.mediaSources.push_back(source);
        }
        return item;
    }

    std::vector<MovieItem> mediaItemsFromJson(
        const JellyfinApiFacade *api, const QJsonArray& items, const QStringList& allowedTypes)
    {
        std::vector<MovieItem> result;
        result.reserve(items.size());
        for (const QJsonValue& value : items) {
            const QJsonObject object = value.toObject();
            if (allowedTypes.contains(object.value(QStringLiteral("Type")).toString()))
                result.push_back(mediaItemFromJson(api, object));
        }
        return result;
    }

}

JellyfinApiFacade::JellyfinApiFacade(QNetworkAccessManager *networkAccessManager, QObject *parent)
    : QObject(parent)
    , m_networkAccessManager(networkAccessManager)
    , m_rest(networkAccessManager, this)
{
    m_requestFactory.setTransferTimeout(std::chrono::milliseconds(HttpRequestPolicy::transferTimeoutMs()));

    applyCommonHeaders();
}

JellyfinApiFacade::~JellyfinApiFacade()
{
    cancelRequests();
}

void JellyfinApiFacade::setServerUrl(const QString& serverUrl)
{
    m_serverUrl = serverUrl;
    while (m_serverUrl.endsWith(QLatin1Char('/')))
        m_serverUrl.chop(1);
    m_requestFactory.setBaseUrl(QUrl(m_serverUrl));
    preconnectToServer();
}

QString JellyfinApiFacade::serverUrl() const
{
    return m_serverUrl;
}

void JellyfinApiFacade::setAcceptLanguage(const QString& bcp47Tag)
{
    if (bcp47Tag.isEmpty())
        return;
    m_acceptLanguage = bcp47Tag;
    applyCommonHeaders();
}

void JellyfinApiFacade::setDeviceIdentity(
    const QString& deviceId, const QString& deviceName, const QString& clientVersion)
{
    m_deviceId = deviceId;
    m_deviceName = deviceName;
    m_clientVersion = clientVersion;
}

QString JellyfinApiFacade::deviceId() const
{
    return m_deviceId;
}

void JellyfinApiFacade::setSession(const AuthSession& session)
{
    m_session = session;
    m_authExpirationReported = false;
    if (m_session.accessToken.isEmpty())
        m_preconnectedAuthority.clear();
    preconnectToServer();
}

void JellyfinApiFacade::preconnectToServer()
{
    if (!m_networkAccessManager || m_serverUrl.isEmpty() || m_session.accessToken.isEmpty())
        return;

    const QUrl url(m_serverUrl);
    if (!url.isValid() || url.host().isEmpty() || url.scheme() != QStringLiteral("https"))
        return;

    const int port = url.port(443);
    const QString authority = url.host() + QLatin1Char(':') + QString::number(port);
    if (m_preconnectedAuthority == authority)
        return;

    m_preconnectedAuthority = authority;
#if QT_CONFIG(ssl)
    m_networkAccessManager->connectToHostEncrypted(
        url.host(), static_cast<quint16>(port), QSslConfiguration::defaultConfiguration());
#else
    // The webOS Qt build has OpenSSL disabled; still warm DNS/TCP so the first
    // authenticated HTTPS request does not pay every socket setup cost.
    m_networkAccessManager->connectToHost(url.host(), static_cast<quint16>(port));
#endif
}

void JellyfinApiFacade::setPlaybackPreferences(qint64 maxStreamingBitrate, bool preferRemux)
{
    m_maxStreamingBitrate = std::clamp<qint64>(maxStreamingBitrate, 1'000'000, 1'000'000'000);
    m_preferRemux = preferRemux;
}

void JellyfinApiFacade::setArtworkUiWidth(int width)
{
    if (width > 0)
        m_artworkUiWidth = width;
}

int JellyfinApiFacade::landscapeCardImageWidth() const
{
    return m_artworkUiWidth >= 3000 ? 640 : 400;
}

int JellyfinApiFacade::landscapeCardImageQuality() const
{
    return m_artworkUiWidth >= 3000 ? 70 : 68;
}

AuthSession JellyfinApiFacade::session() const
{
    return m_session;
}

QString JellyfinApiFacade::buildImageUrl(const QString& itemId, const QString& tag, int maxWidth, int quality,
    const QString& format, const QString& imageType, int fillWidth, int fillHeight) const
{
    if (m_serverUrl.isEmpty() || itemId.isEmpty() || tag.isEmpty() || imageType.isEmpty())
        return {};

    QUrl url = serverUrlWithPath(m_serverUrl, { QStringLiteral("Items"), itemId, QStringLiteral("Images"), imageType });
    QUrlQuery query;
    if (fillWidth > 0 && fillHeight > 0) {
        query.addQueryItem(QStringLiteral("fillWidth"), QString::number(fillWidth));
        query.addQueryItem(QStringLiteral("fillHeight"), QString::number(fillHeight));
    } else {
        query.addQueryItem(QStringLiteral("maxWidth"), QString::number(maxWidth));
    }
    query.addQueryItem(QStringLiteral("quality"), QString::number(quality));
    query.addQueryItem(QStringLiteral("format"), format);
    query.addQueryItem(QStringLiteral("tag"), tag);
    url.setQuery(query);
    return url.toString(QUrl::FullyEncoded);
}

void JellyfinApiFacade::cancelRequests()
{
    if (m_shuttingDown)
        return;
    m_shuttingDown = true;
    const auto replies = m_activeReplies;
    for (QNetworkReply *reply : replies) {
        if (reply)
            reply->abort();
    }
}

QCoro::Task<AuthSession> JellyfinApiFacade::authenticateByName(QString username, QString password)
{
    const QJsonDocument response
        = co_await requestJson(HttpMethod::Post, QStringLiteral("/Users/AuthenticateByName"), {},
            QJsonDocument(QJsonObject {
                { QStringLiteral("Username"), username },
                { QStringLiteral("Pw"), password },
            }));

    const QJsonObject object = response.object();
    const QJsonObject user = object.value(QStringLiteral("User")).toObject();
    const AuthSession session {
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
    const QJsonDocument response
        = co_await requestJson(HttpMethod::Get, QStringLiteral("/QuickConnect/Connect"), query);
    co_return response.object();
}

QCoro::Task<AuthSession> JellyfinApiFacade::authenticateWithQuickConnect(QString secret)
{
    const QJsonDocument response
        = co_await requestJson(HttpMethod::Post, QStringLiteral("/Users/AuthenticateWithQuickConnect"), {},
            QJsonDocument(QJsonObject {
                { QStringLiteral("Secret"), secret },
            }));

    const QJsonObject object = response.object();
    const QJsonObject user = object.value(QStringLiteral("User")).toObject();
    const AuthSession session {
        requireString(user, QStringLiteral("Id")),
        requireString(user, QStringLiteral("Name")),
        requireString(object, QStringLiteral("AccessToken")),
        object.value(QStringLiteral("ServerId")).toString(),
    };
    setSession(session);
    co_return session;
}

QCoro::Task<QString> JellyfinApiFacade::fetchCurrentUserName()
{
    if (m_session.userId.isEmpty())
        co_return QString();

    const QJsonDocument response
        = co_await requestJson(HttpMethod::Get, QStringLiteral("/Users/%1").arg(m_session.userId));
    co_return response.object().value(QStringLiteral("Name")).toString();
}

QCoro::Task<QJsonObject> JellyfinApiFacade::fetchUserConfiguration()
{
    if (m_session.userId.isEmpty())
        co_return QJsonObject();

    const QJsonDocument response
        = co_await requestJson(HttpMethod::Get, QStringLiteral("/Users/%1").arg(m_session.userId));
    co_return response.object().value(QStringLiteral("Configuration")).toObject();
}

QCoro::Task<void> JellyfinApiFacade::updateUserConfiguration(QJsonObject configuration)
{
    if (m_session.userId.isEmpty())
        co_return;

    co_await requestNoContent(HttpMethod::Post, QStringLiteral("/Users/%1/Configuration").arg(m_session.userId),
        QJsonDocument(configuration));
}

QCoro::Task<QJsonObject> JellyfinApiFacade::fetchCurrentUserPolicy()
{
    if (m_session.userId.isEmpty())
        co_return QJsonObject();

    const QJsonDocument response
        = co_await requestJson(HttpMethod::Get, QStringLiteral("/Users/%1").arg(m_session.userId));
    co_return response.object().value(QStringLiteral("Policy")).toObject();
}

QCoro::Task<QJsonArray> JellyfinApiFacade::fetchCultures()
{
    const QJsonDocument response = co_await requestJson(HttpMethod::Get, QStringLiteral("/Localization/Options"));
    co_return response.isArray() ? response.array() : response.object().value(QStringLiteral("Items")).toArray();
}

QCoro::Task<std::vector<LibraryItem>> JellyfinApiFacade::fetchLibraries()
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), m_session.userId);
    query.addQueryItem(QStringLiteral("includeHidden"), QStringLiteral("false"));
    query.addQueryItem(QStringLiteral("presetViews"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary"));
    query.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("1"));

    const QJsonArray items = (co_await requestJson(HttpMethod::Get, QStringLiteral("/UserViews"), query))
                                 .object()
                                 .value(QStringLiteral("Items"))
                                 .toArray();

    std::vector<LibraryItem> libraries;
    libraries.reserve(items.size());
    for (const auto& value : items) {
        const auto object = value.toObject();
        const QString itemId = object.value(QStringLiteral("Id")).toString();
        const QString imageTag
            = object.value(QStringLiteral("ImageTags")).toObject().value(QStringLiteral("Primary")).toString();
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

QCoro::Task<PagedMovieItems> JellyfinApiFacade::fetchBrowsePage(
    BrowseDescriptor descriptor, int startIndex, int limit, QVariantMap queryOptions)
{
    if (!descriptor.isValid())
        co_return PagedMovieItems { {}, 0, std::max(0, startIndex), std::clamp(limit, 1, 200) };

    QString path = QStringLiteral("/Items");
    QStringList allowedTypes;
    const int maximumLimit = descriptor.kind == BrowseKind::Library ? 100 : 200;
    ItemsQuery builder = ItemsQuery()
                             .userId(m_session.userId)
                             .fields(libraryItemFields())
                             .images()
                             .startIndex(startIndex)
                             .limit(limit, maximumLimit);

    switch (descriptor.kind) {
    case BrowseKind::Library: {
        QString includeItemTypes = includeItemTypesForCollection(descriptor.collectionType);
        const QString requestedTypes
            = stringListFromVariantMap(queryOptions, QStringLiteral("includeItemTypes")).join(QLatin1Char(','));
        const bool collectionFilter
            = descriptor.collectionType == QStringLiteral("movies") && requestedTypes == QStringLiteral("BoxSet");
        if (collectionFilter)
            includeItemTypes = requestedTypes;
        builder.parentId(descriptor.id)
            .recursive(!collectionFilter && !libraryBrowseUsesDirectChildren(descriptor.collectionType))
            .includeItemTypes(includeItemTypes);
        if (!collectionFilter && libraryBrowseNeedsVideoMediaType(descriptor.collectionType))
            builder.mediaTypes(QStringLiteral("Video"));
        allowedTypes = itemTypesList(includeItemTypes);
        break;
    }
    case BrowseKind::FolderChildren:
        builder.parentId(descriptor.id)
            .recursive(false)
            .includeItemTypes(
                QStringLiteral("Folder,Movie,Series,Season,Episode,MusicVideo,Video,Playlist,BoxSet,MusicAlbum"));
        allowedTypes = itemTypesList(
            QStringLiteral("Folder,Movie,Series,Season,Episode,MusicVideo,Video,Playlist,BoxSet,MusicAlbum"));
        break;
    case BrowseKind::Person:
        builder.recursive()
            .add(QStringLiteral("personIds"), descriptor.id)
            .includeItemTypes(QStringLiteral("Movie,Series,Episode"))
            .mediaTypes(QStringLiteral("Video"))
            .sort(QStringLiteral("SortName"));
        allowedTypes = itemTypesList(QStringLiteral("Movie,Series,Episode"));
        break;
    case BrowseKind::Genre:
        builder.recursive()
            .add(QStringLiteral("genres"), descriptor.name)
            .includeItemTypes(QStringLiteral("Movie,Series"))
            .mediaTypes(QStringLiteral("Video"))
            .sort(QStringLiteral("SortName"));
        allowedTypes = itemTypesList(QStringLiteral("Movie,Series"));
        break;
    case BrowseKind::Studio:
        builder.recursive()
            .add(QStringLiteral("studios"), descriptor.name)
            .includeItemTypes(QStringLiteral("Movie,Series"))
            .mediaTypes(QStringLiteral("Video"))
            .sort(QStringLiteral("SortName"));
        allowedTypes = itemTypesList(QStringLiteral("Movie,Series"));
        break;
    case BrowseKind::SeriesSeasons:
        path = QStringLiteral("/Shows/%1/Seasons")
                   .arg(descriptor.seriesId.isEmpty() ? descriptor.id : descriptor.seriesId);
        allowedTypes = itemTypesList(QStringLiteral("Season"));
        break;
    case BrowseKind::SeasonEpisodes:
        path = QStringLiteral("/Shows/%1/Episodes").arg(descriptor.seriesId);
        builder.addIfNotEmpty(QStringLiteral("seasonId"), descriptor.seasonId);
        allowedTypes = itemTypesList(QStringLiteral("Episode"));
        break;
    case BrowseKind::Playlist:
        path = QStringLiteral("/Playlists/%1/Items").arg(descriptor.id);
        allowedTypes = itemTypesList(QStringLiteral("Movie,Episode,MusicVideo,Video,Audio"));
        break;
    case BrowseKind::BoxSet:
        builder.parentId(descriptor.id)
            .recursive(false)
            .includeItemTypes(QStringLiteral("Movie,Series,Episode"))
            .mediaTypes(QStringLiteral("Video"))
            .sort(QStringLiteral("SortName"));
        allowedTypes = itemTypesList(QStringLiteral("Movie,Series,Episode"));
        break;
    case BrowseKind::ArtistAlbums:
        builder.recursive()
            .add(QStringLiteral("artistIds"), descriptor.id)
            .includeItemTypes(QStringLiteral("MusicAlbum"))
            .sort(QStringLiteral("SortName"));
        allowedTypes = itemTypesList(QStringLiteral("MusicAlbum"));
        break;
    case BrowseKind::None:
        break;
    }

    QUrlQuery query = builder.toUrlQuery();
    if (descriptor.kind == BrowseKind::Library)
        addLibraryQueryOptions(query, queryOptions);

    const QJsonObject response = (co_await requestJson(HttpMethod::Get, path, query)).object();
    std::vector<MovieItem> items
        = mediaItemsFromJson(this, response.value(QStringLiteral("Items")).toArray(), allowedTypes);
    if (descriptor.kind == BrowseKind::SeriesSeasons) {
        const QString seriesId = descriptor.seriesId.isEmpty() ? descriptor.id : descriptor.seriesId;
        for (MovieItem& item : items) {
            if (item.seriesId.isEmpty())
                item.seriesId = seriesId;
        }
    }

    co_return PagedMovieItems {
        items,
        response.value(QStringLiteral("TotalRecordCount")).toInt(0),
        std::max(0, startIndex),
        std::clamp(limit, 1, maximumLimit),
    };
}

QCoro::Task<QVariantMap> JellyfinApiFacade::fetchLibraryFilterOptions(QString libraryId, QString collectionType)
{
    QVariantMap options;
    if (libraryId.isEmpty())
        co_return options;

    const QString includeItemTypes = includeItemTypesForCollection(collectionType);

    QUrlQuery filterQuery
        = ItemsQuery().userId(m_session.userId).parentId(libraryId).includeItemTypes(includeItemTypes).toUrlQuery();

    const QJsonObject filters
        = (co_await requestJson(HttpMethod::Get, QStringLiteral("/Items/Filters"), filterQuery)).object();
    options.insert(QStringLiteral("genres"), stringsFromJsonArray(filters.value(QStringLiteral("Genres")).toArray()));
    options.insert(QStringLiteral("officialRatings"),
        stringsFromJsonArray(filters.value(QStringLiteral("OfficialRatings")).toArray()));
    options.insert(QStringLiteral("tags"), stringsFromJsonArray(filters.value(QStringLiteral("Tags")).toArray()));

    QVariantList years;
    const QJsonArray yearsArray = filters.value(QStringLiteral("Years")).toArray();
    years.reserve(yearsArray.size());
    for (const QJsonValue& value : yearsArray) {
        const int year = value.toInt();
        if (year > 0)
            years.push_back(year);
    }
    options.insert(QStringLiteral("years"), years);

    QUrlQuery studiosQuery = ItemsQuery()
                                 .userId(m_session.userId)
                                 .parentId(libraryId)
                                 .includeItemTypes(includeItemTypes)
                                 .sort(QStringLiteral("SortName"))
                                 .toUrlQuery();

    const QJsonArray studioItems = (co_await requestJson(HttpMethod::Get, QStringLiteral("/Studios"), studiosQuery))
                                       .object()
                                       .value(QStringLiteral("Items"))
                                       .toArray();
    QVariantList studios;
    studios.reserve(studioItems.size());
    for (const QJsonValue& value : studioItems) {
        const QJsonObject studio = value.toObject();
        const QString id = studio.value(QStringLiteral("Id")).toString();
        const QString name = studio.value(QStringLiteral("Name")).toString();
        if (!id.isEmpty() && !name.isEmpty())
            studios.push_back(QVariantMap { { QStringLiteral("id"), id }, { QStringLiteral("name"), name } });
    }
    options.insert(QStringLiteral("studios"), studios);

    co_return options;
}

QCoro::Task<MovieItem> JellyfinApiFacade::fetchItemDetails(QString itemId)
{
    if (itemId.isEmpty() || m_session.userId.isEmpty())
        co_return MovieItem {};

    QUrlQuery query = ItemsQuery().fields(detailItemFields()).images().toUrlQuery();

    const QJsonObject object = (co_await requestJson(HttpMethod::Get,
                                    QStringLiteral("/Users/%1/Items/%2").arg(m_session.userId, itemId), query))
                                   .object();
    co_return mediaItemFromJson(this, object);
}

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchSeasons(QString seriesId)
{
    const PagedMovieItems page = co_await fetchBrowsePage(BrowseDescriptor::seriesSeasons(seriesId), 0, 200);
    co_return page.items;
}

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchEpisodes(QString seriesId, QString seasonId)
{
    const PagedMovieItems page = co_await fetchBrowsePage(BrowseDescriptor::seasonEpisodes(seriesId, seasonId), 0, 200);
    co_return page.items;
}

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchResumeItems(int limit)
{
    QUrlQuery query = ItemsQuery()
                          .userId(m_session.userId)
                          .limit(limit, 60)
                          .recursive()
                          .fields(libraryItemFields())
                          .includeItemTypes(QStringLiteral("Movie,Episode"))
                          .images()
                          .mediaTypes(QStringLiteral("Video"))
                          .toUrlQuery();

    const QJsonArray items = itemsArrayFromDocument(
        co_await requestJson(HttpMethod::Get, QStringLiteral("/Users/%1/Items/Resume").arg(m_session.userId), query));

    std::vector<MovieItem> result;
    result.reserve(items.size());
    for (const auto& value : items) {
        auto item = mediaItemFromJson(this, value.toObject());
        if (item.playable && item.resumeTicks > 0)
            result.push_back(item);
    }
    co_return result;
}

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchNextUpEpisodes(int limit)
{
    QUrlQuery query = ItemsQuery()
                          .userId(m_session.userId)
                          .limit(limit, 60)
                          .fields(libraryItemFields())
                          .images()
                          .add(QStringLiteral("enableResumable"), QStringLiteral("false"))
                          .toUrlQuery();

    const QJsonArray items
        = itemsArrayFromDocument(co_await requestJson(HttpMethod::Get, QStringLiteral("/Shows/NextUp"), query));

    std::vector<MovieItem> result;
    result.reserve(items.size());
    for (const auto& value : items) {
        auto item = mediaItemFromJson(this, value.toObject());
        if (item.itemType == QStringLiteral("Episode") && item.playable)
            result.push_back(item);
    }
    co_return result;
}

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchLatestItems(QString parentId, int limit)
{
    QUrlQuery query = ItemsQuery()
                          .limit(limit, 60)
                          .fields(libraryItemFields())
                          .includeItemTypes(QStringLiteral("Movie,Series,Episode"))
                          .images()
                          .add(QStringLiteral("groupItems"), QStringLiteral("false"))
                          .parentId(parentId)
                          .toUrlQuery();

    const QJsonDocument doc
        = co_await requestJson(HttpMethod::Get, QStringLiteral("/Users/%1/Items/Latest").arg(m_session.userId), query);

    const QJsonArray items = itemsArrayFromDocument(doc);

    std::vector<MovieItem> result;
    result.reserve(items.size());
    for (const auto& value : items)
        result.push_back(mediaItemFromJson(this, value.toObject()));
    co_return result;
}

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::searchItems(QString searchTerm, int limit)
{
    searchTerm = searchTerm.trimmed();
    if (searchTerm.isEmpty())
        co_return std::vector<MovieItem> {};

    QUrlQuery query = ItemsQuery()
                          .userId(m_session.userId)
                          .recursive()
                          .add(QStringLiteral("searchTerm"), searchTerm)
                          .includeItemTypes(QStringLiteral("Movie,Series,Episode"))
                          .mediaTypes(QStringLiteral("Video"))
                          .fields(libraryItemFields())
                          .sort(QStringLiteral("SortName"))
                          .images()
                          .limit(limit, 200)
                          .toUrlQuery();

    const QJsonArray items = (co_await requestJson(HttpMethod::Get, QStringLiteral("/Items"), query))
                                 .object()
                                 .value(QStringLiteral("Items"))
                                 .toArray();

    const std::vector<MovieItem> result = mediaItemsFromJson(
        this, items, { QStringLiteral("Movie"), QStringLiteral("Series"), QStringLiteral("Episode") });

    co_return result;
}

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchSearchSuggestions(int limit)
{
    QUrlQuery query = ItemsQuery()
                          .userId(m_session.userId)
                          .recursive()
                          .includeItemTypes(QStringLiteral("Movie,Series"))
                          .mediaTypes(QStringLiteral("Video"))
                          .fields(libraryItemFields())
                          .sort(QStringLiteral("IsFavoriteOrLiked,Random"), {})
                          .images()
                          .enableTotalRecordCount(false)
                          .limit(limit, 60)
                          .toUrlQuery();

    const QJsonArray items = (co_await requestJson(HttpMethod::Get, QStringLiteral("/Items"), query))
                                 .object()
                                 .value(QStringLiteral("Items"))
                                 .toArray();

    const std::vector<MovieItem> result
        = mediaItemsFromJson(this, items, { QStringLiteral("Movie"), QStringLiteral("Series") });
    co_return result;
}

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchSimilarItems(QString itemId, int limit)
{
    if (itemId.isEmpty())
        co_return std::vector<MovieItem> {};

    QUrlQuery query
        = ItemsQuery().userId(m_session.userId).limit(limit, 60).fields(libraryItemFields()).images().toUrlQuery();

    const QJsonArray items = itemsArrayFromDocument(
        co_await requestJson(HttpMethod::Get, QStringLiteral("/Items/%1/Similar").arg(itemId), query));

    const std::vector<MovieItem> result = mediaItemsFromJson(
        this, items, { QStringLiteral("Movie"), QStringLiteral("Series"), QStringLiteral("Episode") });
    co_return result;
}

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchItemsByPerson(QString personId, int limit)
{
    const PagedMovieItems page = co_await fetchBrowsePage(BrowseDescriptor::person(personId), 0, limit);
    co_return page.items;
}

QCoro::Task<std::vector<MovieItem>> JellyfinApiFacade::fetchManagementTargets(QString itemType)
{
    if (m_session.userId.isEmpty())
        co_return std::vector<MovieItem> {};

    itemType = itemType.trimmed();
    if (itemType != QStringLiteral("Playlist") && itemType != QStringLiteral("BoxSet"))
        co_return std::vector<MovieItem> {};

    QUrlQuery query = ItemsQuery()
                          .userId(m_session.userId)
                          .recursive()
                          .includeItemTypes(itemType)
                          .fields(libraryItemFields())
                          .sort(QStringLiteral("SortName"))
                          .images()
                          .limit(200, 200)
                          .toUrlQuery();

    const QJsonArray items = (co_await requestJson(HttpMethod::Get, QStringLiteral("/Items"), query))
                                 .object()
                                 .value(QStringLiteral("Items"))
                                 .toArray();
    co_return mediaItemsFromJson(this, items, { itemType });
}

QCoro::Task<QString> JellyfinApiFacade::createPlaylist(QString name, QStringList itemIds)
{
    name = name.trimmed();
    if (name.isEmpty())
        throw std::runtime_error("Playlist name is required");

    QJsonArray ids;
    for (const QString& itemId : itemIds) {
        if (!itemId.isEmpty())
            ids.push_back(itemId);
    }
    QJsonObject body {
        { QStringLiteral("Name"), name },
        { QStringLiteral("UserId"), m_session.userId },
        { QStringLiteral("Ids"), ids },
        { QStringLiteral("IsPublic"), false },
    };
    const QJsonObject response
        = (co_await requestJson(HttpMethod::Post, QStringLiteral("/Playlists"), {}, QJsonDocument(body))).object();
    co_return response.value(QStringLiteral("Id")).toString();
}

QCoro::Task<void> JellyfinApiFacade::addPlaylistItems(QString playlistId, QStringList itemIds, int position)
{
    if (playlistId.isEmpty() || itemIds.isEmpty())
        co_return;

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("ids"), itemIds.join(QLatin1Char(',')));
    query.addQueryItem(QStringLiteral("userId"), m_session.userId);
    if (position >= 0)
        query.addQueryItem(QStringLiteral("position"), QString::number(position));
    co_await requestJson(HttpMethod::Post, QStringLiteral("/Playlists/%1/Items").arg(playlistId), query);
}

QCoro::Task<void> JellyfinApiFacade::removePlaylistItems(QString playlistId, QStringList entryIds)
{
    if (playlistId.isEmpty() || entryIds.isEmpty())
        co_return;

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("entryIds"), entryIds.join(QLatin1Char(',')));
    co_await requestJson(HttpMethod::Delete, QStringLiteral("/Playlists/%1/Items").arg(playlistId), query);
}

QCoro::Task<void> JellyfinApiFacade::movePlaylistItem(QString playlistId, QString playlistItemId, int newIndex)
{
    if (playlistId.isEmpty() || playlistItemId.isEmpty() || newIndex < 0)
        co_return;

    co_await requestNoContent(HttpMethod::Post,
        QStringLiteral("/Playlists/%1/Items/%2/Move/%3").arg(playlistId, playlistItemId, QString::number(newIndex)),
        QJsonDocument());
}

QCoro::Task<void> JellyfinApiFacade::updatePlaylistName(QString playlistId, QString name)
{
    name = name.trimmed();
    if (playlistId.isEmpty() || name.isEmpty())
        co_return;

    co_await requestNoContent(HttpMethod::Post, QStringLiteral("/Playlists/%1").arg(playlistId),
        QJsonDocument(QJsonObject { { QStringLiteral("Name"), name } }));
}

QCoro::Task<QString> JellyfinApiFacade::createCollection(QString name, QStringList itemIds)
{
    name = name.trimmed();
    if (name.isEmpty())
        throw std::runtime_error("Collection name is required");

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("name"), name);
    if (!itemIds.isEmpty())
        query.addQueryItem(QStringLiteral("ids"), itemIds.join(QLatin1Char(',')));
    query.addQueryItem(QStringLiteral("isLocked"), QStringLiteral("false"));
    const QJsonObject response
        = (co_await requestJson(HttpMethod::Post, QStringLiteral("/Collections"), query)).object();
    co_return response.value(QStringLiteral("Id")).toString();
}

QCoro::Task<void> JellyfinApiFacade::addCollectionItems(QString collectionId, QStringList itemIds)
{
    if (collectionId.isEmpty() || itemIds.isEmpty())
        co_return;

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("ids"), itemIds.join(QLatin1Char(',')));
    co_await requestJson(HttpMethod::Post, QStringLiteral("/Collections/%1/Items").arg(collectionId), query);
}

QCoro::Task<void> JellyfinApiFacade::removeCollectionItems(QString collectionId, QStringList itemIds)
{
    if (collectionId.isEmpty() || itemIds.isEmpty())
        co_return;

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("ids"), itemIds.join(QLatin1Char(',')));
    co_await requestJson(HttpMethod::Delete, QStringLiteral("/Collections/%1/Items").arg(collectionId), query);
}

QCoro::Task<void> JellyfinApiFacade::renameItem(QString itemId, QString name)
{
    name = name.trimmed();
    if (itemId.isEmpty() || name.isEmpty())
        co_return;

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), m_session.userId);
    QJsonObject item = (co_await requestJson(HttpMethod::Get, QStringLiteral("/Items/%1").arg(itemId), query)).object();
    item.insert(QStringLiteral("Name"), name);
    co_await requestNoContent(HttpMethod::Post, QStringLiteral("/Items/%1").arg(itemId), QJsonDocument(item));
}

QCoro::Task<void> JellyfinApiFacade::deleteItem(QString itemId)
{
    if (itemId.isEmpty())
        co_return;

    co_await requestNoContent(HttpMethod::Delete, QStringLiteral("/Items/%1").arg(itemId), QJsonDocument());
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

QCoro::Task<void> JellyfinApiFacade::setItemPlaybackPosition(QString itemId, qint64 positionTicks)
{
    if (itemId.isEmpty())
        co_return;

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), m_session.userId);
    const QJsonObject body {
        { QStringLiteral("ItemId"), itemId },
        { QStringLiteral("PlaybackPositionTicks"), std::max<qint64>(0, positionTicks) },
        { QStringLiteral("Played"), false },
    };
    co_await requestJson(
        HttpMethod::Post, QStringLiteral("/UserItems/%1/UserData").arg(itemId), query, QJsonDocument(body));
}

QCoro::Task<std::vector<MediaSegment>> JellyfinApiFacade::fetchMediaSegments(QString itemId)
{
    Diagnostics::Task task(QStringLiteral("api_fetch_media_segments"), { { QStringLiteral("itemId"), itemId } });
    std::vector<MediaSegment> result;
    try {
        const QJsonDocument doc
            = co_await requestJson(HttpMethod::Get, QStringLiteral("/MediaSegments/%1").arg(itemId));
        const QJsonArray items = itemsArrayFromDocument(doc);
        result.reserve(items.size());
        for (const auto& value : items) {
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
    } catch (const std::exception& e) {
        // Servers without the media-segments endpoint return 404. Treat any
        // failure as "no segments" — playback should keep working.
        qInfo() << "api: media segments unavailable for" << itemId << ":" << e.what();
    }
    co_return result;
}

QString JellyfinApiFacade::trickplayTileUrl(const QString& itemId, int width, int tileIndex) const
{
    if (m_serverUrl.isEmpty() || itemId.isEmpty() || width <= 0 || tileIndex < 0)
        return {};

    QUrl url = serverUrlWithPath(m_serverUrl,
        { QStringLiteral("Videos"), itemId, QStringLiteral("Trickplay"), QString::number(width),
            QStringLiteral("%1.jpg").arg(tileIndex) });

    return url.toString(QUrl::FullyEncoded);
}

QCoro::Task<QJsonArray> JellyfinApiFacade::fetchSyncPlayGroups()
{
    Diagnostics::Task task(QStringLiteral("api_syncplay_list"));
    try {
        const QJsonDocument doc = co_await requestJson(HttpMethod::Get, QStringLiteral("/SyncPlay/List"));
        if (doc.isArray())
            co_return doc.array();
        co_return doc.object().value(QStringLiteral("Items")).toArray();
    } catch (const std::exception& e) {
        qInfo() << "api: syncplay list failed:" << e.what();
        co_return QJsonArray();
    }
}

QCoro::Task<void> JellyfinApiFacade::createSyncPlayGroup(QString name)
{
    const QJsonObject body = { { QStringLiteral("GroupName"), name } };
    co_await requestNoContent(HttpMethod::Post, QStringLiteral("/SyncPlay/New"), QJsonDocument(body));
}

QCoro::Task<void> JellyfinApiFacade::joinSyncPlayGroup(QString groupId)
{
    const QJsonObject body = { { QStringLiteral("GroupId"), groupId } };
    co_await requestNoContent(HttpMethod::Post, QStringLiteral("/SyncPlay/Join"), QJsonDocument(body));
}

QCoro::Task<void> JellyfinApiFacade::leaveSyncPlayGroup()
{
    co_await requestNoContent(HttpMethod::Post, QStringLiteral("/SyncPlay/Leave"), QJsonDocument());
}

QCoro::Task<QJsonObject> JellyfinApiFacade::fetchUtcTime()
{
    co_return (co_await requestJson(HttpMethod::Get, QStringLiteral("/GetUtcTime"))).object();
}

QCoro::Task<void> JellyfinApiFacade::syncPlayReportPing(qint64 pingMs)
{
    const QJsonObject body = {
        { QStringLiteral("Ping"), std::max<qint64>(0, pingMs) },
    };
    co_await requestNoContent(HttpMethod::Post, QStringLiteral("/SyncPlay/Ping"), QJsonDocument(body));
}

QCoro::Task<void> JellyfinApiFacade::syncPlayReportBuffering(
    bool buffering, qint64 positionTicks, bool playing, QString playlistItemId, QDateTime serverTime)
{
    if (playlistItemId.isEmpty()) {
        playlistItemId = QStringLiteral("00000000-0000-0000-0000-000000000000");
    }
    const QJsonObject body = {
        { QStringLiteral("When"), serverTime.toUTC().toString(Qt::ISODateWithMs) },
        { QStringLiteral("PositionTicks"), positionTicks },
        { QStringLiteral("IsPlaying"), playing },
        { QStringLiteral("PlaylistItemId"), playlistItemId },
    };
    const QString path = buffering ? QStringLiteral("/SyncPlay/Buffering") : QStringLiteral("/SyncPlay/Ready");
    co_await requestNoContent(HttpMethod::Post, path, QJsonDocument(body));
}

QCoro::Task<PlaybackSession> JellyfinApiFacade::negotiatePlayback(MovieItem movie)
{
    Diagnostics::Task task(QStringLiteral("api_negotiate_playback"),
        { { QStringLiteral("itemId"), movie.id }, { QStringLiteral("title"), movie.title } });
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), m_session.userId);
    query.addQueryItem(QStringLiteral("fields"), QStringLiteral("Trickplay"));

    const QJsonObject body = {
        { QStringLiteral("UserId"), m_session.userId },
        { QStringLiteral("MaxStreamingBitrate"), m_maxStreamingBitrate },
        { QStringLiteral("StartTimeTicks"), movie.resumeTicks },
        { QStringLiteral("AutoOpenLiveStream"), true },
        { QStringLiteral("EnableDirectPlay"), true },
        { QStringLiteral("EnableDirectStream"), true },
        { QStringLiteral("EnableTranscoding"), true },
        { QStringLiteral("AllowVideoStreamCopy"), true },
        { QStringLiteral("AllowAudioStreamCopy"), true },
        { QStringLiteral("DeviceProfile"), buildDeviceProfile() },
    };

    const QJsonObject playbackResponse
        = (co_await requestJson(
               HttpMethod::Post, QStringLiteral("/Items/%1/PlaybackInfo").arg(movie.id), query, QJsonDocument(body)))
              .object();

    co_return buildPlaybackSession(movie, playbackResponse);
}

QCoro::Task<void> JellyfinApiFacade::postCapabilities()
{
    const QJsonObject body = {
        { QStringLiteral("PlayableMediaTypes"), QJsonArray { QStringLiteral("Video"), QStringLiteral("Audio") } },
        { QStringLiteral("SupportedCommands"),
            QJsonArray {
                QStringLiteral("MoveUp"),
                QStringLiteral("MoveDown"),
                QStringLiteral("MoveLeft"),
                QStringLiteral("MoveRight"),
                QStringLiteral("Select"),
                QStringLiteral("Back"),
                QStringLiteral("SetAudioStreamIndex"),
                QStringLiteral("SetSubtitleStreamIndex"),
                QStringLiteral("ToggleOsd"),
            } },
        { QStringLiteral("SupportsMediaControl"), true },
        { QStringLiteral("SupportsPersistentIdentifier"), true },
        { QStringLiteral("DeviceProfile"), buildDeviceProfile() },
    };

    co_await requestNoContent(HttpMethod::Post, QStringLiteral("/Sessions/Capabilities/Full"), QJsonDocument(body));
}

QJsonArray nowPlayingQueueJson(const std::vector<PlaybackQueueItem>& queue)
{
    QJsonArray array;
    for (const PlaybackQueueItem& item : queue) {
        QJsonObject object { { QStringLiteral("Id"), item.itemId } };
        if (!item.playlistItemId.isEmpty())
            object.insert(QStringLiteral("PlaylistItemId"), item.playlistItemId);
        array.push_back(object);
    }
    return array;
}

QCoro::Task<void> JellyfinApiFacade::reportPlaybackStart(PlaybackSession session)
{
    QJsonObject body = {
        { QStringLiteral("CanSeek"), true },
        { QStringLiteral("ItemId"), session.itemId },
        { QStringLiteral("MediaSourceId"), session.mediaSourceId },
        { QStringLiteral("PlayMethod"), session.playMethod },
        { QStringLiteral("PlaySessionId"), session.playSessionId },
        { QStringLiteral("PositionTicks"), session.startTimeTicks },
    };
    if (!session.nowPlayingQueue.empty())
        body.insert(QStringLiteral("NowPlayingQueue"), nowPlayingQueueJson(session.nowPlayingQueue));

    co_await requestNoContent(HttpMethod::Post, QStringLiteral("/Sessions/Playing"), QJsonDocument(body));
}

QCoro::Task<void> JellyfinApiFacade::reportPlaybackProgress(PlaybackSession session, qint64 positionTicks, bool paused)
{
    QJsonObject body = {
        { QStringLiteral("CanSeek"), true },
        { QStringLiteral("ItemId"), session.itemId },
        { QStringLiteral("MediaSourceId"), session.mediaSourceId },
        { QStringLiteral("PlayMethod"), session.playMethod },
        { QStringLiteral("PlaySessionId"), session.playSessionId },
        { QStringLiteral("PositionTicks"), positionTicks },
        { QStringLiteral("IsPaused"), paused },
    };
    if (!session.nowPlayingQueue.empty())
        body.insert(QStringLiteral("NowPlayingQueue"), nowPlayingQueueJson(session.nowPlayingQueue));

    co_await requestNoContent(HttpMethod::Post, QStringLiteral("/Sessions/Playing/Progress"), QJsonDocument(body));
}

QCoro::Task<void> JellyfinApiFacade::reportPlaybackStopped(PlaybackSession session, qint64 positionTicks, bool failed)
{
    const QJsonObject body = {
        { QStringLiteral("ItemId"), session.itemId },
        { QStringLiteral("MediaSourceId"), session.mediaSourceId },
        { QStringLiteral("PlaySessionId"), session.playSessionId },
        { QStringLiteral("PositionTicks"), positionTicks },
        { QStringLiteral("Failed"), failed },
    };

    co_await requestNoContent(HttpMethod::Post, QStringLiteral("/Sessions/Playing/Stopped"), QJsonDocument(body));
}

QNetworkRequest JellyfinApiFacade::createRequest(const QString& path, const QUrlQuery& query) const
{
    QNetworkRequest request
        = query.isEmpty() ? m_requestFactory.createRequest(path) : m_requestFactory.createRequest(path, query);
    request.setRawHeader("Authorization", authorizationHeader().toUtf8());
    return request;
}

QString JellyfinApiFacade::authorizationHeader(const QString& tokenOverride) const
{
    QStringList parts {
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

void JellyfinApiFacade::applyCommonHeaders()
{
    QHttpHeaders headers;
    headers.append(QHttpHeaders::WellKnownHeader::Accept, QStringLiteral("application/json"));
    if (!m_acceptLanguage.isEmpty())
        headers.append(QHttpHeaders::WellKnownHeader::AcceptLanguage, m_acceptLanguage);
    m_requestFactory.setCommonHeaders(headers);
}

QCoro::Task<QJsonDocument> JellyfinApiFacade::requestJson(
    HttpMethod method, QString path, QUrlQuery query, QJsonDocument body)
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

QCoro::Task<QByteArray> JellyfinApiFacade::requestBytes(
    HttpMethod method, QString path, QUrlQuery query, QJsonDocument body)
{
    const QString methodName = method == HttpMethod::Get ? QStringLiteral("GET")
        : method == HttpMethod::Post                     ? QStringLiteral("POST")
                                                         : QStringLiteral("DELETE");
    const HttpOperation operation = operationFor(method, path);
    const int maximumAttempts = HttpRequestPolicy::maximumAttempts(operation);

    for (int attempt = 1; attempt <= maximumAttempts; ++attempt) {
        if (m_shuttingDown)
            throw std::runtime_error("Request canceled during shutdown");

        const QNetworkRequest request = createRequest(path, query);
        Diagnostics::NetworkRequest diagnosticsRequest(methodName, request.url().toString(QUrl::FullyEncoded));
        QNetworkReply *reply = nullptr;

        if (isQuickConnectPath(path)) {
            qInfo() << "api:" << methodName << request.url().toString(QUrl::FullyEncoded) << "deviceId" << m_deviceId;
        }

        switch (method) {
        case HttpMethod::Get:
            reply = m_rest.get(request);
            break;
        case HttpMethod::Post:
            reply = body.isNull() ? m_rest.post(request, QByteArray {}) : m_rest.post(request, body);
            break;
        case HttpMethod::Delete:
            reply = m_rest.deleteResource(request);
            break;
        }

        m_activeReplies.insert(reply);
        reply = co_await reply;
        m_activeReplies.remove(reply);
        const QByteArray payload = reply && reply->isReadable() ? reply->readAll() : QByteArray {};
        const QString errorText = reply ? reply->errorString() : QStringLiteral("Network reply disappeared");
        const int statusCode = reply ? reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() : 500;
        const auto networkError = reply ? reply->error() : QNetworkReply::UnknownNetworkError;
        if (reply)
            reply->deleteLater();
        diagnosticsRequest.finish(statusCode, networkError == QNetworkReply::NoError ? QString() : errorText);

        if (networkError == QNetworkReply::NoError && statusCode < 400) {
            if (isQuickConnectPath(path)) {
                qInfo() << "api:" << path << "ok" << statusCode << QString::fromUtf8(payload.left(256));
            }
            co_return payload;
        }

        const QString details = payload.isEmpty() ? errorText : QString::fromUtf8(payload);
        if (statusCode == 401 && shouldExpireSession(path) && !m_authExpirationReported) {
            m_authExpirationReported = true;
            emit authenticationExpired(QStringLiteral("Your Jellyfin session has expired. Sign in again."));
        }

        if (!HttpRequestPolicy::shouldRetry(operation, attempt, statusCode, networkError)) {
            if (isQuickConnectPath(path))
                qWarning() << "api:" << path << "failed" << statusCode << details;
            throw std::runtime_error(QStringLiteral("%1 (%2)").arg(details).arg(statusCode).toStdString());
        }

        const int delayMs = HttpRequestPolicy::retryDelayMs(attempt);
        qWarning() << "api:" << methodName << path << "attempt" << attempt << "failed with" << statusCode << errorText
                   << "- retrying in" << delayMs << "ms";
        co_await QCoro::sleepFor(std::chrono::milliseconds(delayMs));
    }

    throw std::runtime_error("HTTP retry policy exhausted");
}

HttpOperation JellyfinApiFacade::operationFor(HttpMethod method, const QString& path) const
{
    if (path.startsWith(QStringLiteral("/Sessions/Playing")))
        return HttpOperation::PlaybackReport;
    return method == HttpMethod::Get ? HttpOperation::Read : HttpOperation::Mutation;
}

bool JellyfinApiFacade::shouldExpireSession(const QString& path) const
{
    return !m_session.accessToken.isEmpty() && !isQuickConnectPath(path)
        && !path.startsWith(QStringLiteral("/Users/Authenticate"));
}

QJsonObject JellyfinApiFacade::buildDeviceProfile() const
{
    return PlaybackNegotiation::buildDeviceProfile(m_maxStreamingBitrate);
}

PlaybackSession JellyfinApiFacade::buildPlaybackSession(
    const MovieItem& movie, const QJsonObject& playbackResponse) const
{
    const QJsonArray mediaSources = playbackResponse.value(QStringLiteral("MediaSources")).toArray();
    if (mediaSources.isEmpty())
        throw std::runtime_error("No media sources returned by Jellyfin");

    const PlaybackSelection selection = PlaybackNegotiation::selectSource(mediaSources, m_preferRemux);
    const QJsonObject selectedSource = selection.source;

    const QString mediaSourceId = requireString(selectedSource, QStringLiteral("Id"));
    const QString container = cleanContainerName(selectedSource.value(QStringLiteral("Container")).toString());

    TrickplayInfo trickplay
        = trickplayFromApiJson(playbackResponse.value(QStringLiteral("Trickplay")).toObject(), mediaSourceId, 320);
    if (trickplay.width <= 0) {
        trickplay
            = trickplayFromApiJson(selectedSource.value(QStringLiteral("Trickplay")).toObject(), mediaSourceId, 320);
    }

    return {
        movie.id,
        movie.title,
        movie.itemType,
        PlaybackNegotiation::buildUrl(m_serverUrl, movie.id, m_session.accessToken, selection),
        mediaSourceId,
        playbackResponse.value(QStringLiteral("PlaySessionId")).toString(),
        selection.playMethod,
        container,
        movie.resumeTicks,
        movie.runtimeTicks,
        mediaStreamsFromApiJson(selectedSource.value(QStringLiteral("MediaStreams")).toArray()),
        {},
        trickplay,
    };
}

} // namespace JellyfinNative
