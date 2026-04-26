#include "MovieGridModel.h"

namespace JellyfinNative {

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
    case SeasonNumberRole:
        return movie.seasonNumber;
    case EpisodeNumberRole:
        return movie.episodeNumber;
    case PlayableRole:
        return movie.playable;
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
        {SeasonNumberRole, "seasonNumber"},
        {EpisodeNumberRole, "episodeNumber"},
        {PlayableRole, "playable"},
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
        {QStringLiteral("seasonNumber"), movie.seasonNumber},
        {QStringLiteral("episodeNumber"), movie.episodeNumber},
        {QStringLiteral("playable"), movie.playable},
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

} // namespace JellyfinNative
