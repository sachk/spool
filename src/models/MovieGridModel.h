#pragma once

#include "../common/JellyfinTypes.h"

#include <QAbstractListModel>

#include <vector>

namespace JellyfinNative {

class MovieGridModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

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
        RuntimeTicksRole,
        ProgressRole,
        SeriesNameRole,
        SeriesPosterUrlRole,
        DisplayTitleRole,
        DisplaySubtitleRole,
        PlayActionLabelRole,
        PlayableRole,
        FavoriteRole,
        PlayedRole,
        BackdropUrlRole,
        LogoUrlRole,
        BannerUrlRole,
        ThumbUrlRole,
        GenresRole,
        TagsRole,
        StudiosRole,
        OfficialRatingRole,
        CommunityRatingRole,
        CriticRatingRole,
        PremiereDateRole,
        EndDateRole,
        PeopleRole,
        MediaSourcesRole,
    };

    explicit MovieGridModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int count() const;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QVariantMap get(int index) const;

    void setMovies(const std::vector<MovieItem> &movies);
    void appendMovies(const std::vector<MovieItem> &movies);
    void clear();
    MovieItem movieAt(int index) const;
    const std::vector<MovieItem> &movies() const;
    bool updateResumeTicks(const QString &itemId, qint64 resumeTicks);
    bool updateFavorite(const QString &itemId, bool favorite);
    bool updatePlayed(const QString &itemId, bool played);

signals:
    void countChanged();

private:
    std::vector<MovieItem> m_movies;
};

} // namespace JellyfinNative
