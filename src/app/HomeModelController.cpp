#include "HomeModelController.h"

#include "../api/JellyfinApiFacade.h"
#include "../common/AsyncTask.h"
#include "LibraryQuery.h"
#include "LibraryPrefetchController.h"

#include <QDebug>
#include <QQmlEngine>
#include <QStringList>
#include <QVariantMap>

#include <algorithm>
#include <utility>

namespace JellyfinNative {

namespace {

QString homeItemSample(const std::vector<MovieItem> &items)
{
    QStringList sample;
    for (const auto &item : items) {
        sample.push_back(QStringLiteral("%1:%2:%3").arg(item.itemType, item.title).arg(item.resumeTicks));
        if (sample.size() >= 5)
            break;
    }
    return sample.join(QStringLiteral(" | "));
}

int latestLibraryLimit(const LibraryItem &library)
{
    if (library.collectionType == QStringLiteral("tvshows"))
        return 12;
    return 16;
}

} // namespace

HomeModelController::HomeModelController(JellyfinApiFacade *api,
                                         LibraryPrefetchController *prefetch,
                                         QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_prefetch(prefetch)
{
}

MovieGridModel *HomeModelController::resumeItems()
{
    return &m_resumeItems;
}

MovieGridModel *HomeModelController::nextUpItems()
{
    return &m_nextUpItems;
}

MovieGridModel *HomeModelController::latestItems()
{
    return &m_latestItems;
}

QVariantList HomeModelController::latestLibraryRows() const
{
    QVariantList rows;
    rows.reserve(static_cast<qsizetype>(m_latestLibrarySections.size()));
    for (size_t row = 0; row < m_latestLibrarySections.size(); ++row) {
        const LatestLibrarySection &section = m_latestLibrarySections[row];
        if (!section.model || section.model->rowCount() <= 0)
            continue;
        rows.push_back(QVariantMap{
            {QStringLiteral("rowIndex"), static_cast<int>(row)},
            {QStringLiteral("title"), QStringLiteral("Recently Added in %1").arg(section.library.name)},
            {QStringLiteral("libraryName"), section.library.name},
            {QStringLiteral("libraryId"), section.library.id},
            {QStringLiteral("collectionType"), section.library.collectionType},
            {QStringLiteral("kind"), section.library.collectionType == QStringLiteral("tvshows")
                                         || section.library.collectionType == QStringLiteral("movies")
                                     ? QStringLiteral("poster")
                                     : QStringLiteral("landscape")},
            {QStringLiteral("count"), section.model->rowCount()},
        });
    }
    return rows;
}

QObject *HomeModelController::latestLibraryItems(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= static_cast<int>(m_latestLibrarySections.size()))
        return nullptr;
    return m_latestLibrarySections[static_cast<size_t>(rowIndex)].model.get();
}

MovieItem HomeModelController::resumeItemAt(int index) const
{
    return m_resumeItems.movieAt(index);
}

MovieItem HomeModelController::nextUpItemAt(int index) const
{
    return m_nextUpItems.movieAt(index);
}

MovieItem HomeModelController::latestItemAt(int index) const
{
    return m_latestItems.movieAt(index);
}

MovieItem HomeModelController::latestLibraryItemAt(int rowIndex, int itemIndex) const
{
    if (rowIndex < 0 || rowIndex >= static_cast<int>(m_latestLibrarySections.size()))
        return {};

    const MovieGridModel *model = m_latestLibrarySections[static_cast<size_t>(rowIndex)].model.get();
    return model ? model->movieAt(itemIndex) : MovieItem{};
}

void HomeModelController::refresh(const std::vector<LibraryItem> &libraries)
{
    if (!m_api || m_api->session().accessToken.isEmpty())
        return;

    const RequestGeneration::Token generation = m_generation.next();
    clearLatestLibraryRows();
    m_latestItems.clear();
    m_prefetch->stop();
    m_librariesForPrefetch = libraries;

    std::vector<LibraryItem> latestLibraries;
    latestLibraries.reserve(libraries.size());
    for (const LibraryItem &library : libraries) {
        if (supportsLatestLibraryRow(library))
            latestLibraries.push_back(library);
    }

    m_loadsPending = 2 + static_cast<int>(latestLibraries.size());

    Async::runLatest(
        this, m_api->fetchResumeItems(), m_generation, generation,
        [this, generation](const std::vector<MovieItem> &items) {
            qInfo() << "home: resume items" << items.size() << homeItemSample(items);
            m_resumeItems.setMovies(items);
            m_prefetch->prefetchPosters(items);
            handleHomeRowLoaded(generation);
        },
        [this, generation](const std::exception_ptr &error) {
            qWarning() << "home: resume fetch failed" << exceptionMessage(error);
            handleHomeRowLoaded(generation);
        });

    Async::runLatest(
        this, m_api->fetchNextUpEpisodes(), m_generation, generation,
        [this, generation](const std::vector<MovieItem> &items) {
            qInfo() << "home: next-up items" << items.size() << homeItemSample(items);
            m_nextUpItems.setMovies(items);
            m_prefetch->prefetchPosters(items);
            handleHomeRowLoaded(generation);
        },
        [this, generation](const std::exception_ptr &error) {
            qWarning() << "home: next-up fetch failed" << exceptionMessage(error);
            handleHomeRowLoaded(generation);
        });

    for (int order = 0; order < static_cast<int>(latestLibraries.size()); ++order) {
        const LibraryItem library = latestLibraries[static_cast<size_t>(order)];
        Async::runLatest(
            this, m_api->fetchLatestItems(library.id, latestLibraryLimit(library)),
            m_generation, generation,
            [this, generation, order, library](const std::vector<MovieItem> &items) {
                qInfo() << "home: latest items" << library.name << items.size() << homeItemSample(items);
                addLatestLibraryRow(generation, order, library, items);
                handleHomeRowLoaded(generation);
            },
            [this, generation, library](const std::exception_ptr &error) {
                qWarning() << "home: latest fetch failed" << library.name << exceptionMessage(error);
                handleHomeRowLoaded(generation);
            });
    }
}

void HomeModelController::recordLibraryUse(const LibraryItem &library)
{
    if (library.id.isEmpty())
        return;

    m_recentLibraryIds.removeAll(library.id);
    m_recentLibraryIds.prepend(library.id);
    while (m_recentLibraryIds.size() > 12)
        m_recentLibraryIds.removeLast();
}

void HomeModelController::updateResumeTicks(const QString &itemId, qint64 positionTicks)
{
    m_resumeItems.updateResumeTicks(itemId, positionTicks);
    m_nextUpItems.updateResumeTicks(itemId, positionTicks);
    m_latestItems.updateResumeTicks(itemId, positionTicks);
    for (LatestLibrarySection &section : m_latestLibrarySections) {
        if (section.model)
            section.model->updateResumeTicks(itemId, positionTicks);
    }
}

void HomeModelController::updateFavorite(const QString &itemId, bool favorite)
{
    m_resumeItems.updateFavorite(itemId, favorite);
    m_nextUpItems.updateFavorite(itemId, favorite);
    m_latestItems.updateFavorite(itemId, favorite);
    for (LatestLibrarySection &section : m_latestLibrarySections) {
        if (section.model)
            section.model->updateFavorite(itemId, favorite);
    }
}

void HomeModelController::updatePlayed(const QString &itemId, bool played)
{
    m_resumeItems.updatePlayed(itemId, played);
    m_nextUpItems.updatePlayed(itemId, played);
    m_latestItems.updatePlayed(itemId, played);
    for (LatestLibrarySection &section : m_latestLibrarySections) {
        if (section.model)
            section.model->updatePlayed(itemId, played);
    }
}

void HomeModelController::reset()
{
    m_generation.invalidate();
    m_loadsPending = 0;
    m_prefetch->stop();
    m_resumeItems.clear();
    m_nextUpItems.clear();
    m_latestItems.clear();
    m_librariesForPrefetch.clear();
    m_recentLibraryIds.clear();
    clearLatestLibraryRows();
}

void HomeModelController::clearLatestLibraryRows()
{
    if (m_latestLibrarySections.empty())
        return;
    m_latestLibrarySections.clear();
    emit latestLibraryRowsChanged();
}

void HomeModelController::addLatestLibraryRow(RequestGeneration::Token generation,
                                             int order,
                                             const LibraryItem &library,
                                             const std::vector<MovieItem> &items)
{
    if (!m_generation.isCurrent(generation) || items.empty())
        return;

    auto model = std::make_unique<MovieGridModel>();
    QQmlEngine::setObjectOwnership(model.get(), QQmlEngine::CppOwnership);
    model->setMovies(items);

    LatestLibrarySection section;
    section.order = order;
    section.library = library;
    section.model = std::move(model);
    m_latestLibrarySections.push_back(std::move(section));

    std::sort(m_latestLibrarySections.begin(), m_latestLibrarySections.end(),
              [](const LatestLibrarySection &left, const LatestLibrarySection &right) {
                  return left.order < right.order;
              });

    m_prefetch->prefetchPosters(items);
    emit latestLibraryRowsChanged();
}

void HomeModelController::handleHomeRowLoaded(RequestGeneration::Token generation)
{
    if (!m_generation.isCurrent(generation) || m_loadsPending <= 0)
        return;

    --m_loadsPending;
    if (m_loadsPending == 0)
        m_prefetch->schedule(m_librariesForPrefetch, m_recentLibraryIds);
}

} // namespace JellyfinNative
