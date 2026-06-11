#pragma once

#include "../common/RequestGeneration.h"
#include "../models/MovieGridModel.h"

#include <QObject>
#include <QString>

namespace JellyfinNative {

class JellyfinApiFacade;
class LibraryPrefetchController;

class ContentModelController final : public QObject
{
    Q_OBJECT

public:
    ContentModelController(JellyfinApiFacade *api,
                           LibraryPrefetchController *prefetch,
                           QObject *parent = nullptr);

    MovieGridModel *searchResults();
    MovieGridModel *searchSuggestions();
    MovieGridModel *detailSeasons();
    MovieGridModel *detailSimilarItems();
    MovieGridModel *personItems();

    bool searchBusy() const;
    QString searchQuery() const;
    bool searchSuggestionsBusy() const;
    bool detailRowsBusy() const;
    bool personItemsBusy() const;

    MovieItem searchResultAt(int index) const;
    MovieItem suggestionAt(int index) const;
    MovieItem detailSeasonAt(int index) const;
    MovieItem detailSimilarItemAt(int index) const;
    MovieItem personItemAt(int index) const;

    void search(const QString &query);
    void clearSearch();
    void loadSearchSuggestions();
    void loadDetailRows(const QString &itemId, const QString &itemType);
    void loadPersonItems(const QString &personId);
    void updateResumeTicks(const QString &itemId, qint64 positionTicks);
    void updateFavorite(const QString &itemId, bool favorite);
    void updatePlayed(const QString &itemId, bool played);
    void reset();

signals:
    void searchChanged();
    void searchSuggestionsChanged();
    void detailRowsChanged();
    void personItemsChanged();
    void errorOccurred(const QString &message);

private:
    void finishDetailRowLoad(RequestGeneration::Token generation);

    JellyfinApiFacade *m_api = nullptr;
    LibraryPrefetchController *m_prefetch = nullptr;
    MovieGridModel m_searchResults;
    MovieGridModel m_searchSuggestions;
    MovieGridModel m_detailSeasons;
    MovieGridModel m_detailSimilarItems;
    MovieGridModel m_personItems;
    QString m_searchQuery;
    bool m_searchBusy = false;
    RequestGeneration m_searchGeneration;
    bool m_searchSuggestionsBusy = false;
    RequestGeneration m_searchSuggestionsGeneration;
    bool m_searchSuggestionsLoaded = false;
    bool m_detailRowsBusy = false;
    RequestGeneration m_detailRowsGeneration;
    int m_detailRowsPending = 0;
    bool m_personItemsBusy = false;
    RequestGeneration m_personItemsGeneration;
};

} // namespace JellyfinNative
