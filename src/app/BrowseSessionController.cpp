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

int BrowseSessionController::filterActiveCount() const
{
    return activeLibraryFilterCount(m_query);
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
    if (firstIndex < 0 || lastIndex < firstIndex || m_items.rowCount() <= 0)
        return;
    if (m_prefetch)
        m_prefetch->prefetchPosters(m_items.movies(), firstIndex, lastIndex - firstIndex + 1);
    if (lastIndex + 200 >= m_items.rowCount())
        emit moreItemsRequested();
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

bool BrowseSessionController::enterItem(const MovieItem& item)
{
    BrowseDescriptor descriptor;
    QString viewKind;
    QString contentLabel;
    if (item.itemType == QStringLiteral("Series")) {
        descriptor = BrowseDescriptor::seriesSeasons(item.id, item.title);
        viewKind = QStringLiteral("seasons");
        contentLabel = QStringLiteral("Seasons");
    } else if (item.itemType == QStringLiteral("Playlist")) {
        descriptor = BrowseDescriptor::playlist(item.id, item.title);
        viewKind = QStringLiteral("playlist");
        contentLabel = QStringLiteral("Items");
    } else if (item.itemType == QStringLiteral("BoxSet")) {
        descriptor = BrowseDescriptor::boxSet(item.id, item.title);
        viewKind = QStringLiteral("boxset");
        contentLabel = QStringLiteral("Titles");
    } else if (item.itemType == QStringLiteral("Folder")) {
        descriptor = BrowseDescriptor::folderChildren(item.id, item.title);
        viewKind = QStringLiteral("folder");
        contentLabel = QStringLiteral("Items");
    } else {
        return false;
    }
    clearBrowseIdentity();
    if (item.itemType == QStringLiteral("Series"))
        m_seriesId = item.id;
    m_descriptor = std::move(descriptor);
    m_viewKind = std::move(viewKind);
    m_title = item.title;
    m_contentLabel = std::move(contentLabel);
    m_query.clear();
    m_filterOptions.clear();
    emit changed();
    return true;
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

void BrowseSessionController::setSort(const QString& sortBy, const QString& sortOrder)
{
    QVariantMap next = m_query;
    next.insert(QStringLiteral("sortBy"), sortBy.isEmpty() ? QStringLiteral("SortName") : sortBy);
    next.insert(QStringLiteral("sortOrder"),
        sortOrder == QStringLiteral("Descending") ? QStringLiteral("Descending") : QStringLiteral("Ascending"));
    requestQuery(std::move(next));
}

void BrowseSessionController::setQueryListValue(const QString& key, const QString& value, bool enabled)
{
    if (key.isEmpty() || value.isEmpty())
        return;
    QVariantMap next = m_query;
    QStringList values = libraryQueryStringList(next, key);
    values.removeAll(value);
    if (enabled)
        values.push_back(value);
    values.removeDuplicates();
    if (values.isEmpty())
        next.remove(key);
    else
        next.insert(key, values);
    requestQuery(std::move(next));
}

void BrowseSessionController::setQueryValue(const QString& key, const QVariant& value)
{
    if (key.isEmpty())
        return;
    QVariantMap next = m_query;
    if (!value.isValid() || value.isNull())
        next.remove(key);
    else
        next.insert(key, value);
    requestQuery(std::move(next));
}

void BrowseSessionController::clearFilters()
{
    if (m_libraryId.isEmpty())
        return;
    QVariantMap next;
    next.insert(
        QStringLiteral("sortBy"), m_query.value(QStringLiteral("sortBy"), QStringLiteral("SortName")).toString());
    next.insert(QStringLiteral("sortOrder"),
        m_query.value(QStringLiteral("sortOrder"), QStringLiteral("Ascending")).toString());
    requestQuery(std::move(next));
}

bool BrowseSessionController::setQuery(QVariantMap query)
{
    if (m_query == query)
        return false;
    m_query = std::move(query);
    if (!m_libraryId.isEmpty())
        m_libraryQueries.insert(m_libraryId, m_query);
    emit changed();
    return true;
}

void BrowseSessionController::requestQuery(QVariantMap query)
{
    if (setQuery(std::move(query)))
        emit reloadRequested();
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
