#include "CurrentItemsController.h"

#include "LibraryPrefetchController.h"

#include <algorithm>

namespace JellyfinNative {

CurrentItemsController::CurrentItemsController(
    LibraryPrefetchController *prefetch, QObject *parent)
    : QObject(parent)
    , m_prefetch(prefetch)
{
}

MovieGridModel *CurrentItemsController::items()
{
    return &m_items;
}

QString CurrentItemsController::cacheKey() const
{
    return m_cacheKey;
}

bool CurrentItemsController::loadingMore() const
{
    return m_loadingMore;
}

bool CurrentItemsController::hasMore() const
{
    return m_hasMore;
}

int CurrentItemsController::totalCount() const
{
    return m_totalCount;
}

int CurrentItemsController::nextStartIndex() const
{
    return m_nextStartIndex;
}

int CurrentItemsController::rowCount() const
{
    return m_items.rowCount();
}

MovieItem CurrentItemsController::itemAt(int index) const
{
    return m_items.movieAt(index);
}

int CurrentItemsController::applyCachedPage(const QString &cacheKey)
{
    const auto prefetchedPage = m_prefetch->cachedPage(cacheKey);
    if (prefetchedPage) {
        m_items.setMovies(prefetchedPage->items);
        m_prefetch->prefetchPosters(prefetchedPage->items);
        return m_items.rowCount();
    }
    m_items.clear();
    return 0;
}

void CurrentItemsController::clear()
{
    m_items.clear();
}

void CurrentItemsController::resetPaging(const QString &cacheKey)
{
    m_cacheKey = cacheKey;
    m_loadingMore = false;
    m_hasMore = false;
    m_totalCount = 0;
    m_nextStartIndex = 0;
    emit pagingChanged();
}

void CurrentItemsController::setItems(const std::vector<MovieItem> &items,
                                      const QString &cacheKey)
{
    resetPaging(cacheKey);
    m_items.setMovies(items);
    m_prefetch->prefetchPosters(items);
}

void CurrentItemsController::setPage(const PagedMovieItems &page,
                                     const QString &cacheKey, bool append)
{
    if (append)
        m_items.appendMovies(page.items);
    else
        m_items.setMovies(page.items);

    const int loadedCount = m_items.rowCount();
    const int pageEnd = page.startIndex + static_cast<int>(page.items.size());
    const bool hasServerTotal = page.totalRecordCount > 0;
    m_cacheKey = cacheKey;
    m_nextStartIndex = std::max(loadedCount, pageEnd);
    m_totalCount = hasServerTotal ? std::max(page.totalRecordCount, m_nextStartIndex)
                                  : m_nextStartIndex;
    m_hasMore = hasServerTotal
                    ? m_nextStartIndex < m_totalCount
                    : page.items.size() >= static_cast<size_t>(std::max(1, page.limit));
    if (page.items.empty())
        m_hasMore = false;
    m_loadingMore = false;

    m_prefetch->prefetchPosters(page.items);
    emit pagingChanged();
}

void CurrentItemsController::setLoadingMore(bool loading)
{
    if (m_loadingMore == loading)
        return;
    m_loadingMore = loading;
    emit pagingChanged();
}

void CurrentItemsController::setWarmCachePaging(int cachedCount, int pageSize)
{
    m_nextStartIndex = cachedCount;
    m_totalCount = cachedCount;
    m_hasMore = cachedCount >= pageSize;
    m_loadingMore = true;
    emit pagingChanged();
}

void CurrentItemsController::prefetchVisibleRange(int firstIndex, int lastIndex)
{
    if (firstIndex < 0 || lastIndex < firstIndex || m_items.rowCount() <= 0)
        return;
    m_prefetch->prefetchPosters(
        m_items.movies(), firstIndex, lastIndex - firstIndex + 1);
}

void CurrentItemsController::updateResumeTicks(const QString &itemId,
                                               qint64 positionTicks)
{
    m_items.updateResumeTicks(itemId, positionTicks);
}

void CurrentItemsController::updateFavorite(const QString &itemId, bool favorite)
{
    m_items.updateFavorite(itemId, favorite);
}

void CurrentItemsController::updatePlayed(const QString &itemId, bool played)
{
    m_items.updatePlayed(itemId, played);
}

} // namespace JellyfinNative
