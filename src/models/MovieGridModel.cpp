#include "MovieGridModel.h"

#include <algorithm>

namespace JellyfinNative {

namespace {

    double playbackProgress(const MovieItem& movie)
    {
        const qint64 resumeTicks = normalizedResumeTicks(movie.resumeTicks, movie.runtimeTicks);
        if (resumeTicks <= 0 || movie.runtimeTicks <= 0)
            return 0.0;
        return std::clamp(static_cast<double>(resumeTicks) / static_cast<double>(movie.runtimeTicks), 0.0, 1.0);
    }

    qint64 displayResumeTicks(const MovieItem& movie)
    {
        return normalizedResumeTicks(movie.resumeTicks, movie.runtimeTicks);
    }

    QString displayTitle(const MovieItem& movie)
    {
        if (movie.itemType == QStringLiteral("Episode") && !movie.seriesName.isEmpty())
            return movie.seriesName;
        return movie.title;
    }

    QString displaySubtitle(const MovieItem& movie)
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

int MovieGridModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_movies.size());
}

int MovieGridModel::count() const
{
    return rowCount();
}

QVariant MovieGridModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};

    const auto& movie = m_movies[static_cast<size_t>(index.row())];
    switch (role) {
    case ItemRole:
        return QVariant::fromValue(movie);
    case DisplayTitleRole:
        return displayTitle(movie);
    case DisplaySubtitleRole:
        return displaySubtitle(movie);
    case ProgressRole:
        return playbackProgress(movie);
    case PlayActionLabelRole:
        return displayResumeTicks(movie) > 0 ? QStringLiteral("Resume") : QStringLiteral("Play");
    default:
        return {};
    }
}

QHash<int, QByteArray> MovieGridModel::roleNames() const
{
    return {
        { ItemRole, "item" },
        { DisplayTitleRole, "displayTitle" },
        { DisplaySubtitleRole, "displaySubtitle" },
        { ProgressRole, "progress" },
        { PlayActionLabelRole, "playActionLabel" },
    };
}

MovieItem MovieGridModel::get(int index) const
{
    return movieAt(index);
}

void MovieGridModel::setMovies(const std::vector<MovieItem>& movies)
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

void MovieGridModel::appendMovies(const std::vector<MovieItem>& movies)
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

const std::vector<MovieItem>& MovieGridModel::movies() const
{
    return m_movies;
}

bool MovieGridModel::updateResumeTicks(const QString& itemId, qint64 resumeTicks)
{
    if (itemId.isEmpty())
        return false;

    bool updated = false;
    for (int row = 0; row < rowCount(); ++row) {
        auto& movie = m_movies[static_cast<size_t>(row)];
        const qint64 normalizedTicks = normalizedResumeTicks(resumeTicks, movie.runtimeTicks);
        if (movie.id != itemId || movie.resumeTicks == normalizedTicks)
            continue;

        movie.resumeTicks = normalizedTicks;
        const QModelIndex changed = index(row, 0);
        emit dataChanged(changed, changed, { ItemRole, ProgressRole, PlayActionLabelRole });
        updated = true;
    }
    return updated;
}

bool MovieGridModel::updateFavorite(const QString& itemId, bool favorite)
{
    if (itemId.isEmpty())
        return false;

    bool updated = false;
    for (int row = 0; row < rowCount(); ++row) {
        auto& movie = m_movies[static_cast<size_t>(row)];
        if (movie.id != itemId || movie.favorite == favorite)
            continue;

        movie.favorite = favorite;
        const QModelIndex changed = index(row, 0);
        emit dataChanged(changed, changed, { ItemRole });
        updated = true;
    }
    return updated;
}

bool MovieGridModel::updatePlayed(const QString& itemId, bool played)
{
    if (itemId.isEmpty())
        return false;

    bool updated = false;
    for (int row = 0; row < rowCount(); ++row) {
        auto& movie = m_movies[static_cast<size_t>(row)];
        const bool clearsResume = !played && movie.resumeTicks != 0;
        if (movie.id != itemId || (movie.played == played && !clearsResume))
            continue;

        movie.played = played;
        movie.resumeTicks = 0;
        const QModelIndex changed = index(row, 0);
        emit dataChanged(changed, changed, { ItemRole, ProgressRole, PlayActionLabelRole });
        updated = true;
    }
    return updated;
}

bool MovieGridModel::removeUnresumable()
{
    bool removed = false;
    for (int row = rowCount() - 1; row >= 0; --row) {
        const MovieItem& movie = m_movies[static_cast<size_t>(row)];
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
