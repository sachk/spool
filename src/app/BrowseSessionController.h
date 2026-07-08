#pragma once

#include "../common/JellyfinTypes.h"
#include "../models/MovieGridModel.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include <vector>

namespace JellyfinNative {

class LibraryPrefetchController;

class BrowseSessionController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(JellyfinNative::MovieGridModel *items READ items CONSTANT)
    Q_PROPERTY(bool loadingMore READ loadingMore NOTIFY pagingChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY pagingChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY pagingChanged)
    Q_PROPERTY(QString libraryId READ libraryId NOTIFY changed)
    Q_PROPERTY(QString libraryCollectionType READ libraryCollectionType NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString contentLabel READ contentLabel NOTIFY changed)
    Q_PROPERTY(QString viewKind READ viewKind NOTIFY changed)
    Q_PROPERTY(QVariantMap query READ query NOTIFY changed)
    Q_PROPERTY(QVariantMap filterOptions READ filterOptions NOTIFY changed)
    Q_PROPERTY(int filterActiveCount READ filterActiveCount NOTIFY changed)

public:
    explicit BrowseSessionController(LibraryPrefetchController *prefetch, QObject *parent = nullptr);

    MovieGridModel *items();

    QString cacheKey() const;
    bool loadingMore() const;
    bool hasMore() const;
    int totalCount() const;
    int nextStartIndex() const;
    int rowCount() const;

    QString libraryId() const;
    QString libraryCollectionType() const;
    QString title() const;
    QString contentLabel() const;
    QString viewKind() const;
    QString seriesId() const;
    QString seasonId() const;
    QVariantMap query() const;
    QVariantMap filterOptions() const;
    int filterActiveCount() const;
    BrowseDescriptor descriptor() const;

    MovieItem itemAt(int index) const;
    int applyCachedPage(const QString& cacheKey);
    void clear();
    void reset();
    void resetPaging(const QString& cacheKey = {});
    void setItems(const std::vector<MovieItem>& items, const QString& cacheKey = {});
    void setPage(const PagedMovieItems& page, const QString& cacheKey, bool append);
    void setLoadingMore(bool loading);
    void setWarmCachePaging(int cachedCount, int pageSize);
    void prefetchVisibleRange(int firstIndex, int lastIndex);
    void updateResumeTicks(const QString& itemId, qint64 positionTicks);
    void updateFavorite(const QString& itemId, bool favorite);
    void updatePlayed(const QString& itemId, bool played);

    void enterLibrary(const LibraryItem& library, const QString& contentLabel, const QVariantMap& defaultQuery);
    void enterSeries(const MovieItem& series);
    void enterSeason(const QString& seriesId, const MovieItem& season);
    void enterNamedCollection(const QString& viewKind, const QString& name);
    void enterPlaylist(const MovieItem& playlist);
    void enterBoxSet(const MovieItem& boxSet);
    void enterFolder(const MovieItem& folder);
    bool setQuery(const QVariantMap& query);
    void clearFilterOptions();
    void setFilterOptions(const QVariantMap& options);

signals:
    void pagingChanged();
    void changed();

private:
    void clearBrowseIdentity();

    LibraryPrefetchController *m_prefetch = nullptr;
    MovieGridModel m_items;
    QString m_cacheKey;
    bool m_loadingMore = false;
    bool m_hasMore = false;
    int m_totalCount = 0;
    int m_nextStartIndex = 0;

    QString m_libraryId;
    QString m_libraryCollectionType;
    QString m_title;
    QString m_contentLabel = QStringLiteral("Movies");
    QString m_viewKind;
    QString m_seriesId;
    QString m_seasonId;
    BrowseDescriptor m_descriptor;
    QHash<QString, QVariantMap> m_libraryQueries;
    QVariantMap m_query;
    QVariantMap m_filterOptions;
};

} // namespace JellyfinNative
