#include "PlayQueueController.h"

#include "../api/JellyfinApiFacade.h"
#include "../common/AsyncTask.h"

#include <QDebug>
#include <algorithm>
#include <numeric>

namespace JellyfinNative {

namespace {

    QString displayTitle(const MovieItem& item)
    {
        if (item.itemType == QStringLiteral("Episode") && !item.seriesName.isEmpty())
            return item.seriesName;
        return item.title;
    }

    QVariantMap itemSnapshot(const MovieItem& item)
    {
        return {
            { QStringLiteral("movieId"), item.id },
            { QStringLiteral("playlistItemId"), item.playlistItemId },
            { QStringLiteral("title"), item.title },
            { QStringLiteral("displayTitle"), displayTitle(item) },
            { QStringLiteral("displaySubtitle"), itemDisplaySubtitle(item) },
            { QStringLiteral("itemType"), item.itemType },
            { QStringLiteral("playable"), isPlayableItem(item) },
        };
    }

} // namespace

PlayQueueController::PlayQueueController(JellyfinApiFacade *api, QObject *parent)
    : QAbstractListModel(parent)
    , m_api(api)
{
}

int PlayQueueController::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_entries.size());
}

QVariant PlayQueueController::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};

    const MovieItem& item = m_entries[static_cast<size_t>(index.row())];
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
        return itemDisplaySubtitle(item);
    case ItemTypeRole:
        return item.itemType;
    case PlayableRole:
        return isPlayableItem(item);
    default:
        return {};
    }
}

QHash<int, QByteArray> PlayQueueController::roleNames() const
{
    return {
        { ItemIdRole, "movieId" },
        { PlaylistItemIdRole, "playlistItemId" },
        { TitleRole, "title" },
        { DisplayTitleRole, "displayTitle" },
        { DisplaySubtitleRole, "displaySubtitle" },
        { ItemTypeRole, "itemType" },
        { PlayableRole, "playable" },
    };
}

std::vector<PlaybackQueueItem> PlayQueueController::nowPlayingQueue() const
{
    std::vector<PlaybackQueueItem> queue;
    queue.reserve(m_order.size());
    for (int naturalIndex : m_order) {
        if (naturalIndex < 0 || naturalIndex >= rowCount())
            continue;
        const MovieItem& item = m_entries[static_cast<size_t>(naturalIndex)];
        queue.push_back({ item.id, item.playlistItemId });
    }
    return queue;
}

QVariantMap PlayQueueController::get(int index) const
{
    if (index < 0 || index >= rowCount())
        return {};
    return itemSnapshot(m_entries[static_cast<size_t>(index)]);
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

bool PlayQueueController::playNow(const std::vector<MovieItem>& items, int startIndex)
{
    if (startIndex < 0 || startIndex >= static_cast<int>(items.size())
        || !isQueueable(items[static_cast<size_t>(startIndex)])) {
        return false;
    }

    std::vector<MovieItem> nextEntries;
    nextEntries.reserve(items.size());
    int nextCurrent = -1;
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        if (!isQueueable(items[static_cast<size_t>(i)]))
            continue;
        if (i == startIndex)
            nextCurrent = static_cast<int>(nextEntries.size());
        nextEntries.push_back(items[static_cast<size_t>(i)]);
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

bool PlayQueueController::playNow(const MovieItem& item)
{
    return playNow(std::vector<MovieItem> { item }, 0);
}

bool PlayQueueController::playNext(const MovieItem& item)
{
    if (!isQueueable(item))
        return false;

    if (m_entries.empty())
        return playNow(item);

    const int previousCurrent = currentIndex();
    const int naturalIndex = rowCount();
    beginInsertRows({}, naturalIndex, naturalIndex);
    m_entries.push_back(item);
    endInsertRows();

    const int insertOrderIndex = m_orderIndex >= 0 ? m_orderIndex + 1 : 0;
    m_order.insert(m_order.begin() + insertOrderIndex, naturalIndex);
    if (m_orderIndex < 0)
        m_orderIndex = insertOrderIndex;
    emitQueueStateChanged(previousCurrent);
    return true;
}

bool PlayQueueController::addToQueue(const MovieItem& item)
{
    if (!isQueueable(item))
        return false;

    if (m_entries.empty())
        return playNow(item);

    const int previousCurrent = currentIndex();
    const int naturalIndex = rowCount();
    beginInsertRows({}, naturalIndex, naturalIndex);
    m_entries.push_back(item);
    endInsertRows();
    m_order.push_back(naturalIndex);
    emitQueueStateChanged(previousCurrent);
    return true;
}

void PlayQueueController::enqueueEpisodeSuccessors(const MovieItem& episode)
{
    if (!m_api || episode.itemType != QStringLiteral("Episode") || episode.seriesId.isEmpty()
        || m_api->session().accessToken.isEmpty()) {
        return;
    }
    Async::runScoped(
        this, m_api->fetchEpisodes(episode.seriesId),
        [this, episode](const std::vector<MovieItem>& episodes) {
            auto current = std::find_if(episodes.begin(), episodes.end(),
                [&episode](const MovieItem& candidate) { return candidate.id == episode.id; });
            if (current == episodes.end())
                return;
            std::vector<MovieItem> successors;
            std::copy_if(++current, episodes.end(), std::back_inserter(successors),
                [](const MovieItem& item) { return !item.id.isEmpty() && isPlayableItem(item); });
            if (playNow(successors, 0))
                emit successorPlaybackReady();
        },
        [](const std::exception_ptr& error) {
            qWarning() << "play queue: episode successor lookup failed" << exceptionMessage(error);
        });
}

bool PlayQueueController::isQueueable(const MovieItem& item)
{
    return !item.id.isEmpty() && isPlayableItem(item);
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
    if (orderIndex < 0 || orderIndex >= static_cast<int>(m_order.size()) || m_orderIndex == orderIndex) {
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
