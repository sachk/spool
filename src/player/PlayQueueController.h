#pragma once

#include "../common/JellyfinTypes.h"

#include <QAbstractListModel>
#include <QVariantMap>

#include <vector>

namespace JellyfinNative {

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
        PlayableRole,
        PosterUrlRole,
        LandscapeCardUrlRole,
    };

    explicit PlayQueueController(QObject *parent = nullptr)
        : QAbstractListModel(parent)
    {
    }

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    int currentIndex() const;
    bool shuffled() const;
    bool canGoNext() const;
    bool canGoPrevious() const;
    MovieItem currentItem() const;
    std::vector<PlaybackQueueItem> nowPlayingQueue() const;

    Q_INVOKABLE QVariantMap get(int index) const;
    Q_INVOKABLE bool next();
    Q_INVOKABLE bool previous();
    Q_INVOKABLE bool playAt(int index);
    Q_INVOKABLE void setShuffled(bool shuffled);
    Q_INVOKABLE void clear();

    bool playNow(const std::vector<MovieItem>& items, int startIndex);
    bool playNow(const MovieItem& item);
    bool playNext(const MovieItem& item);
    bool addToQueue(const MovieItem& item);

signals:
    void queueChanged();
    void currentIndexChanged();

private:
    static bool isQueueable(const MovieItem& item);
    void rebuildNaturalOrder();
    void rebuildShuffledOrder(int currentNaturalIndex);
    void setCurrentOrderIndex(int orderIndex);
    void emitQueueStateChanged(int previousCurrentIndex);

    std::vector<MovieItem> m_entries;
    std::vector<int> m_order;
    int m_orderIndex = -1;
    bool m_shuffled = false;
};

} // namespace JellyfinNative
