#include "ContentModelController.h"

#include "../api/JellyfinApiFacade.h"
#include "../common/AsyncTask.h"
#include "LibraryPrefetchController.h"

#include <QDebug>

#include <utility>

namespace JellyfinNative {

ContentModelController::ContentModelController(
    JellyfinApiFacade *api, LibraryPrefetchController *prefetch, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_prefetch(prefetch)
{
}

void ContentModelController::loadDetailRows(
    const QString& itemId, const QString& itemType, const QString& seriesId, const QString& seasonId)
{
    const RequestGeneration::Token generation = m_detailRowsGeneration.next();
    m_detailRowsPending = 0;
    m_detailRowsBusy = false;
    m_detailSeasons.clear();
    m_detailSeasonOptions.clear();
    m_detailSimilarItems.clear();

    if (itemId.isEmpty() || !m_api || m_api->session().accessToken.isEmpty()) {
        emit detailRowsChanged();
        return;
    }

    const bool loadSeasons = itemType == QStringLiteral("Series");
    const bool loadEpisodes
        = (itemType == QStringLiteral("Episode") || itemType == QStringLiteral("Season")) && !seriesId.isEmpty();
    const bool loadSeasonOptions = loadEpisodes;
    const bool loadBoxSet = itemType == QStringLiteral("BoxSet");
    const bool loadContext = loadSeasons || loadEpisodes || loadBoxSet;
    m_detailRowsBusy = true;
    m_detailRowsPending = 1 + (loadContext ? 1 : 0) + (loadSeasonOptions ? 1 : 0);
    emit detailRowsChanged();

    qInfo() << "detail rows: loading" << itemType << itemId << "context="
            << (loadSeasons           ? "seasons"
                       : loadEpisodes ? "episodes"
                       : loadBoxSet   ? "boxset"
                                      : "none");

    if (loadSeasons) {
        Async::runLatest(
            this, m_api->fetchSeasons(itemId), m_detailRowsGeneration, generation,
            [this, generation, itemId](const std::vector<MovieItem>& seasons) {
                qInfo() << "detail rows: seasons loaded" << itemId << seasons.size();
                m_detailSeasons.setMovies(seasons);
                m_prefetch->prefetchPosters(seasons);
                emit detailRowsChanged();
                finishDetailRowLoad(generation);
            },
            [this, generation, itemId](const std::exception_ptr& error) {
                qWarning() << "detail rows: seasons fetch failed" << itemId << exceptionMessage(error);
                finishDetailRowLoad(generation);
            });
    } else if (loadEpisodes) {
        Async::runLatest(
            this, m_api->fetchEpisodes(seriesId, seasonId), m_detailRowsGeneration, generation,
            [this, generation, seriesId](const std::vector<MovieItem>& episodes) {
                qInfo() << "detail rows: episodes loaded" << seriesId << episodes.size();
                m_detailSeasons.setMovies(episodes);
                m_prefetch->prefetchPosters(episodes);
                emit detailRowsChanged();
                finishDetailRowLoad(generation);
            },
            [this, generation, seriesId](const std::exception_ptr& error) {
                qWarning() << "detail rows: episodes fetch failed" << seriesId << exceptionMessage(error);
                finishDetailRowLoad(generation);
            });
    } else if (loadBoxSet) {
        Async::runLatest(
            this, m_api->fetchBrowsePage(BrowseDescriptor::boxSet(itemId), 0, 200), m_detailRowsGeneration, generation,
            [this, generation, itemId](const PagedMovieItems& page) {
                qInfo() << "detail rows: box set children loaded" << itemId << page.items.size();
                m_detailSeasons.setMovies(page.items);
                m_prefetch->prefetchPosters(page.items);
                emit detailRowsChanged();
                finishDetailRowLoad(generation);
            },
            [this, generation, itemId](const std::exception_ptr& error) {
                qWarning() << "detail rows: box set children fetch failed" << itemId << exceptionMessage(error);
                finishDetailRowLoad(generation);
            });
    }

    if (loadSeasonOptions) {
        Async::runLatest(
            this, m_api->fetchSeasons(seriesId), m_detailRowsGeneration, generation,
            [this, generation, seriesId](const std::vector<MovieItem>& seasons) {
                qInfo() << "detail rows: season options loaded" << seriesId << seasons.size();
                m_detailSeasonOptions.setMovies(seasons);
                m_prefetch->prefetchPosters(seasons);
                emit detailRowsChanged();
                finishDetailRowLoad(generation);
            },
            [this, generation, seriesId](const std::exception_ptr& error) {
                qWarning() << "detail rows: season options fetch failed" << seriesId << exceptionMessage(error);
                finishDetailRowLoad(generation);
            });
    }

    Async::runLatest(
        this, m_api->fetchSimilarItems(itemId), m_detailRowsGeneration, generation,
        [this, generation, itemId](const std::vector<MovieItem>& items) {
            qInfo() << "detail rows: similar loaded" << itemId << items.size();
            m_detailSimilarItems.setMovies(items);
            m_prefetch->prefetchPosters(items);
            emit detailRowsChanged();
            finishDetailRowLoad(generation);
        },
        [this, generation, itemId](const std::exception_ptr& error) {
            qWarning() << "detail rows: similar fetch failed" << itemId << exceptionMessage(error);
            finishDetailRowLoad(generation);
        });
}

void ContentModelController::loadItemDetail(const QString& itemId)
{
    const RequestGeneration::Token generation = m_detailItemGeneration.next();
    m_detailItem = {};

    if (itemId.isEmpty() || !m_api || m_api->session().accessToken.isEmpty()) {
        emit detailItemChanged();
        return;
    }

    emit detailItemChanged();

    Async::runLatest(
        this, m_api->fetchItemDetails(itemId), m_detailItemGeneration, generation,
        [this](const MovieItem& item) {
            m_detailItem = item;
            emit detailItemChanged();
        },
        [this](const std::exception_ptr& error) {
            m_detailItem = {};
            emit detailItemChanged();
            emit errorOccurred(exceptionMessage(error));
        });
}

void ContentModelController::loadPersonItems(const QString& personId)
{
    const RequestGeneration::Token generation = m_personItemsGeneration.next();
    m_personItems.clear();
    if (personId.isEmpty() || !m_api || m_api->session().accessToken.isEmpty()) {
        m_personItemsBusy = false;
        emit personItemsChanged();
        return;
    }

    m_personItemsBusy = true;
    emit personItemsChanged();

    Async::runLatest(
        this, m_api->fetchItemsByPerson(personId), m_personItemsGeneration, generation,
        [this](const std::vector<MovieItem>& items) {
            m_personItems.setMovies(items);
            m_prefetch->prefetchPosters(items);
            m_personItemsBusy = false;
            emit personItemsChanged();
        },
        [this](const std::exception_ptr& error) {
            m_personItems.clear();
            m_personItemsBusy = false;
            emit personItemsChanged();
            emit errorOccurred(exceptionMessage(error));
        });
}

void ContentModelController::prepareLinkedItem(const QString& itemId, const QString& title, const QString& itemType,
    const QString& seriesId, const QString& seriesName, const QString& seasonId)
{
    MovieItem item;
    item.id = itemId;
    item.title = title;
    item.itemType = itemType;
    item.seriesId = seriesId;
    item.seriesName = seriesName;
    item.seasonId = seasonId;
    m_linkedItems.setMovies({ std::move(item) });
}

void ContentModelController::updateResumeTicks(const QString& itemId, qint64 positionTicks)
{
    m_detailSeasons.updateResumeTicks(itemId, positionTicks);
    m_detailSeasonOptions.updateResumeTicks(itemId, positionTicks);
    m_detailSimilarItems.updateResumeTicks(itemId, positionTicks);
    m_personItems.updateResumeTicks(itemId, positionTicks);
}

void ContentModelController::updateFavorite(const QString& itemId, bool favorite)
{
    m_detailSeasons.updateFavorite(itemId, favorite);
    m_detailSeasonOptions.updateFavorite(itemId, favorite);
    m_detailSimilarItems.updateFavorite(itemId, favorite);
    m_personItems.updateFavorite(itemId, favorite);
}

void ContentModelController::updatePlayed(const QString& itemId, bool played)
{
    m_detailSeasons.updatePlayed(itemId, played);
    m_detailSeasonOptions.updatePlayed(itemId, played);
    m_detailSimilarItems.updatePlayed(itemId, played);
    m_personItems.updatePlayed(itemId, played);
}

void ContentModelController::reset()
{
    m_detailRowsGeneration.invalidate();
    m_detailItemGeneration.invalidate();
    m_personItemsGeneration.invalidate();

    m_detailSeasons.clear();
    m_detailSeasonOptions.clear();
    m_detailSimilarItems.clear();
    m_personItems.clear();
    m_linkedItems.clear();
    m_detailItem = {};
    m_detailRowsBusy = false;
    m_detailRowsPending = 0;
    m_personItemsBusy = false;

    emit detailRowsChanged();
    emit detailItemChanged();
    emit personItemsChanged();
}

void ContentModelController::finishDetailRowLoad(RequestGeneration::Token generation)
{
    if (!m_detailRowsGeneration.isCurrent(generation) || m_detailRowsPending <= 0) {
        return;
    }

    --m_detailRowsPending;
    if (m_detailRowsPending == 0) {
        m_detailRowsBusy = false;
        emit detailRowsChanged();
    }
}

} // namespace JellyfinNative
