#include "PlayQueueController.h"

#include <algorithm>
#include <numeric>

namespace JellyfinNative {

namespace {

QString displayTitle(const MovieItem &item)
{
    if (item.itemType == QStringLiteral("Episode") && !item.seriesName.isEmpty())
        return item.seriesName;
    return item.title;
}

QString displaySubtitle(const MovieItem &item)
{
    if (item.itemType == QStringLiteral("Episode")) {
        if (!item.subtitle.isEmpty() && !item.title.isEmpty())
            return QStringLiteral("%1 · %2").arg(item.subtitle, item.title);
        if (!item.title.isEmpty())
            return item.title;
    }
    return item.subtitle;
}

QVariantMap itemSnapshot(const MovieItem &item)
{
    return {
        {QStringLiteral("movieId"), item.id},
        {QStringLiteral("playlistItemId"), item.playlistItemId},
        {QStringLiteral("title"), item.title},
        {QStringLiteral("displayTitle"), displayTitle(item)},
        {QStringLiteral("displaySubtitle"), displaySubtitle(item)},
        {QStringLiteral("itemType"), item.itemType},
        {QStringLiteral("playable"), item.playable},
        {QStringLiteral("posterUrl"), item.posterUrl},
        {QStringLiteral("landscapeCardUrl"), item.landscapeCardUrl},
    };
}

} // namespace

PlayQueueController::PlayQueueController(QObject *parent)
    : QAbstractListModel(parent)
{
}

int PlayQueueController::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_entries.size());
}

QVariant PlayQueueController::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};

    const MovieItem &item = m_entries[static_cast<size_t>(index.row())].item;
    switch (role) {
    case ItemIdRole:
        return item.id;
    case PlaylistItemIdRole:
        return item.playlistItemId;
    case TitleRole:
        return item.title;
    case DisplayTitleRole:
        return displayTitle(item);
    case DisplaySubtitleRole:
        return displaySubtitle(item);
    case ItemTypeRole:
        return item.itemType;
    case PlayableRole:
        return item.playable;
    case PosterUrlRole:
        return item.posterUrl;
    case LandscapeCardUrlRole:
        return item.landscapeCardUrl;
    default:
        return {};
    }
}

QHash<int, QByteArray> PlayQueueController::roleNames() const
{
    return {
        {ItemIdRole, "movieId"},
        {PlaylistItemIdRole, "playlistItemId"},
        {TitleRole, "title"},
        {DisplayTitleRole, "displayTitle"},
        {DisplaySubtitleRole, "displaySubtitle"},
        {ItemTypeRole, "itemType"},
        {PlayableRole, "playable"},
        {PosterUrlRole, "posterUrl"},
        {LandscapeCardUrlRole, "landscapeCardUrl"},
    };
}

int PlayQueueController::count() const
{
    return rowCount();
}

int PlayQueueController::currentIndex() const
{
    if (m_orderIndex < 0 || m_orderIndex >= static_cast<int>(m_order.size()))
        return -1;
    return m_order[static_cast<size_t>(m_orderIndex)];
}

bool PlayQueueController::shuffled() const
{
    return m_shuffled;
}

bool PlayQueueController::canGoNext() const
{
    return m_orderIndex >= 0 && m_orderIndex + 1 < static_cast<int>(m_order.size());
}

bool PlayQueueController::canGoPrevious() const
{
    return m_orderIndex > 0;
}

MovieItem PlayQueueController::currentItem() const
{
    const int index = currentIndex();
    if (index < 0 || index >= rowCount())
        return {};
    return m_entries[static_cast<size_t>(index)].item;
}

std::vector<PlaybackQueueItem> PlayQueueController::nowPlayingQueue() const
{
    std::vector<PlaybackQueueItem> queue;
    queue.reserve(m_order.size());
    for (int naturalIndex : m_order) {
        if (naturalIndex < 0 || naturalIndex >= rowCount())
            continue;
        const MovieItem &item = m_entries[static_cast<size_t>(naturalIndex)].item;
        queue.push_back({item.id, item.playlistItemId});
    }
    return queue;
}

QVariantMap PlayQueueController::get(int index) const
{
    if (index < 0 || index >= rowCount())
        return {};
    return itemSnapshot(m_entries[static_cast<size_t>(index)].item);
}

bool PlayQueueController::next()
{
    if (!canGoNext())
        return false;
    setCurrentOrderIndex(m_orderIndex + 1);
    return true;
}

bool PlayQueueController::previous()
{
    if (!canGoPrevious())
        return false;
    setCurrentOrderIndex(m_orderIndex - 1);
    return true;
}

bool PlayQueueController::playAt(int index)
{
    if (index < 0 || index >= rowCount())
        return false;
    const auto current = std::find(m_order.begin(), m_order.end(), index);
    if (current == m_order.end())
        return false;
    setCurrentOrderIndex(static_cast<int>(std::distance(m_order.begin(), current)));
    return true;
}

void PlayQueueController::setShuffled(bool shuffled)
{
    if (m_shuffled == shuffled)
        return;

    const int previousCurrent = currentIndex();
    m_shuffled = shuffled;
    if (m_entries.empty()) {
        emit queueChanged();
        return;
    }

    if (m_shuffled)
        rebuildShuffledOrder(previousCurrent);
    else {
        rebuildNaturalOrder();
        m_orderIndex = previousCurrent >= 0 ? previousCurrent : 0;
    }
    emitQueueStateChanged(previousCurrent);
}

void PlayQueueController::clear()
{
    if (m_entries.empty())
        return;
    const int previousCurrent = currentIndex();
    beginResetModel();
    m_entries.clear();
    m_order.clear();
    m_orderIndex = -1;
    m_shuffled = false;
    endResetModel();
    emitQueueStateChanged(previousCurrent);
}

void PlayQueueController::removeAt(int index)
{
    if (index < 0 || index >= rowCount())
        return;

    const int previousCurrent = currentIndex();
    beginRemoveRows({}, index, index);
    m_entries.erase(m_entries.begin() + index);
    endRemoveRows();

    m_order.erase(std::remove(m_order.begin(), m_order.end(), index), m_order.end());
    for (int &naturalIndex : m_order) {
        if (naturalIndex > index)
            --naturalIndex;
    }

    if (m_order.empty()) {
        m_orderIndex = -1;
    } else if (m_orderIndex >= static_cast<int>(m_order.size())) {
        m_orderIndex = static_cast<int>(m_order.size()) - 1;
    }
    emitQueueStateChanged(previousCurrent);
}

bool PlayQueueController::playNow(const std::vector<MovieItem> &items,
                                  int startIndex)
{
    if (startIndex < 0 || startIndex >= static_cast<int>(items.size()) ||
        !isQueueable(items[static_cast<size_t>(startIndex)])) {
        return false;
    }

    std::vector<Entry> nextEntries;
    nextEntries.reserve(items.size());
    int nextCurrent = -1;
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        if (!isQueueable(items[static_cast<size_t>(i)]))
            continue;
        if (i == startIndex)
            nextCurrent = static_cast<int>(nextEntries.size());
        nextEntries.push_back({items[static_cast<size_t>(i)]});
    }
    if (nextCurrent < 0)
        return false;

    const int previousCurrent = currentIndex();
    beginResetModel();
    m_entries = std::move(nextEntries);
    rebuildNaturalOrder();
    m_orderIndex = nextCurrent;
    if (m_shuffled)
        rebuildShuffledOrder(nextCurrent);
    endResetModel();
    emitQueueStateChanged(previousCurrent);
    return true;
}

bool PlayQueueController::playNow(const MovieItem &item)
{
    return playNow(std::vector<MovieItem>{item}, 0);
}

bool PlayQueueController::playNext(const MovieItem &item)
{
    if (!isQueueable(item))
        return false;

    if (m_entries.empty())
        return playNow(item);

    const int previousCurrent = currentIndex();
    const int naturalIndex = rowCount();
    beginInsertRows({}, naturalIndex, naturalIndex);
    m_entries.push_back({item});
    endInsertRows();

    const int insertOrderIndex = m_orderIndex >= 0 ? m_orderIndex + 1 : 0;
    m_order.insert(m_order.begin() + insertOrderIndex, naturalIndex);
    if (m_orderIndex < 0)
        m_orderIndex = insertOrderIndex;
    emitQueueStateChanged(previousCurrent);
    return true;
}

bool PlayQueueController::addToQueue(const MovieItem &item)
{
    if (!isQueueable(item))
        return false;

    if (m_entries.empty())
        return playNow(item);

    const int previousCurrent = currentIndex();
    const int naturalIndex = rowCount();
    beginInsertRows({}, naturalIndex, naturalIndex);
    m_entries.push_back({item});
    endInsertRows();
    m_order.push_back(naturalIndex);
    emitQueueStateChanged(previousCurrent);
    return true;
}

bool PlayQueueController::isQueueable(const MovieItem &item)
{
    return !item.id.isEmpty() && item.playable;
}

void PlayQueueController::rebuildNaturalOrder()
{
    m_order.resize(m_entries.size());
    std::iota(m_order.begin(), m_order.end(), 0);
}

void PlayQueueController::rebuildShuffledOrder(int currentNaturalIndex)
{
    rebuildNaturalOrder();
    if (currentNaturalIndex < 0 || currentNaturalIndex >= rowCount()) {
        m_orderIndex = m_order.empty() ? -1 : 0;
        return;
    }

    auto current = std::find(m_order.begin(), m_order.end(), currentNaturalIndex);
    if (current != m_order.end())
        m_order.erase(current);
    std::reverse(m_order.begin(), m_order.end());
    m_order.insert(m_order.begin(), currentNaturalIndex);
    m_orderIndex = 0;
}


void PlayQueueController::setCurrentOrderIndex(int orderIndex)
{
    if (orderIndex < 0 || orderIndex >= static_cast<int>(m_order.size()) ||
        m_orderIndex == orderIndex) {
        return;
    }
    const int previousCurrent = currentIndex();
    m_orderIndex = orderIndex;
    emitQueueStateChanged(previousCurrent);
}

void PlayQueueController::emitQueueStateChanged(int previousCurrentIndex)
{
    emit queueChanged();
    if (previousCurrentIndex != currentIndex())
        emit currentIndexChanged();
}

} // namespace JellyfinNative
