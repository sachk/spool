#include "BrowseSessionController.h"

#include "LibraryPrefetchController.h"
#include "LibraryQuery.h"

#include <algorithm>

namespace JellyfinNative {

BrowseSessionController::BrowseSessionController(LibraryPrefetchController *prefetch, QObject *parent)
    : QObject(parent)
    , m_prefetch(prefetch)
{
}

MovieGridModel *BrowseSessionController::items()
{
    return &m_items;
}

QString BrowseSessionController::cacheKey() const
{
    return m_cacheKey;
}

bool BrowseSessionController::loadingMore() const
{
    return m_loadingMore;
}

bool BrowseSessionController::hasMore() const
{
    return m_hasMore;
}

int BrowseSessionController::totalCount() const
{
    return m_totalCount;
}

int BrowseSessionController::nextStartIndex() const
{
    return m_nextStartIndex;
}

int BrowseSessionController::rowCount() const
{
    return m_items.rowCount();
}

QString BrowseSessionController::libraryId() const
{
    return m_libraryId;
}

QString BrowseSessionController::libraryCollectionType() const
{
    return m_libraryCollectionType;
}

QString BrowseSessionController::title() const
{
    return m_title;
}

QString BrowseSessionController::contentLabel() const
{
    return m_contentLabel;
}

QString BrowseSessionController::viewKind() const
{
    return m_viewKind;
}

QString BrowseSessionController::seriesId() const
{
    return m_seriesId;
}

QString BrowseSessionController::seasonId() const
{
    return m_seasonId;
}

QVariantMap BrowseSessionController::query() const
{
    return m_query;
}

QVariantMap BrowseSessionController::filterOptions() const
{
    return m_filterOptions;
}

int BrowseSessionController::filterActiveCount() const
{
    return activeLibraryFilterCount(m_query);
}

BrowseDescriptor BrowseSessionController::descriptor() const
{
    return m_descriptor;
}

MovieItem BrowseSessionController::itemAt(int index) const
{
    return m_items.movieAt(index);
}

int BrowseSessionController::applyCachedPage(const QString& cacheKey)
{
    if (!m_prefetch) {
        m_items.clear();
        return 0;
    }

    const auto prefetchedPage = m_prefetch->cachedPage(cacheKey);
    if (prefetchedPage) {
        m_items.setMovies(prefetchedPage->items);
        m_prefetch->prefetchPosters(prefetchedPage->items);
        return m_items.rowCount();
    }
    m_items.clear();
    return 0;
}

void BrowseSessionController::clear()
{
    m_items.clear();
}

void BrowseSessionController::reset()
{
    clearBrowseIdentity();
    m_libraryQueries.clear();
    m_query.clear();
    m_filterOptions.clear();
    m_items.clear();
    resetPaging();
    emit changed();
}

void BrowseSessionController::resetPaging(const QString& cacheKey)
{
    m_cacheKey = cacheKey;
    m_loadingMore = false;
    m_hasMore = false;
    m_totalCount = 0;
    m_nextStartIndex = 0;
    emit pagingChanged();
}

void BrowseSessionController::setPage(const PagedMovieItems& page, const QString& cacheKey, bool append)
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
    m_totalCount = hasServerTotal ? std::max(page.totalRecordCount, m_nextStartIndex) : m_nextStartIndex;
    m_hasMore = hasServerTotal ? m_nextStartIndex < m_totalCount
                               : page.items.size() >= static_cast<size_t>(std::max(1, page.limit));
    if (page.items.empty())
        m_hasMore = false;
    m_loadingMore = false;

    if (m_prefetch)
        m_prefetch->prefetchPosters(page.items);
    emit pagingChanged();
}

void BrowseSessionController::setLoadingMore(bool loading)
{
    if (m_loadingMore == loading)
        return;
    m_loadingMore = loading;
    emit pagingChanged();
}

void BrowseSessionController::setWarmCachePaging(int cachedCount, int pageSize)
{
    m_nextStartIndex = cachedCount;
    m_totalCount = cachedCount;
    m_hasMore = cachedCount >= pageSize;
    m_loadingMore = true;
    emit pagingChanged();
}

void BrowseSessionController::prefetchVisibleRange(int firstIndex, int lastIndex)
{
    if (!m_prefetch || firstIndex < 0 || lastIndex < firstIndex || m_items.rowCount() <= 0)
        return;
    m_prefetch->prefetchPosters(m_items.movies(), firstIndex, lastIndex - firstIndex + 1);
}

void BrowseSessionController::updateResumeTicks(const QString& itemId, qint64 positionTicks)
{
    m_items.updateResumeTicks(itemId, positionTicks);
}

void BrowseSessionController::updateFavorite(const QString& itemId, bool favorite)
{
    m_items.updateFavorite(itemId, favorite);
}

void BrowseSessionController::updatePlayed(const QString& itemId, bool played)
{
    m_items.updatePlayed(itemId, played);
}

void BrowseSessionController::enterLibrary(
    const LibraryItem& library, const QString& contentLabel, const QVariantMap& defaultQuery)
{
    m_libraryId = library.id;
    m_libraryCollectionType = library.collectionType;
    m_title = library.name;
    m_contentLabel = contentLabel;
    m_viewKind = QStringLiteral("library");
    m_seriesId.clear();
    m_seasonId.clear();
    m_descriptor = BrowseDescriptor::library(library.id, library.collectionType, library.name);
    m_query = m_libraryQueries.value(library.id, defaultQuery);
    m_filterOptions.clear();
    emit changed();
}

void BrowseSessionController::enterSeries(const MovieItem& series)
{
    m_libraryId.clear();
    m_libraryCollectionType.clear();
    m_seriesId = series.id;
    m_seasonId.clear();
    m_descriptor = BrowseDescriptor::seriesSeasons(series.id, series.title);
    m_viewKind = QStringLiteral("seasons");
    m_title = series.title;
    m_contentLabel = QStringLiteral("Seasons");
    m_query.clear();
    m_filterOptions.clear();
    emit changed();
}

void BrowseSessionController::enterSeason(const QString& seriesId, const MovieItem& season)
{
    m_libraryId.clear();
    m_libraryCollectionType.clear();
    m_seriesId = seriesId;
    m_seasonId = season.itemType == QStringLiteral("Season") ? season.id : QString();
    m_descriptor = BrowseDescriptor::seasonEpisodes(m_seriesId, m_seasonId, season.title);
    m_viewKind = QStringLiteral("episodes");
    m_title = season.title;
    m_contentLabel = QStringLiteral("Episodes");
    m_query.clear();
    m_filterOptions.clear();
    emit changed();
}

void BrowseSessionController::enterNamedCollection(const QString& viewKind, const QString& name)
{
    clearBrowseIdentity();
    if (viewKind == QStringLiteral("genre"))
        m_descriptor = BrowseDescriptor::genre(name);
    else if (viewKind == QStringLiteral("studio"))
        m_descriptor = BrowseDescriptor::studio(name);
    else
        m_descriptor = {};
    m_viewKind = viewKind;
    m_title = name;
    m_contentLabel = QStringLiteral("Titles");
    m_query.clear();
    m_filterOptions.clear();
    emit changed();
}

void BrowseSessionController::enterPlaylist(const MovieItem& playlist)
{
    clearBrowseIdentity();
    m_descriptor = BrowseDescriptor::playlist(playlist.id, playlist.title);
    m_viewKind = QStringLiteral("playlist");
    m_title = playlist.title;
    m_contentLabel = QStringLiteral("Items");
    m_query.clear();
    m_filterOptions.clear();
    emit changed();
}

void BrowseSessionController::enterBoxSet(const MovieItem& boxSet)
{
    clearBrowseIdentity();
    m_descriptor = BrowseDescriptor::boxSet(boxSet.id, boxSet.title);
    m_viewKind = QStringLiteral("boxset");
    m_title = boxSet.title;
    m_contentLabel = QStringLiteral("Titles");
    m_query.clear();
    m_filterOptions.clear();
    emit changed();
}

void BrowseSessionController::enterFolder(const MovieItem& folder)
{
    clearBrowseIdentity();
    m_descriptor = BrowseDescriptor::folderChildren(folder.id, folder.title);
    m_viewKind = QStringLiteral("folder");
    m_title = folder.title;
    m_contentLabel = QStringLiteral("Items");
    m_query.clear();
    m_filterOptions.clear();
    emit changed();
}

bool BrowseSessionController::setQuery(const QVariantMap& query)
{
    if (m_query == query)
        return false;
    m_query = query;
    if (!m_libraryId.isEmpty())
        m_libraryQueries.insert(m_libraryId, m_query);
    emit changed();
    return true;
}

void BrowseSessionController::clearFilterOptions()
{
    m_filterOptions.clear();
    emit changed();
}

void BrowseSessionController::setFilterOptions(const QVariantMap& options)
{
    m_filterOptions = options;
    emit changed();
}

void BrowseSessionController::clearBrowseIdentity()
{
    m_libraryId.clear();
    m_libraryCollectionType.clear();
    m_seriesId.clear();
    m_seasonId.clear();
    m_descriptor = {};
}

} // namespace JellyfinNative
