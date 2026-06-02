#include "MovieGridModel.h"

#include <algorithm>

namespace JellyfinNative {

namespace {

double playbackProgress(const MovieItem &movie)
{
    if (movie.resumeTicks <= 0 || movie.runtimeTicks <= 0)
        return 0.0;
    return std::clamp(static_cast<double>(movie.resumeTicks) / static_cast<double>(movie.runtimeTicks), 0.0, 1.0);
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
    case SubtitleRole:
        return movie.subtitle;
    case PathRole:
        return movie.path;
    case SeasonNumberRole:
        return movie.seasonNumber;
    case EpisodeNumberRole:
        return movie.episodeNumber;
    case ResumeTicksRole:
        return QVariant::fromValue(movie.resumeTicks);
    case RuntimeTicksRole:
        return QVariant::fromValue(movie.runtimeTicks);
    case ProgressRole:
        return playbackProgress(movie);
    case SeriesNameRole:
        return movie.seriesName;
    case DisplayTitleRole:
        return displayTitle(movie);
    case DisplaySubtitleRole:
        return displaySubtitle(movie);
    case PlayActionLabelRole:
        return movie.resumeTicks > 0 ? QStringLiteral("Resume") : QStringLiteral("Play");
    case PlayableRole:
        return movie.playable;
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
        {SubtitleRole, "subtitle"},
        {PathRole, "path"},
        {SeasonNumberRole, "seasonNumber"},
        {EpisodeNumberRole, "episodeNumber"},
        {ResumeTicksRole, "resumeTicks"},
        {RuntimeTicksRole, "runtimeTicks"},
        {ProgressRole, "progress"},
        {SeriesNameRole, "seriesName"},
        {DisplayTitleRole, "displayTitle"},
        {DisplaySubtitleRole, "displaySubtitle"},
        {PlayActionLabelRole, "playActionLabel"},
        {PlayableRole, "playable"},
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
        {QStringLiteral("subtitle"), movie.subtitle},
        {QStringLiteral("path"), movie.path},
        {QStringLiteral("seasonNumber"), movie.seasonNumber},
        {QStringLiteral("episodeNumber"), movie.episodeNumber},
        {QStringLiteral("resumeTicks"), QVariant::fromValue(movie.resumeTicks)},
        {QStringLiteral("runtimeTicks"), QVariant::fromValue(movie.runtimeTicks)},
        {QStringLiteral("progress"), playbackProgress(movie)},
        {QStringLiteral("seriesName"), movie.seriesName},
        {QStringLiteral("displayTitle"), displayTitle(movie)},
        {QStringLiteral("displaySubtitle"), displaySubtitle(movie)},
        {QStringLiteral("playActionLabel"), movie.resumeTicks > 0 ? QStringLiteral("Resume") : QStringLiteral("Play")},
        {QStringLiteral("playable"), movie.playable},
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
    };
}

void MovieGridModel::setMovies(const std::vector<MovieItem> &movies)
{
    beginResetModel();
    m_movies = movies;
    endResetModel();
}

void MovieGridModel::clear()
{
    beginResetModel();
    m_movies.clear();
    endResetModel();
}

MovieItem MovieGridModel::movieAt(int index) const
{
    if (index < 0 || index >= rowCount())
        return {};
    return m_movies[static_cast<size_t>(index)];
}

bool MovieGridModel::updateResumeTicks(const QString &itemId, qint64 resumeTicks)
{
    if (itemId.isEmpty())
        return false;

    bool updated = false;
    for (int row = 0; row < rowCount(); ++row) {
        auto &movie = m_movies[static_cast<size_t>(row)];
        if (movie.id != itemId || movie.resumeTicks == resumeTicks)
            continue;

        movie.resumeTicks = resumeTicks;
        const QModelIndex changed = index(row, 0);
        emit dataChanged(changed, changed, {ResumeTicksRole, ProgressRole, PlayActionLabelRole});
        updated = true;
    }
    return updated;
}

} // namespace JellyfinNative
