#include "JellyfinTypes.h"
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

QVariantMap BrowseDescriptor::toVariantMap() const
{
    QVariantMap map;
    map.insert(QStringLiteral("kind"), kindKey());
    map.insert(QString::fromLatin1("id"), id);
    map.insert(QStringLiteral("name"), name);
    map.insert(QStringLiteral("collectionType"), collectionType);
    map.insert(QStringLiteral("seriesId"), seriesId);
    map.insert(QStringLiteral("seasonId"), seasonId);
    return map;
}

BrowseDescriptor BrowseDescriptor::fromVariantMap(const QVariantMap& map)
{
    BrowseDescriptor descriptor;
    descriptor.kind = browseKindFromKey(map.value(QStringLiteral("kind")).toString());
    descriptor.id = map.value(QString::fromLatin1("id")).toString();
    descriptor.name = map.value(QStringLiteral("name")).toString();
    descriptor.collectionType = map.value(QStringLiteral("collectionType")).toString();
    descriptor.seriesId = map.value(QStringLiteral("seriesId")).toString();
    descriptor.seasonId = map.value(QStringLiteral("seasonId")).toString();
    return descriptor;
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
    return (mode == QStringLiteral("starfish") || mode == QStringLiteral("starfish-pcm"))
        ? QStringLiteral("starfish-pcm")
        : QStringLiteral("alsa");
}

QString sanitizedDiagnosticUrl(QString url, qsizetype maxLength)
{
    static const QRegularExpression secretQuery(
        QStringLiteral("([?&](?:api_key|access_token|token)=)[^&]+"), QRegularExpression::CaseInsensitiveOption);
    url.replace(secretQuery, QStringLiteral("\\1<redacted>"));
    return maxLength >= 0 ? url.left(maxLength) : url;
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
