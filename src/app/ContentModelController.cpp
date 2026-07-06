#include "ContentModelController.h"

#include "../api/JellyfinApiFacade.h"
#include "../common/AsyncTask.h"
#include "LibraryPrefetchController.h"

#include <QDebug>

namespace JellyfinNative {

ContentModelController::ContentModelController(
    JellyfinApiFacade *api, LibraryPrefetchController *prefetch,
    QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_prefetch(prefetch)
{
}

MovieGridModel *ContentModelController::searchResults()
{
    return &m_searchResults;
}

MovieGridModel *ContentModelController::searchSuggestions()
{
    return &m_searchSuggestions;
}

MovieGridModel *ContentModelController::detailSeasons()
{
    return &m_detailSeasons;
}

MovieGridModel *ContentModelController::detailSimilarItems()
{
    return &m_detailSimilarItems;
}

MovieGridModel *ContentModelController::personItems()
{
    return &m_personItems;
}

bool ContentModelController::searchBusy() const
{
    return m_searchBusy;
}

QString ContentModelController::searchQuery() const
{
    return m_searchQuery;
}

bool ContentModelController::searchSuggestionsBusy() const
{
    return m_searchSuggestionsBusy;
}

bool ContentModelController::detailRowsBusy() const
{
    return m_detailRowsBusy;
}

bool ContentModelController::personItemsBusy() const
{
    return m_personItemsBusy;
}

QVariantMap ContentModelController::detailItem() const
{
    return m_detailItem;
}

MovieItem ContentModelController::searchResultAt(int index) const
{
    return m_searchResults.movieAt(index);
}

MovieItem ContentModelController::suggestionAt(int index) const
{
    return m_searchSuggestions.movieAt(index);
}

MovieItem ContentModelController::detailSeasonAt(int index) const
{
    return m_detailSeasons.movieAt(index);
}

MovieItem ContentModelController::detailSimilarItemAt(int index) const
{
    return m_detailSimilarItems.movieAt(index);
}

MovieItem ContentModelController::personItemAt(int index) const
{
    return m_personItems.movieAt(index);
}

void ContentModelController::search(const QString &query)
{
    const QString trimmed = query.trimmed();
    const RequestGeneration::Token generation = m_searchGeneration.next();
    m_searchQuery = trimmed;

    if (trimmed.size() < 2 || !m_api ||
        m_api->session().accessToken.isEmpty()) {
        m_searchBusy = false;
        m_searchResults.clear();
        emit searchChanged();
        return;
    }

    m_searchBusy = true;
    emit searchChanged();

    Async::runLatest(
        this, m_api->searchItems(trimmed), m_searchGeneration, generation,
        [this](const std::vector<MovieItem> &items) {
            m_searchResults.setMovies(items);
            m_prefetch->prefetchPosters(items);
            m_searchBusy = false;
            emit searchChanged();
        },
        [this](const std::exception_ptr &error) {
            m_searchResults.clear();
            m_searchBusy = false;
            emit searchChanged();
            emit errorOccurred(exceptionMessage(error));
        });
}

void ContentModelController::clearSearch()
{
    m_searchGeneration.invalidate();
    m_searchQuery.clear();
    m_searchBusy = false;
    m_searchResults.clear();
    emit searchChanged();
}

void ContentModelController::loadSearchSuggestions()
{
    if (!m_api || m_api->session().accessToken.isEmpty() ||
        m_searchSuggestionsLoaded || m_searchSuggestionsBusy) {
        return;
    }

    const RequestGeneration::Token generation =
        m_searchSuggestionsGeneration.next();
    m_searchSuggestionsBusy = true;
    emit searchSuggestionsChanged();

    Async::runLatest(
        this, m_api->fetchSearchSuggestions(),
        m_searchSuggestionsGeneration, generation,
        [this](const std::vector<MovieItem> &items) {
            m_searchSuggestions.setMovies(items);
            m_prefetch->prefetchPosters(items);
            m_searchSuggestionsBusy = false;
            m_searchSuggestionsLoaded = true;
            emit searchSuggestionsChanged();
        },
        [this](const std::exception_ptr &error) {
            m_searchSuggestions.clear();
            m_searchSuggestionsBusy = false;
            emit searchSuggestionsChanged();
            qWarning() << "search: suggestions fetch failed"
                       << exceptionMessage(error);
        });
}

void ContentModelController::loadDetailRows(const QString &itemId,
                                            const QString &itemType,
                                            const QString &seriesId,
                                            const QString &seasonId)
{
    const RequestGeneration::Token generation =
        m_detailRowsGeneration.next();
    m_detailRowsPending = 0;
    m_detailRowsBusy = false;
    m_detailSeasons.clear();
    m_detailSimilarItems.clear();

    if (itemId.isEmpty() || !m_api ||
        m_api->session().accessToken.isEmpty()) {
        emit detailRowsChanged();
        return;
    }

    const bool loadSeasons = itemType == QStringLiteral("Series");
    const bool loadEpisodes = (itemType == QStringLiteral("Episode") ||
                               itemType == QStringLiteral("Season")) &&
                              !seriesId.isEmpty();
    m_detailRowsBusy = true;
    m_detailRowsPending = (loadSeasons || loadEpisodes) ? 2 : 1;
    emit detailRowsChanged();

    qInfo() << "detail rows: loading" << itemType << itemId
            << "context="
            << (loadSeasons ? "seasons" : loadEpisodes ? "episodes" : "none");

    if (loadSeasons) {
        Async::runLatest(
            this, m_api->fetchSeasons(itemId), m_detailRowsGeneration,
            generation,
            [this, generation, itemId](
                const std::vector<MovieItem> &seasons) {
                qInfo() << "detail rows: seasons loaded" << itemId
                        << seasons.size();
                m_detailSeasons.setMovies(seasons);
                m_prefetch->prefetchPosters(seasons);
                emit detailRowsChanged();
                finishDetailRowLoad(generation);
            },
            [this, generation, itemId](
                const std::exception_ptr &error) {
                qWarning() << "detail rows: seasons fetch failed" << itemId
                           << exceptionMessage(error);
                finishDetailRowLoad(generation);
            });
    } else if (loadEpisodes) {
        Async::runLatest(
            this, m_api->fetchEpisodes(seriesId, seasonId), m_detailRowsGeneration,
            generation,
            [this, generation, seriesId](
                const std::vector<MovieItem> &episodes) {
                qInfo() << "detail rows: episodes loaded" << seriesId
                        << episodes.size();
                m_detailSeasons.setMovies(episodes);
                m_prefetch->prefetchPosters(episodes);
                emit detailRowsChanged();
                finishDetailRowLoad(generation);
            },
            [this, generation, seriesId](
                const std::exception_ptr &error) {
                qWarning() << "detail rows: episodes fetch failed" << seriesId
                           << exceptionMessage(error);
                finishDetailRowLoad(generation);
            });
    }

    Async::runLatest(
        this, m_api->fetchSimilarItems(itemId), m_detailRowsGeneration,
        generation,
        [this, generation, itemId](const std::vector<MovieItem> &items) {
            qInfo() << "detail rows: similar loaded" << itemId
                    << items.size();
            m_detailSimilarItems.setMovies(items);
            m_prefetch->prefetchPosters(items);
            emit detailRowsChanged();
            finishDetailRowLoad(generation);
        },
        [this, generation, itemId](const std::exception_ptr &error) {
            qWarning() << "detail rows: similar fetch failed" << itemId
                       << exceptionMessage(error);
            finishDetailRowLoad(generation);
        });
}


void ContentModelController::loadItemDetail(const QString &itemId)
{
    const RequestGeneration::Token generation = m_detailItemGeneration.next();
    m_detailItem.clear();

    if (itemId.isEmpty() || !m_api || m_api->session().accessToken.isEmpty()) {
        emit detailItemChanged();
        return;
    }

    emit detailItemChanged();

    Async::runLatest(
        this, m_api->fetchItemDetails(itemId), m_detailItemGeneration, generation,
        [this](const MovieItem &item) {
            MovieGridModel detailModel;
            detailModel.setMovies({item});
            QVariantMap snapshot = detailModel.get(0);
            const QVariantMap details = detailModel.detailsAt(0);
            snapshot.insert(QStringLiteral("people"), details.value(QStringLiteral("people")));
            snapshot.insert(QStringLiteral("mediaSources"), details.value(QStringLiteral("mediaSources")));
            m_detailItem = std::move(snapshot);
            emit detailItemChanged();
        },
        [this](const std::exception_ptr &error) {
            m_detailItem.clear();
            emit detailItemChanged();
            emit errorOccurred(exceptionMessage(error));
        });
}

void ContentModelController::loadPersonItems(const QString &personId)
{
    const RequestGeneration::Token generation =
        m_personItemsGeneration.next();
    m_personItems.clear();
    if (personId.isEmpty() || !m_api ||
        m_api->session().accessToken.isEmpty()) {
        m_personItemsBusy = false;
        emit personItemsChanged();
        return;
    }

    m_personItemsBusy = true;
    emit personItemsChanged();

    Async::runLatest(
        this, m_api->fetchItemsByPerson(personId), m_personItemsGeneration,
        generation,
        [this](const std::vector<MovieItem> &items) {
            m_personItems.setMovies(items);
            m_prefetch->prefetchPosters(items);
            m_personItemsBusy = false;
            emit personItemsChanged();
        },
        [this](const std::exception_ptr &error) {
            m_personItems.clear();
            m_personItemsBusy = false;
            emit personItemsChanged();
            emit errorOccurred(exceptionMessage(error));
        });
}

void ContentModelController::updateResumeTicks(const QString &itemId,
                                               qint64 positionTicks)
{
    m_searchResults.updateResumeTicks(itemId, positionTicks);
    m_searchSuggestions.updateResumeTicks(itemId, positionTicks);
    m_detailSeasons.updateResumeTicks(itemId, positionTicks);
    m_detailSimilarItems.updateResumeTicks(itemId, positionTicks);
    m_personItems.updateResumeTicks(itemId, positionTicks);
}

void ContentModelController::updateFavorite(const QString &itemId,
                                            bool favorite)
{
    m_searchResults.updateFavorite(itemId, favorite);
    m_searchSuggestions.updateFavorite(itemId, favorite);
    m_detailSeasons.updateFavorite(itemId, favorite);
    m_detailSimilarItems.updateFavorite(itemId, favorite);
    m_personItems.updateFavorite(itemId, favorite);
}

void ContentModelController::updatePlayed(const QString &itemId, bool played)
{
    m_searchResults.updatePlayed(itemId, played);
    m_searchSuggestions.updatePlayed(itemId, played);
    m_detailSeasons.updatePlayed(itemId, played);
    m_detailSimilarItems.updatePlayed(itemId, played);
    m_personItems.updatePlayed(itemId, played);
}

void ContentModelController::reset()
{
    m_searchGeneration.invalidate();
    m_searchSuggestionsGeneration.invalidate();
    m_detailRowsGeneration.invalidate();
    m_detailItemGeneration.invalidate();
    m_personItemsGeneration.invalidate();

    m_searchResults.clear();
    m_searchSuggestions.clear();
    m_detailSeasons.clear();
    m_detailSimilarItems.clear();
    m_personItems.clear();
    m_detailItem.clear();
    m_searchQuery.clear();
    m_searchBusy = false;
    m_searchSuggestionsBusy = false;
    m_searchSuggestionsLoaded = false;
    m_detailRowsBusy = false;
    m_detailRowsPending = 0;
    m_personItemsBusy = false;

    emit searchChanged();
    emit searchSuggestionsChanged();
    emit detailRowsChanged();
    emit detailItemChanged();
    emit personItemsChanged();
}

void ContentModelController::finishDetailRowLoad(
    RequestGeneration::Token generation)
{
    if (!m_detailRowsGeneration.isCurrent(generation) ||
        m_detailRowsPending <= 0) {
        return;
    }

    --m_detailRowsPending;
    if (m_detailRowsPending == 0) {
        m_detailRowsBusy = false;
        emit detailRowsChanged();
    }
}

} // namespace JellyfinNative
