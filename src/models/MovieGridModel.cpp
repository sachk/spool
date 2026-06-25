#include "MovieGridModel.h"

#include <algorithm>

namespace JellyfinNative {

namespace {

double playbackProgress(const MovieItem &movie)
{
    const qint64 resumeTicks = normalizedResumeTicks(movie.resumeTicks, movie.runtimeTicks);
    if (resumeTicks <= 0 || movie.runtimeTicks <= 0)
        return 0.0;
    return std::clamp(static_cast<double>(resumeTicks) / static_cast<double>(movie.runtimeTicks), 0.0, 1.0);
}

qint64 displayResumeTicks(const MovieItem &movie)
{
    return normalizedResumeTicks(movie.resumeTicks, movie.runtimeTicks);
}

QString displayTitle(const MovieItem &movie)
{
    if (movie.itemType == QStringLiteral("Episode") && !movie.seriesName.isEmpty())
        return movie.seriesName;
    return movie.title;
}

QString displaySubtitle(const MovieItem &movie)
{
    if (movie.itemType == QStringLiteral("Episode")) {
        if (!movie.subtitle.isEmpty() && !movie.title.isEmpty())
            return QStringLiteral("%1 · %2").arg(movie.subtitle, movie.title);
        if (!movie.title.isEmpty())
            return movie.title;
    }
    return movie.subtitle;
}

QVariantList peopleVariantList(const std::vector<PersonItem> &people)
{
    QVariantList result;
    result.reserve(static_cast<qsizetype>(people.size()));
    for (const PersonItem &person : people) {
        result.push_back(QVariantMap{
            {QStringLiteral("personId"), person.id},
            {QStringLiteral("name"), person.name},
            {QStringLiteral("type"), person.type},
            {QStringLiteral("role"), person.role},
            {QStringLiteral("imageUrl"), person.imageUrl},
            {QStringLiteral("imageTag"), person.imageTag},
        });
    }
    return result;
}

QVariantList mediaStreamsVariantList(const std::vector<MediaStreamInfo> &streams)
{
    QVariantList result;
    result.reserve(static_cast<qsizetype>(streams.size()));
    for (const MediaStreamInfo &stream : streams) {
        result.push_back(QVariantMap{
            {QStringLiteral("index"), stream.index},
            {QStringLiteral("type"), stream.type},
            {QStringLiteral("codec"), stream.codec},
            {QStringLiteral("profile"), stream.profile},
            {QStringLiteral("displayTitle"), stream.displayTitle},
            {QStringLiteral("title"), stream.title},
            {QStringLiteral("language"), stream.language},
            {QStringLiteral("pixelFormat"), stream.pixelFormat},
            {QStringLiteral("videoRange"), stream.videoRange},
            {QStringLiteral("colorPrimaries"), stream.colorPrimaries},
            {QStringLiteral("colorTransfer"), stream.colorTransfer},
            {QStringLiteral("colorSpace"), stream.colorSpace},
            {QStringLiteral("aspectRatio"), stream.aspectRatio},
            {QStringLiteral("width"), stream.width},
            {QStringLiteral("height"), stream.height},
            {QStringLiteral("frameRate"), stream.frameRate},
            {QStringLiteral("bitRate"), stream.bitRate},
            {QStringLiteral("bitDepth"), stream.bitDepth},
            {QStringLiteral("channels"), stream.channels},
            {QStringLiteral("sampleRate"), stream.sampleRate},
            {QStringLiteral("isDefault"), stream.isDefault},
            {QStringLiteral("isForced"), stream.isForced},
            {QStringLiteral("isExternal"), stream.isExternal},
            {QStringLiteral("isInterlaced"), stream.isInterlaced},
        });
    }
    return result;
}

QVariantList mediaSourcesVariantList(const std::vector<MediaSourceInfo> &sources)
{
    QVariantList result;
    result.reserve(static_cast<qsizetype>(sources.size()));
    for (const MediaSourceInfo &source : sources) {
        result.push_back(QVariantMap{
            {QStringLiteral("id"), source.id},
            {QStringLiteral("name"), source.name},
            {QStringLiteral("path"), source.path},
            {QStringLiteral("container"), source.container},
            {QStringLiteral("protocol"), source.protocol},
            {QStringLiteral("videoType"), source.videoType},
            {QStringLiteral("size"), QVariant::fromValue(source.size)},
            {QStringLiteral("bitRate"), source.bitRate},
            {QStringLiteral("runtimeTicks"), QVariant::fromValue(source.runtimeTicks)},
            {QStringLiteral("streams"), mediaStreamsVariantList(source.streams)},
        });
    }
    return result;
}

}

MovieGridModel::MovieGridModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MovieGridModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_movies.size());
}

int MovieGridModel::count() const
{
    return rowCount();
}

QVariant MovieGridModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};

    const auto &movie = m_movies[static_cast<size_t>(index.row())];
    switch (role) {
    case IdRole:
        return movie.id;
    case TitleRole:
        return movie.title;
    case OverviewRole:
        return movie.overview;
    case PosterUrlRole:
        return movie.posterUrl;
    case PosterTagRole:
        return movie.posterTag;
    case YearRole:
        return movie.year;
    case ItemTypeRole:
        return movie.itemType;
    case SeasonIdRole:
        return movie.seasonId;
    case SubtitleRole:
        return movie.subtitle;
    case PathRole:
        return movie.path;
    case SeasonNumberRole:
        return movie.seasonNumber;
    case EpisodeNumberRole:
        return movie.episodeNumber;
    case ResumeTicksRole:
        return QVariant::fromValue(displayResumeTicks(movie));
    case RuntimeTicksRole:
        return QVariant::fromValue(movie.runtimeTicks);
    case ProgressRole:
        return playbackProgress(movie);
    case SeriesNameRole:
        return movie.seriesName;
    case SeriesPosterUrlRole:
        return movie.seriesPosterUrl;
    case DisplayTitleRole:
        return displayTitle(movie);
    case DisplaySubtitleRole:
        return displaySubtitle(movie);
    case PlayActionLabelRole:
        return displayResumeTicks(movie) > 0 ? QStringLiteral("Resume") : QStringLiteral("Play");
    case PlayableRole:
        return movie.playable;
    case FavoriteRole:
        return movie.favorite;
    case PlayedRole:
        return movie.played;
    case BackdropUrlRole:
        return movie.backdropUrl;
    case LogoUrlRole:
        return movie.logoUrl;
    case BannerUrlRole:
        return movie.bannerUrl;
    case ThumbUrlRole:
        return movie.thumbUrl;
    case GenresRole:
        return movie.genres;
    case TagsRole:
        return movie.tags;
    case StudiosRole:
        return movie.studios;
    case OfficialRatingRole:
        return movie.officialRating;
    case CommunityRatingRole:
        return movie.communityRating;
    case CriticRatingRole:
        return movie.criticRating;
    case PremiereDateRole:
        return movie.premiereDate;
    case EndDateRole:
        return movie.endDate;
    case PeopleRole:
        return peopleVariantList(movie.people);
    case MediaSourcesRole:
        return mediaSourcesVariantList(movie.mediaSources);
    default:
        return {};
    }
}

QHash<int, QByteArray> MovieGridModel::roleNames() const
{
    return {
        {IdRole, "movieId"},
        {TitleRole, "title"},
        {OverviewRole, "overview"},
        {PosterUrlRole, "posterUrl"},
        {PosterTagRole, "posterTag"},
        {YearRole, "year"},
        {ItemTypeRole, "itemType"},
        {SeasonIdRole, "seasonId"},
        {SubtitleRole, "subtitle"},
        {PathRole, "path"},
        {SeasonNumberRole, "seasonNumber"},
        {EpisodeNumberRole, "episodeNumber"},
        {ResumeTicksRole, "resumeTicks"},
        {RuntimeTicksRole, "runtimeTicks"},
        {ProgressRole, "progress"},
        {SeriesNameRole, "seriesName"},
        {SeriesPosterUrlRole, "seriesPosterUrl"},
        {DisplayTitleRole, "displayTitle"},
        {DisplaySubtitleRole, "displaySubtitle"},
        {PlayActionLabelRole, "playActionLabel"},
        {PlayableRole, "playable"},
        {FavoriteRole, "favorite"},
        {PlayedRole, "played"},
        {BackdropUrlRole, "backdropUrl"},
        {LogoUrlRole, "logoUrl"},
        {BannerUrlRole, "bannerUrl"},
        {ThumbUrlRole, "thumbUrl"},
        {GenresRole, "genres"},
        {TagsRole, "tags"},
        {StudiosRole, "studios"},
        {OfficialRatingRole, "officialRating"},
        {CommunityRatingRole, "communityRating"},
        {CriticRatingRole, "criticRating"},
        {PremiereDateRole, "premiereDate"},
        {EndDateRole, "endDate"},
        {PeopleRole, "people"},
        {MediaSourcesRole, "mediaSources"},
    };
}

QVariantMap MovieGridModel::get(int index) const
{
    if (index < 0 || index >= rowCount())
        return {};

    const auto &movie = m_movies[static_cast<size_t>(index)];
    return {
        {QStringLiteral("movieId"), movie.id},
        {QStringLiteral("title"), movie.title},
        {QStringLiteral("overview"), movie.overview},
        {QStringLiteral("posterUrl"), movie.posterUrl},
        {QStringLiteral("posterTag"), movie.posterTag},
        {QStringLiteral("year"), movie.year},
        {QStringLiteral("itemType"), movie.itemType},
        {QStringLiteral("seasonId"), movie.seasonId},
        {QStringLiteral("subtitle"), movie.subtitle},
        {QStringLiteral("path"), movie.path},
        {QStringLiteral("seasonNumber"), movie.seasonNumber},
        {QStringLiteral("episodeNumber"), movie.episodeNumber},
        {QStringLiteral("resumeTicks"), QVariant::fromValue(displayResumeTicks(movie))},
        {QStringLiteral("runtimeTicks"), QVariant::fromValue(movie.runtimeTicks)},
        {QStringLiteral("progress"), playbackProgress(movie)},
        {QStringLiteral("seriesName"), movie.seriesName},
        {QStringLiteral("seriesPosterUrl"), movie.seriesPosterUrl},
        {QStringLiteral("displayTitle"), displayTitle(movie)},
        {QStringLiteral("displaySubtitle"), displaySubtitle(movie)},
        {QStringLiteral("playActionLabel"), displayResumeTicks(movie) > 0 ? QStringLiteral("Resume") : QStringLiteral("Play")},
        {QStringLiteral("playable"), movie.playable},
        {QStringLiteral("favorite"), movie.favorite},
        {QStringLiteral("played"), movie.played},
        {QStringLiteral("backdropUrl"), movie.backdropUrl},
        {QStringLiteral("logoUrl"), movie.logoUrl},
        {QStringLiteral("bannerUrl"), movie.bannerUrl},
        {QStringLiteral("thumbUrl"), movie.thumbUrl},
        {QStringLiteral("genres"), movie.genres},
        {QStringLiteral("tags"), movie.tags},
        {QStringLiteral("studios"), movie.studios},
        {QStringLiteral("officialRating"), movie.officialRating},
        {QStringLiteral("communityRating"), movie.communityRating},
        {QStringLiteral("criticRating"), movie.criticRating},
        {QStringLiteral("premiereDate"), movie.premiereDate},
        {QStringLiteral("endDate"), movie.endDate},
        {QStringLiteral("people"), peopleVariantList(movie.people)},
        {QStringLiteral("mediaSources"), mediaSourcesVariantList(movie.mediaSources)},
    };
}

void MovieGridModel::setMovies(const std::vector<MovieItem> &movies)
{
    const int oldCount = rowCount();
    if (oldCount == static_cast<int>(movies.size())) {
        m_movies = movies;
        if (oldCount > 0)
            emit dataChanged(index(0, 0), index(oldCount - 1, 0));
        return;
    }

    beginResetModel();
    m_movies = movies;
    endResetModel();
    emit countChanged();
}

void MovieGridModel::appendMovies(const std::vector<MovieItem> &movies)
{
    if (movies.empty())
        return;

    const int first = rowCount();
    const int last = first + static_cast<int>(movies.size()) - 1;
    beginInsertRows({}, first, last);
    m_movies.insert(m_movies.end(), movies.begin(), movies.end());
    endInsertRows();
    emit countChanged();
}

void MovieGridModel::clear()
{
    const int oldCount = rowCount();
    beginResetModel();
    m_movies.clear();
    endResetModel();
    if (oldCount != 0)
        emit countChanged();
}

MovieItem MovieGridModel::movieAt(int index) const
{
    if (index < 0 || index >= rowCount())
        return {};
    return m_movies[static_cast<size_t>(index)];
}

const std::vector<MovieItem> &MovieGridModel::movies() const
{
    return m_movies;
}

bool MovieGridModel::updateResumeTicks(const QString &itemId, qint64 resumeTicks)
{
    if (itemId.isEmpty())
        return false;

    bool updated = false;
    for (int row = 0; row < rowCount(); ++row) {
        auto &movie = m_movies[static_cast<size_t>(row)];
        const qint64 normalizedTicks = normalizedResumeTicks(resumeTicks, movie.runtimeTicks);
        if (movie.id != itemId || movie.resumeTicks == normalizedTicks)
            continue;

        movie.resumeTicks = normalizedTicks;
        const QModelIndex changed = index(row, 0);
        emit dataChanged(changed, changed, {ResumeTicksRole, ProgressRole, PlayActionLabelRole});
        updated = true;
    }
    return updated;
}

bool MovieGridModel::updateFavorite(const QString &itemId, bool favorite)
{
    if (itemId.isEmpty())
        return false;

    bool updated = false;
    for (int row = 0; row < rowCount(); ++row) {
        auto &movie = m_movies[static_cast<size_t>(row)];
        if (movie.id != itemId || movie.favorite == favorite)
            continue;

        movie.favorite = favorite;
        const QModelIndex changed = index(row, 0);
        emit dataChanged(changed, changed, {FavoriteRole});
        updated = true;
    }
    return updated;
}

bool MovieGridModel::updatePlayed(const QString &itemId, bool played)
{
    if (itemId.isEmpty())
        return false;

    bool updated = false;
    for (int row = 0; row < rowCount(); ++row) {
        auto &movie = m_movies[static_cast<size_t>(row)];
        const bool clearsResume = !played && movie.resumeTicks != 0;
        if (movie.id != itemId || (movie.played == played && !clearsResume))
            continue;

        movie.played = played;
        movie.resumeTicks = 0;
        const QModelIndex changed = index(row, 0);
        emit dataChanged(changed, changed, {PlayedRole, ResumeTicksRole, ProgressRole, PlayActionLabelRole});
        updated = true;
    }
    return updated;
}

bool MovieGridModel::removeUnresumable()
{
    bool removed = false;
    for (int row = rowCount() - 1; row >= 0; --row) {
        const MovieItem &movie = m_movies[static_cast<size_t>(row)];
        if (isMeaningfulResumePosition(movie.resumeTicks, movie.runtimeTicks))
            continue;

        beginRemoveRows({}, row, row);
        m_movies.erase(m_movies.begin() + row);
        endRemoveRows();
        removed = true;
    }
    if (removed)
        emit countChanged();
    return removed;
}

} // namespace JellyfinNative
