#include "SearchController.h"

#include "../api/JellyfinApiFacade.h"
#include "../common/AsyncTask.h"
#include "LibraryPrefetchController.h"

#include <QDebug>

namespace JellyfinNative {

namespace {
    constexpr int kSearchDebounceMs = 260;
}

SearchController::SearchController(JellyfinApiFacade *api, LibraryPrefetchController *prefetch, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_prefetch(prefetch)
{
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(kSearchDebounceMs);
    connect(&m_debounceTimer, &QTimer::timeout, this, &SearchController::submit);
}

void SearchController::setQuery(const QString& query)
{
    const QString trimmed = query.trimmed();
    if (m_query != trimmed) {
        m_query = trimmed;
        emit queryChanged();
    }

    if (m_query.size() < 2) {
        m_debounceTimer.stop();
        submit();
        return;
    }

    m_debounceTimer.start();
}

void SearchController::submit()
{
    m_debounceTimer.stop();

    if (m_query.size() < 2 || !authenticated()) {
        m_searchGeneration.invalidate();
        setBusy(false);
        m_results.clear();
        emit resultsChanged();
        return;
    }

    const RequestGeneration::Token generation = m_searchGeneration.next();
    setBusy(true);

    Async::runLatest(
        this, m_api->searchItems(m_query), m_searchGeneration, generation,
        [this](const std::vector<MovieItem>& items) {
            m_results.setMovies(items);
            if (m_prefetch)
                m_prefetch->prefetchPosters(items);
            setBusy(false);
            emit resultsChanged();
        },
        [this](const std::exception_ptr& error) {
            m_results.clear();
            setBusy(false);
            emit resultsChanged();
            emit errorOccurred(exceptionMessage(error));
        });
}

void SearchController::search(const QString& query)
{
    const QString trimmed = query.trimmed();
    if (m_query != trimmed) {
        m_query = trimmed;
        emit queryChanged();
    }
    submit();
}

void SearchController::clear()
{
    m_debounceTimer.stop();
    m_searchGeneration.invalidate();
    if (!m_query.isEmpty()) {
        m_query.clear();
        emit queryChanged();
    }
    setBusy(false);
    m_results.clear();
    emit resultsChanged();
}

void SearchController::loadSuggestions()
{
    if (!authenticated() || m_suggestionsLoaded || m_suggestionsBusy)
        return;

    const RequestGeneration::Token generation = m_suggestionsGeneration.next();
    setSuggestionsBusy(true);

    Async::runLatest(
        this, m_api->fetchSearchSuggestions(), m_suggestionsGeneration, generation,
        [this](const std::vector<MovieItem>& items) {
            m_suggestions.setMovies(items);
            if (m_prefetch)
                m_prefetch->prefetchPosters(items);
            m_suggestionsLoaded = true;
            setSuggestionsBusy(false);
            emit suggestionsChanged();
        },
        [this](const std::exception_ptr& error) {
            m_suggestions.clear();
            setSuggestionsBusy(false);
            emit suggestionsChanged();
            qWarning() << "search: suggestions fetch failed" << exceptionMessage(error);
        });
}

void SearchController::updateResumeTicks(const QString& itemId, qint64 positionTicks)
{
    m_results.updateResumeTicks(itemId, positionTicks);
    m_suggestions.updateResumeTicks(itemId, positionTicks);
}

void SearchController::updateFavorite(const QString& itemId, bool favorite)
{
    m_results.updateFavorite(itemId, favorite);
    m_suggestions.updateFavorite(itemId, favorite);
}

void SearchController::updatePlayed(const QString& itemId, bool played)
{
    m_results.updatePlayed(itemId, played);
    m_suggestions.updatePlayed(itemId, played);
}

void SearchController::reset()
{
    m_debounceTimer.stop();
    m_searchGeneration.invalidate();
    m_suggestionsGeneration.invalidate();
    m_results.clear();
    m_suggestions.clear();
    if (!m_query.isEmpty()) {
        m_query.clear();
        emit queryChanged();
    }
    setBusy(false);
    setSuggestionsBusy(false);
    m_suggestionsLoaded = false;
    emit resultsChanged();
    emit suggestionsChanged();
}

bool SearchController::authenticated() const
{
    return m_api && !m_api->session().accessToken.isEmpty();
}

void SearchController::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void SearchController::setSuggestionsBusy(bool busy)
{
    if (m_suggestionsBusy == busy)
        return;
    m_suggestionsBusy = busy;
    emit suggestionsChanged();
}

} // namespace JellyfinNative
