#pragma once

#include "../common/JellyfinTypes.h"
#include "../models/MovieGridModel.h"

#include <QObject>
#include <QString>

#include <vector>

namespace JellyfinNative {

class LibraryPrefetchController;

class CurrentItemsController final : public QObject
{
    Q_OBJECT

public:
    explicit CurrentItemsController(LibraryPrefetchController *prefetch,
                                    QObject *parent = nullptr);

    MovieGridModel *items();

    QString cacheKey() const;
    bool loadingMore() const;
    bool hasMore() const;
    int totalCount() const;
    int nextStartIndex() const;
    int rowCount() const;

    MovieItem itemAt(int index) const;
    int applyCachedPage(const QString &cacheKey);
    void clear();
    void resetPaging(const QString &cacheKey = {});
    void setItems(const std::vector<MovieItem> &items,
                  const QString &cacheKey = {});
    void setPage(const PagedMovieItems &page, const QString &cacheKey,
                 bool append);
    void setLoadingMore(bool loading);
    void setWarmCachePaging(int cachedCount, int pageSize);
    void prefetchVisibleRange(int firstIndex, int lastIndex);
    void updateResumeTicks(const QString &itemId, qint64 positionTicks);
    void updateFavorite(const QString &itemId, bool favorite);
    void updatePlayed(const QString &itemId, bool played);

signals:
    void pagingChanged();

private:
    LibraryPrefetchController *m_prefetch = nullptr;
    MovieGridModel m_items;
    QString m_cacheKey;
    bool m_loadingMore = false;
    bool m_hasMore = false;
    int m_totalCount = 0;
    int m_nextStartIndex = 0;
};

} // namespace JellyfinNative
