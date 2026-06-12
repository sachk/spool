#pragma once

#include "../common/JellyfinTypes.h"
#include "../common/RequestGeneration.h"
#include "../models/MovieGridModel.h"

#include <QObject>
#include <QStringList>
#include <QVariantList>

#include <memory>
#include <vector>

namespace JellyfinNative {

class JellyfinApiFacade;
class LibraryPrefetchController;

class HomeModelController final : public QObject
{
    Q_OBJECT

public:
    HomeModelController(JellyfinApiFacade *api,
                        LibraryPrefetchController *prefetch,
                        QObject *parent = nullptr);

    MovieGridModel *resumeItems();
    MovieGridModel *nextUpItems();
    MovieGridModel *latestItems();
    QVariantList latestLibraryRows() const;
    QObject *latestLibraryItems(int rowIndex);

    MovieItem resumeItemAt(int index) const;
    MovieItem nextUpItemAt(int index) const;
    MovieItem latestItemAt(int index) const;
    MovieItem latestLibraryItemAt(int rowIndex, int itemIndex) const;

    void refresh(const std::vector<LibraryItem> &libraries);
    void recordLibraryUse(const LibraryItem &library);
    void updateResumeTicks(const QString &itemId, qint64 positionTicks);
    void updateFavorite(const QString &itemId, bool favorite);
    void updatePlayed(const QString &itemId, bool played);
    void reset();

signals:
    void latestLibraryRowsChanged();

private:
    struct LatestLibrarySection {
        int order = 0;
        LibraryItem library;
        std::unique_ptr<MovieGridModel> model;
    };

    void clearLatestLibraryRows();
    void addLatestLibraryRow(RequestGeneration::Token generation, int order,
                             const LibraryItem &library, const std::vector<MovieItem> &items);
    void handleHomeRowLoaded(RequestGeneration::Token generation);

    JellyfinApiFacade *m_api = nullptr;
    LibraryPrefetchController *m_prefetch = nullptr;
    MovieGridModel m_resumeItems;
    MovieGridModel m_nextUpItems;
    MovieGridModel m_latestItems;
    std::vector<LatestLibrarySection> m_latestLibrarySections;
    std::vector<LibraryItem> m_librariesForPrefetch;
    RequestGeneration m_generation;
    int m_loadsPending = 0;
    QStringList m_recentLibraryIds;
};

} // namespace JellyfinNative
