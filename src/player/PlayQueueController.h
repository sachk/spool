#pragma once

#include "../common/JellyfinTypes.h"

#include <QAbstractListModel>
#include <QVariantMap>

#include <vector>

namespace JellyfinNative {
class JellyfinApiFacade;

class PlayQueueController final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY queueChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(bool shuffled READ shuffled WRITE setShuffled NOTIFY queueChanged)
    Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY currentIndexChanged)
    Q_PROPERTY(bool canGoPrevious READ canGoPrevious NOTIFY currentIndexChanged)

public:
    enum Roles {
        ItemIdRole = Qt::UserRole + 1,
        PlaylistItemIdRole,
        TitleRole,
        DisplayTitleRole,
        DisplaySubtitleRole,
        ItemTypeRole,
        SeriesIdRole,
        SeasonIdRole,
        SeriesNameRole,
        YearRole,
        SeasonNumberRole,
        EpisodeNumberRole,
        EpisodeCodeRole,
        GenericEpisodeTitleRole,
        PlayableRole,
    };

    explicit PlayQueueController(JellyfinApiFacade *api = nullptr, QObject *parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const
    {
        return rowCount();
    }
    int currentIndex() const
    {
        return m_orderIndex < 0 || m_orderIndex >= static_cast<int>(m_order.size())
            ? -1
            : m_order[static_cast<size_t>(m_orderIndex)];
    }
    bool shuffled() const
    {
        return m_shuffled;
    }
    bool canGoNext() const
    {
        return m_orderIndex >= 0 && m_orderIndex + 1 < static_cast<int>(m_order.size());
    }
    bool canGoPrevious() const
    {
        return m_orderIndex > 0;
    }
    MovieItem currentItem() const
    {
        const int index = currentIndex();
        return index < 0 || index >= rowCount() ? MovieItem {} : m_entries[static_cast<size_t>(index)];
    }
    std::vector<PlaybackQueueItem> nowPlayingQueue() const;

    Q_INVOKABLE QVariantMap get(int index) const;
    Q_INVOKABLE bool next();
    Q_INVOKABLE bool previous();
    Q_INVOKABLE bool playAt(int index);
    Q_INVOKABLE void setShuffled(bool shuffled);
    Q_INVOKABLE bool moveItem(int from, int to);
    Q_INVOKABLE bool removeItem(int index);
    Q_INVOKABLE void clear();

    bool playNow(const std::vector<MovieItem>& items, int startIndex);
    bool playNow(const MovieItem& item);
    bool playNext(const MovieItem& item);
    bool addToQueue(const MovieItem& item);
    void enqueueEpisodeSuccessors(const MovieItem& episode);
    bool updateResumeTicks(const QString& itemId, qint64 resumeTicks);

signals:
    void queueChanged();
    void currentIndexChanged();

    void successorPlaybackReady();

private:
    static bool isQueueable(const MovieItem& item);
    void rebuildNaturalOrder();
    void rebuildShuffledOrder(int currentNaturalIndex);
    void setCurrentOrderIndex(int orderIndex);
    void emitQueueStateChanged(int previousCurrentIndex);

    JellyfinApiFacade *m_api = nullptr;
    std::vector<MovieItem> m_entries;
    std::vector<int> m_order;
    int m_orderIndex = -1;
    bool m_shuffled = false;
};

} // namespace JellyfinNative
