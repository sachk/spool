#include "JellyfinTypes.h"
#include "../platform/PlatformSettingsPolicy.h"
#include "MetaJson.h"

#include <QJsonValue>
#include <QRegularExpression>
#include <QUrl>

#include <stdexcept>

namespace JellyfinNative {

namespace {

    constexpr qint64 kTicksPerSecond = 10000000;

    QString browseKindKey(BrowseKind kind)
    {
        switch (kind) {
        case BrowseKind::Library:
            return QStringLiteral("library");
        case BrowseKind::FolderChildren:
            return QStringLiteral("folderChildren");
        case BrowseKind::Person:
            return QStringLiteral("person");
        case BrowseKind::Genre:
            return QStringLiteral("genre");
        case BrowseKind::Studio:
            return QStringLiteral("studio");
        case BrowseKind::SeriesSeasons:
            return QStringLiteral("seriesSeasons");
        case BrowseKind::SeasonEpisodes:
            return QStringLiteral("seasonEpisodes");
        case BrowseKind::Playlist:
            return QStringLiteral("playlist");
        case BrowseKind::BoxSet:
            return QStringLiteral("boxset");
        case BrowseKind::ArtistAlbums:
            return QStringLiteral("artistAlbums");
        case BrowseKind::None:
            break;
        }
        return QStringLiteral("none");
    }

    BrowseKind browseKindFromKey(const QString& key)
    {
        if (key == QStringLiteral("library"))
            return BrowseKind::Library;
        if (key == QStringLiteral("folderChildren"))
            return BrowseKind::FolderChildren;
        if (key == QStringLiteral("person"))
            return BrowseKind::Person;
        if (key == QStringLiteral("genre"))
            return BrowseKind::Genre;
        if (key == QStringLiteral("studio"))
            return BrowseKind::Studio;
        if (key == QStringLiteral("seriesSeasons"))
            return BrowseKind::SeriesSeasons;
        if (key == QStringLiteral("seasonEpisodes"))
            return BrowseKind::SeasonEpisodes;
        if (key == QStringLiteral("playlist"))
            return BrowseKind::Playlist;
        if (key == QStringLiteral("boxset"))
            return BrowseKind::BoxSet;
        if (key == QStringLiteral("artistAlbums"))
            return BrowseKind::ArtistAlbums;
        return BrowseKind::None;
    }

    QString queryValueSignature(const QVariant& value)
    {
        if (value.typeId() == QMetaType::QStringList) {
            QStringList items = value.toStringList();
            items.sort();
            return items.join(QLatin1Char(','));
        }
        if (value.typeId() == QMetaType::QVariantList) {
            QStringList items;
            const QVariantList values = value.toList();
            items.reserve(values.size());
            for (const QVariant& item : values)
                items.push_back(item.toString());
            items.sort();
            return items.join(QLatin1Char(','));
        }
        if (value.typeId() == QMetaType::Bool)
            return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        return value.toString();
    }

    QString querySignature(const QVariantMap& query)
    {
        QStringList keys = query.keys();
        keys.sort();

        QStringList parts;
        parts.reserve(keys.size());
        for (const QString& key : keys) {
            if (key.isEmpty())
                continue;
            const QVariant value = query.value(key);
            if (!value.isValid() || value.isNull())
                continue;
            const QString encodedKey = QString::fromLatin1(QUrl::toPercentEncoding(key));
            const QString encodedValue = QString::fromLatin1(QUrl::toPercentEncoding(queryValueSignature(value)));
            parts.push_back(QStringLiteral("%1=%2").arg(encodedKey, encodedValue));
        }
        return parts.join(QLatin1Char('&'));
    }

    QString serverPath(QString basePath, const QStringList& segments)
    {
        while (basePath.endsWith(QLatin1Char('/')))
            basePath.chop(1);
        for (const QString& segment : segments) {
            if (!segment.isEmpty())
                basePath += QLatin1Char('/') + QString::fromLatin1(QUrl::toPercentEncoding(segment));
        }
        return basePath.isEmpty() ? QStringLiteral("/") : basePath;
    }

}

BrowseDescriptor BrowseDescriptor::library(QString libraryId, QString collectionType, QString name)
{
    BrowseDescriptor descriptor;
    descriptor.kind = BrowseKind::Library;
    descriptor.id = libraryId;
    descriptor.collectionType = collectionType;
    descriptor.name = name;
    return descriptor;
}

BrowseDescriptor BrowseDescriptor::folderChildren(QString folderId, QString name)
{
    BrowseDescriptor descriptor;
    descriptor.kind = BrowseKind::FolderChildren;
    descriptor.id = folderId;
    descriptor.name = name;
    return descriptor;
}

BrowseDescriptor BrowseDescriptor::person(QString personId, QString name)
{
    BrowseDescriptor descriptor;
    descriptor.kind = BrowseKind::Person;
    descriptor.id = personId;
    descriptor.name = name;
    return descriptor;
}

BrowseDescriptor BrowseDescriptor::genre(QString name)
{
    BrowseDescriptor descriptor;
    descriptor.kind = BrowseKind::Genre;
    descriptor.name = name;
    return descriptor;
}

BrowseDescriptor BrowseDescriptor::studio(QString name)
{
    BrowseDescriptor descriptor;
    descriptor.kind = BrowseKind::Studio;
    descriptor.name = name;
    return descriptor;
}

BrowseDescriptor BrowseDescriptor::seriesSeasons(QString seriesId, QString seriesName)
{
    BrowseDescriptor descriptor;
    descriptor.kind = BrowseKind::SeriesSeasons;
    descriptor.id = seriesId;
    descriptor.seriesId = seriesId;
    descriptor.name = seriesName;
    return descriptor;
}

BrowseDescriptor BrowseDescriptor::seasonEpisodes(QString seriesId, QString seasonId, QString seasonName)
{
    BrowseDescriptor descriptor;
    descriptor.kind = BrowseKind::SeasonEpisodes;
    descriptor.id = seasonId.isEmpty() ? seriesId : seasonId;
    descriptor.seriesId = seriesId;
    descriptor.seasonId = seasonId;
    descriptor.name = seasonName;
    return descriptor;
}

BrowseDescriptor BrowseDescriptor::playlist(QString playlistId, QString name)
{
    BrowseDescriptor descriptor;
    descriptor.kind = BrowseKind::Playlist;
    descriptor.id = playlistId;
    descriptor.name = name;
    return descriptor;
}

BrowseDescriptor BrowseDescriptor::boxSet(QString boxSetId, QString name)
{
    BrowseDescriptor descriptor;
    descriptor.kind = BrowseKind::BoxSet;
    descriptor.id = boxSetId;
    descriptor.name = name;
    return descriptor;
}

BrowseDescriptor BrowseDescriptor::artistAlbums(QString artistId, QString artistName)
{
    BrowseDescriptor descriptor;
    descriptor.kind = BrowseKind::ArtistAlbums;
    descriptor.id = artistId;
    descriptor.name = artistName;
    return descriptor;
}

bool BrowseDescriptor::isValid() const
{
    switch (kind) {
    case BrowseKind::Genre:
    case BrowseKind::Studio:
        return !name.trimmed().isEmpty();
    case BrowseKind::SeasonEpisodes:
        return !seriesId.isEmpty();
    case BrowseKind::Library:
    case BrowseKind::FolderChildren:
    case BrowseKind::Person:
    case BrowseKind::SeriesSeasons:
    case BrowseKind::Playlist:
    case BrowseKind::BoxSet:
    case BrowseKind::ArtistAlbums:
        return !id.isEmpty();
    case BrowseKind::None:
        break;
    }
    return false;
}

QString BrowseDescriptor::kindKey() const
{
    return browseKindKey(kind);
}

QString BrowseDescriptor::cacheKey(const QVariantMap& query) const
{
    QString key = kindKey();
    if (!seriesId.isEmpty())
        key += QStringLiteral("/series/%1").arg(seriesId);
    if (!id.isEmpty() && id != seriesId)
        key += QStringLiteral("/%1").arg(id);
    if (!seasonId.isEmpty() && seasonId != id)
        key += QStringLiteral("/season/%1").arg(seasonId);
    if (id.isEmpty() && !name.isEmpty())
        key += QStringLiteral("/%1").arg(name);
    if (kind == BrowseKind::Library && !collectionType.isEmpty())
        key += QStringLiteral("/%1").arg(collectionType);

    const QString signature = querySignature(query);
    return signature.isEmpty() ? key : QStringLiteral("%1?%2").arg(key, signature);
}

QString exceptionMessage(const std::exception_ptr& exception)
{
    if (!exception)
        return QStringLiteral("Unknown error");

    try {
        std::rethrow_exception(exception);
    } catch (const std::exception& error) {
        return QString::fromUtf8(error.what());
    } catch (...) {
        return QStringLiteral("Unknown error");
    }
}

QString normalizedAudioOutputMode(const QString& mode)
{
    return normalizedPlatformAudioOutputMode(mode);
}

QString sanitizedDiagnosticUrl(QString url, qsizetype maxLength)
{
    static const QRegularExpression secretQuery(
        QStringLiteral("([?&](?:api_key|access_token|token)=)[^&]+"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression tokenHeader(
        QStringLiteral("(X-Emby-Token\\s*[:=]\\s*)[^,\\r\\n]+"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression authorizationHeader(
        QStringLiteral("(Authorization\\s*[:=]\\s*)[^\\r\\n]+"), QRegularExpression::CaseInsensitiveOption);
    url.replace(secretQuery, QStringLiteral("\\1<redacted>"));
    url.replace(tokenHeader, QStringLiteral("\\1<redacted>"));
    url.replace(authorizationHeader, QStringLiteral("\\1<redacted>"));
    return maxLength >= 0 ? url.left(maxLength) : url;
}

QUrl serverUrlWithPath(const QString& serverUrl, const QStringList& segments)
{
    QUrl url(serverUrl);
    url.setPath(serverPath(url.path(), segments), QUrl::StrictMode);
    return url;
}

bool MovieItem::isPlayable() const
{
    return isPlayableItem(*this);
}

QString MovieItem::subtitle() const
{
    return itemSubtitle(*this);
}

bool isPlayableItem(const MovieItem& item)
{
    if (item.isVirtualItem || item.locationType.compare(QStringLiteral("Virtual"), Qt::CaseInsensitive) == 0)
        return false;
    return item.itemType == QStringLiteral("Movie") || item.itemType == QStringLiteral("Episode")
        || item.itemType == QStringLiteral("MusicVideo") || item.itemType == QStringLiteral("Video")
        || item.itemType == QStringLiteral("Audio") || item.itemType == QStringLiteral("AudioBook")
        || item.itemType == QStringLiteral("Trailer");
}

QString itemSubtitle(const MovieItem& item)
{
    if (item.itemType == QStringLiteral("Series")) {
        if (!item.episodeLabel.isEmpty())
            return item.episodeLabel;
        return item.year > 0 ? QString::number(item.year) : QStringLiteral("Series");
    }
    if (item.itemType == QStringLiteral("Season"))
        return QStringLiteral("Season");
    if (item.itemType == QStringLiteral("Episode")) {
        if (!item.episodeLabel.isEmpty())
            return item.episodeLabel;
        if (item.seasonNumber > 0 && item.episodeNumber > 0) {
            return QStringLiteral("S%1:E%2")
                .arg(item.seasonNumber, 2, 10, QLatin1Char('0'))
                .arg(item.episodeNumber, 2, 10, QLatin1Char('0'));
        }
        return item.episodeNumber > 0 ? QStringLiteral("Episode %1").arg(item.episodeNumber)
                                      : QStringLiteral("Episode");
    }
    return item.year > 0 ? QString::number(item.year) : QString();
}

QString itemDisplaySubtitle(const MovieItem& item)
{
    const QString subtitle = itemSubtitle(item);
    if (item.itemType == QStringLiteral("Episode") && !item.title.isEmpty())
        return subtitle.isEmpty() ? item.title : QStringLiteral("%1 · %2").arg(subtitle, item.title);
    return subtitle;
}

QString itemEpisodeCode(const MovieItem& item)
{
    if (item.itemType != QStringLiteral("Episode") || item.seasonNumber <= 0 || item.episodeNumber <= 0)
        return {};
    return QStringLiteral("S%1E%2")
        .arg(item.seasonNumber, 2, 10, QLatin1Char('0'))
        .arg(item.episodeNumber, 2, 10, QLatin1Char('0'));
}

bool isGenericEpisodeTitle(const MovieItem& item)
{
    if (item.itemType != QStringLiteral("Episode") || item.episodeNumber <= 0)
        return false;
    const QString title = item.title.simplified();
    constexpr QLatin1StringView prefix("Episode ");
    if (!title.startsWith(prefix, Qt::CaseInsensitive))
        return false;
    bool validNumber = false;
    const int titleNumber = title.sliced(prefix.size()).toInt(&validNumber);
    return validNumber && titleNumber == item.episodeNumber;
}

int episodicPlaybackStartIndex(const std::vector<MovieItem>& episodes)
{
    int lastPlayed = -1;
    for (int index = 0; index < static_cast<int>(episodes.size()); ++index) {
        if (episodes[static_cast<size_t>(index)].played)
            lastPlayed = index;
    }
    for (int index = lastPlayed + 1; index < static_cast<int>(episodes.size()); ++index) {
        if (isPlayableItem(episodes[static_cast<size_t>(index)]))
            return index;
    }
    return -1;
}

bool isMeaningfulResumePosition(qint64 resumeTicks, qint64 runtimeTicks)
{
    if (resumeTicks < 5 * kTicksPerSecond)
        return false;
    if (runtimeTicks <= 0)
        return true;

    const qint64 remainingTicks = runtimeTicks - resumeTicks;
    return resumeTicks < runtimeTicks && resumeTicks * 100 < runtimeTicks * 95 && remainingTicks > 30 * kTicksPerSecond;
}

qint64 normalizedResumeTicks(qint64 resumeTicks, qint64 runtimeTicks)
{
    return isMeaningfulResumePosition(resumeTicks, runtimeTicks) ? resumeTicks : 0;
}

} // namespace JellyfinNative
