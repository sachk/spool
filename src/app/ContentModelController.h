#pragma once

#include "../common/RequestGeneration.h"
#include "../models/MovieGridModel.h"

#include <QObject>
#include <QString>

namespace JellyfinNative {

class JellyfinApiFacade;
class LibraryPrefetchController;

class ContentModelController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(JellyfinNative::MovieItem detailItem READ detailItem NOTIFY detailItemChanged)
    Q_PROPERTY(JellyfinNative::MovieGridModel *detailSeasons READ detailSeasons CONSTANT)
    Q_PROPERTY(JellyfinNative::MovieGridModel *detailSeasonOptions READ detailSeasonOptions CONSTANT)
    Q_PROPERTY(JellyfinNative::MovieGridModel *detailSimilarItems READ detailSimilarItems CONSTANT)
    Q_PROPERTY(JellyfinNative::MovieGridModel *linkedItems READ linkedItems CONSTANT)
    Q_PROPERTY(JellyfinNative::MovieGridModel *personItems READ personItems CONSTANT)
    Q_PROPERTY(bool detailRowsBusy READ detailRowsBusy NOTIFY detailRowsChanged)
    Q_PROPERTY(bool personItemsBusy READ personItemsBusy NOTIFY personItemsChanged)

public:
    ContentModelController(JellyfinApiFacade *api, LibraryPrefetchController *prefetch, QObject *parent = nullptr);

    MovieGridModel *detailSeasons()
    {
        return &m_detailSeasons;
    }
    MovieGridModel *detailSeasonOptions()
    {
        return &m_detailSeasonOptions;
    }
    MovieGridModel *detailSimilarItems()
    {
        return &m_detailSimilarItems;
    }
    MovieGridModel *personItems()
    {
        return &m_personItems;
    }
    MovieGridModel *linkedItems()
    {
        return &m_linkedItems;
    }
    bool detailRowsBusy() const
    {
        return m_detailRowsBusy;
    }
    bool personItemsBusy() const
    {
        return m_personItemsBusy;
    }
    MovieItem detailItem() const
    {
        return m_detailItem;
    }
    MovieItem detailSeasonAt(int index) const
    {
        return m_detailSeasons.movieAt(index);
    }

    Q_INVOKABLE void loadDetailRows(
        const QString& itemId, const QString& itemType, const QString& seriesId = {}, const QString& seasonId = {});
    Q_INVOKABLE void loadItemDetail(const QString& itemId);
    Q_INVOKABLE void loadPersonItems(const QString& personId);
    Q_INVOKABLE void prepareLinkedItem(const QString& itemId, const QString& title, const QString& itemType,
        const QString& seriesId = {}, const QString& seriesName = {}, const QString& seasonId = {});
    void updateResumeTicks(const QString& itemId, qint64 positionTicks);
    void updateFavorite(const QString& itemId, bool favorite);
    void updatePlayed(const QString& itemId, bool played);
    void reset();

signals:
    void detailRowsChanged();
    void detailItemChanged();
    void personItemsChanged();
    void errorOccurred(const QString& message);

private:
    void finishDetailRowLoad(RequestGeneration::Token generation);

    JellyfinApiFacade *m_api = nullptr;
    LibraryPrefetchController *m_prefetch = nullptr;
    MovieGridModel m_detailSeasons;
    MovieGridModel m_detailSeasonOptions;
    MovieGridModel m_detailSimilarItems;
    MovieGridModel m_personItems;
    MovieGridModel m_linkedItems;
    MovieItem m_detailItem;
    bool m_detailRowsBusy = false;
    RequestGeneration m_detailRowsGeneration;
    RequestGeneration m_detailItemGeneration;
    int m_detailRowsPending = 0;
    bool m_personItemsBusy = false;
    RequestGeneration m_personItemsGeneration;
};

} // namespace JellyfinNative
