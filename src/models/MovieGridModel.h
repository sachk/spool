#pragma once

#include "../common/JellyfinTypes.h"

#include <QAbstractListModel>

#include <vector>

namespace JellyfinNative {

class MovieGridModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        OverviewRole,
        PosterUrlRole,
        PosterTagRole,
        YearRole,
        ItemTypeRole,
        SubtitleRole,
        PathRole,
        SeasonNumberRole,
        EpisodeNumberRole,
        ResumeTicksRole,
        PlayActionLabelRole,
        PlayableRole,
    };

    explicit MovieGridModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QVariantMap get(int index) const;

    void setMovies(const std::vector<MovieItem> &movies);
    void clear();
    MovieItem movieAt(int index) const;

private:
    std::vector<MovieItem> m_movies;
};

} // namespace JellyfinNative
