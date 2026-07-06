#pragma once

#include "../common/RequestGeneration.h"
#include "../models/MovieGridModel.h"

#include <QObject>
#include <QString>
#include <QVariantMap>

namespace JellyfinNative {

class JellyfinApiFacade;
class LibraryPrefetchController;

class ContentModelController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap detailItem READ detailItem NOTIFY detailItemChanged)

public:
    ContentModelController(JellyfinApiFacade *api,
                           LibraryPrefetchController *prefetch,
                           QObject *parent = nullptr);

    MovieGridModel *detailSeasons();
    MovieGridModel *detailSimilarItems();
    MovieGridModel *personItems();

    bool detailRowsBusy() const;
    bool personItemsBusy() const;
    QVariantMap detailItem() const;

    MovieItem detailSeasonAt(int index) const;
    MovieItem detailSimilarItemAt(int index) const;
    MovieItem personItemAt(int index) const;

    void loadDetailRows(const QString &itemId, const QString &itemType,
                        const QString &seriesId, const QString &seasonId);
    void loadItemDetail(const QString &itemId);
    void loadPersonItems(const QString &personId);
    void updateResumeTicks(const QString &itemId, qint64 positionTicks);
    void updateFavorite(const QString &itemId, bool favorite);
    void updatePlayed(const QString &itemId, bool played);
    void reset();

signals:
    void detailRowsChanged();
    void detailItemChanged();
    void personItemsChanged();
    void errorOccurred(const QString &message);

private:
    void finishDetailRowLoad(RequestGeneration::Token generation);

    JellyfinApiFacade *m_api = nullptr;
    LibraryPrefetchController *m_prefetch = nullptr;
    MovieGridModel m_detailSeasons;
    MovieGridModel m_detailSimilarItems;
    MovieGridModel m_personItems;
    QVariantMap m_detailItem;
    bool m_detailRowsBusy = false;
    RequestGeneration m_detailRowsGeneration;
    RequestGeneration m_detailItemGeneration;
    int m_detailRowsPending = 0;
    bool m_personItemsBusy = false;
    RequestGeneration m_personItemsGeneration;
};

} // namespace JellyfinNative
