#include "HomeModelController.h"

#include "../api/JellyfinApiFacade.h"
#include "../common/AsyncTask.h"
#include "../common/MetaJson.h"
#include "LibraryPrefetchController.h"
#include "LibraryQuery.h"

#include <QDebug>
#include <QJsonArray>
#include <QQmlEngine>
#include <QStringList>
#include <QVariantMap>

#include <algorithm>
#include <utility>

namespace JellyfinNative {

namespace {

    QString homeItemSample(const std::vector<MovieItem>& items)
    {
        QStringList sample;
        for (const auto& item : items) {
            sample.push_back(QStringLiteral("%1:%2:%3").arg(item.itemType, item.title).arg(item.resumeTicks));
            if (sample.size() >= 5)
                break;
        }
        return sample.join(QStringLiteral(" | "));
    }

    bool latestRowPrefersLandscape(const LibraryItem& library, const std::vector<MovieItem>& items)
    {
        if (!items.empty()) {
            const QString& type = items.front().itemType;
            if (type == QStringLiteral("Episode") || type == QStringLiteral("Video"))
                return true;
            if (type == QStringLiteral("Movie") || type == QStringLiteral("Series") || type == QStringLiteral("Season"))
                return false;
        }
        return library.collectionType != QStringLiteral("movies")
            && library.collectionType != QStringLiteral("tvshows");
    }

    QString latestRowKind(const LibraryItem& library, const std::vector<MovieItem>& items)
    {
        return latestRowPrefersLandscape(library, items) ? QStringLiteral("landscape") : QStringLiteral("poster");
    }

    void removeItem(MovieGridModel& model, const QString& itemId)
    {
        std::vector<MovieItem> items = model.movies();
        const auto oldSize = items.size();
        items.erase(
            std::remove_if(items.begin(), items.end(), [&itemId](const MovieItem& item) { return item.id == itemId; }),
            items.end());
        if (items.size() == oldSize)
            return;
        model.setMovies(items);
    }

    int latestLibraryLimit(const LibraryItem& library)
    {
        if (library.collectionType == QStringLiteral("tvshows"))
            return 12;
        return 16;
    }

    QJsonArray movieArrayToJson(const std::vector<MovieItem>& items)
    {
        QJsonArray array;
        for (const MovieItem& item : items)
            array.push_back(metaToJson(item));
        return array;
    }

    std::vector<MovieItem> movieArrayFromJson(const QJsonArray& array)
    {
        std::vector<MovieItem> items;
        items.reserve(array.size());
        for (const QJsonValue& value : array) {
            MovieItem item = metaFromJson<MovieItem>(value.toObject());
            if (!item.id.isEmpty())
                items.push_back(std::move(item));
        }
        return items;
    }

} // namespace

HomeModelController::HomeModelController(JellyfinApiFacade *api, LibraryPrefetchController *prefetch, QObject *parent)
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

QVariantList HomeModelController::latestLibraryRows() const
{
    QVariantList rows;
    rows.reserve(static_cast<qsizetype>(m_latestLibrarySections.size()));
    for (size_t row = 0; row < m_latestLibrarySections.size(); ++row) {
        const LatestLibrarySection& section = m_latestLibrarySections[row];
        if (!section.model || section.model->rowCount() <= 0)
            continue;
        const std::vector<MovieItem>& items = section.model->movies();
        rows.push_back(QVariantMap {
            { QStringLiteral("rowIndex"), static_cast<int>(row) },
            { QStringLiteral("title"), QStringLiteral("Recently Added in %1").arg(section.library.name) },
            { QStringLiteral("libraryName"), section.library.name },
            { QStringLiteral("libraryId"), section.library.id },
            { QStringLiteral("collectionType"), section.library.collectionType },
            { QStringLiteral("kind"), latestRowKind(section.library, items) },
            { QStringLiteral("count"), section.model->rowCount() },
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

bool HomeModelController::applyCachedPayload(const QJsonObject& payload)
{
    if (payload.isEmpty())
        return false;

    std::vector<PendingLatestLibrarySection> sections;
    const QJsonArray latestRows = payload.value(QStringLiteral("latestRows")).toArray();
    sections.reserve(latestRows.size());
    for (const QJsonValue& value : latestRows) {
        const QJsonObject row = value.toObject();
        PendingLatestLibrarySection section;
        section.order = row.value(QStringLiteral("order")).toInt();
        section.library = metaFromJson<LibraryItem>(row.value(QStringLiteral("library")).toObject());
        section.items = movieArrayFromJson(row.value(QStringLiteral("items")).toArray());
        if (!section.library.id.isEmpty() && !section.items.empty())
            sections.push_back(std::move(section));
    }

    m_resumeItems.setMovies(movieArrayFromJson(payload.value(QStringLiteral("resumeItems")).toArray()));
    m_nextUpItems.setMovies(movieArrayFromJson(payload.value(QStringLiteral("nextUpItems")).toArray()));
    replaceLatestLibraryRows(std::move(sections));
    emit latestLibraryRowsChanged();
    return m_resumeItems.rowCount() > 0 || m_nextUpItems.rowCount() > 0 || !m_latestLibrarySections.empty();
}

void HomeModelController::refresh(const std::vector<LibraryItem>& libraries)
{
    if (!m_api || m_api->session().accessToken.isEmpty())
        return;
    if (m_loaded || m_refreshInFlight)
        return;

    const RequestGeneration::Token generation = m_generation.next();
    m_refreshInFlight = true;
    m_prefetch->stop();
    Async::runScoped(
        this, refreshAsync(libraries, generation), []() {},
        [this, generation](const std::exception_ptr& error) {
            if (!m_generation.isCurrent(generation))
                return;
            m_refreshInFlight = false;
            qWarning() << "home: refresh failed" << exceptionMessage(error);
        },
        "home refresh");
}

QCoro::Task<void> HomeModelController::refreshAsync(
    std::vector<LibraryItem> libraries, RequestGeneration::Token generation)
{
    std::vector<LibraryItem> latestLibraries;
    latestLibraries.reserve(libraries.size());
    for (const LibraryItem& library : libraries) {
        if (supportsLatestLibraryRow(library))
            latestLibraries.push_back(library);
    }

    std::vector<MovieItem> resumeItems;
    try {
        resumeItems = co_await m_api->fetchResumeItems();
        qInfo() << "home: resume items" << resumeItems.size() << homeItemSample(resumeItems);
    } catch (const std::exception& error) {
        qWarning() << "home: resume fetch failed" << error.what();
    }
    if (!m_generation.isCurrent(generation))
        co_return;

    std::vector<MovieItem> nextUpItems;
    try {
        nextUpItems = co_await m_api->fetchNextUpEpisodes();
        qInfo() << "home: next-up items" << nextUpItems.size() << homeItemSample(nextUpItems);
    } catch (const std::exception& error) {
        qWarning() << "home: next-up fetch failed" << error.what();
    }
    if (!m_generation.isCurrent(generation))
        co_return;

    std::vector<PendingLatestLibrarySection> latestSections;
    latestSections.reserve(latestLibraries.size());
    for (int order = 0; order < static_cast<int>(latestLibraries.size()); ++order) {
        const LibraryItem library = latestLibraries[static_cast<size_t>(order)];
        try {
            std::vector<MovieItem> items = co_await m_api->fetchLatestItems(library.id, latestLibraryLimit(library));
            qInfo() << "home: latest items" << library.name << items.size() << homeItemSample(items);
            if (!items.empty())
                latestSections.push_back({ order, library, std::move(items) });
        } catch (const std::exception& error) {
            qWarning() << "home: latest fetch failed" << library.name << error.what();
        }
        if (!m_generation.isCurrent(generation))
            co_return;
    }

    m_refreshInFlight = false;
    m_loaded = true;
    m_resumeItems.setMovies(resumeItems);
    m_nextUpItems.setMovies(nextUpItems);
    emit homePayloadReady(payloadFromSections(resumeItems, nextUpItems, latestSections));
    replaceLatestLibraryRows(std::move(latestSections));

    m_prefetch->prefetchPosters(resumeItems, 0, 12, LibraryPrefetchController::ImageKind::Landscape);
    m_prefetch->prefetchPosters(nextUpItems, 0, 12, LibraryPrefetchController::ImageKind::Landscape);
    for (const LatestLibrarySection& section : m_latestLibrarySections) {
        if (!section.model)
            continue;
        m_prefetch->prefetchPosters(section.model->movies(), 0, 12,
            latestRowPrefersLandscape(section.library, section.model->movies())
                ? LibraryPrefetchController::ImageKind::Landscape
                : LibraryPrefetchController::ImageKind::Poster);
    }
    m_prefetch->schedule(libraries, m_recentLibraryIds);
    emit latestLibraryRowsChanged();
}

void HomeModelController::recordLibraryUse(const LibraryItem& library)
{
    if (library.id.isEmpty())
        return;

    m_recentLibraryIds.removeAll(library.id);
    m_recentLibraryIds.prepend(library.id);
    while (m_recentLibraryIds.size() > 12)
        m_recentLibraryIds.removeLast();
}

void HomeModelController::upsertResumeItem(MovieItem item, qint64 positionTicks)
{
    if (item.id.isEmpty())
        return;

    item.resumeTicks = normalizedResumeTicks(positionTicks, item.runtimeTicks);
    item.played = false;
    if (!isMeaningfulResumePosition(item.resumeTicks, item.runtimeTicks)) {
        updateResumeTicks(item.id, item.resumeTicks);
        return;
    }

    const auto current = m_resumeItems.movies();
    if (!current.empty() && current.front().id == item.id) {
        m_resumeItems.updateResumeTicks(item.id, item.resumeTicks);
        return;
    }

    std::vector<MovieItem> items = current;
    items.erase(std::remove_if(items.begin(), items.end(),
                    [&item](const MovieItem& candidate) { return candidate.id == item.id; }),
        items.end());
    items.insert(items.begin(), item);
    if (items.size() > 24)
        items.resize(24);

    m_resumeItems.setMovies(items);
    m_prefetch->prefetchPosters(
        std::vector<MovieItem> { item }, 0, 12, LibraryPrefetchController::ImageKind::Landscape);
}

void HomeModelController::updateResumeTicks(const QString& itemId, qint64 positionTicks)
{
    m_resumeItems.updateResumeTicks(itemId, positionTicks);
    m_resumeItems.removeUnresumable();
    m_nextUpItems.updateResumeTicks(itemId, positionTicks);
    for (LatestLibrarySection& section : m_latestLibrarySections) {
        if (section.model)
            section.model->updateResumeTicks(itemId, positionTicks);
    }
}

void HomeModelController::updateFavorite(const QString& itemId, bool favorite)
{
    m_resumeItems.updateFavorite(itemId, favorite);
    m_nextUpItems.updateFavorite(itemId, favorite);
    for (LatestLibrarySection& section : m_latestLibrarySections) {
        if (section.model)
            section.model->updateFavorite(itemId, favorite);
    }
}

void HomeModelController::updatePlayed(const QString& itemId, bool played)
{
    m_resumeItems.updatePlayed(itemId, played);
    m_resumeItems.removeUnresumable();
    if (played)
        removeItem(m_nextUpItems, itemId);
    else
        m_nextUpItems.updatePlayed(itemId, played);
    for (LatestLibrarySection& section : m_latestLibrarySections) {
        if (section.model)
            section.model->updatePlayed(itemId, played);
    }
}

void HomeModelController::reset()
{
    m_generation.invalidate();
    m_refreshInFlight = false;
    m_loaded = false;
    m_prefetch->stop();
    m_resumeItems.clear();
    m_nextUpItems.clear();
    m_recentLibraryIds.clear();
    m_latestLibrarySections.clear();
    emit latestLibraryRowsChanged();
}

void HomeModelController::replaceLatestLibraryRows(std::vector<PendingLatestLibrarySection> sections)
{
    std::sort(sections.begin(), sections.end(),
        [](const PendingLatestLibrarySection& left, const PendingLatestLibrarySection& right) {
            return left.order < right.order;
        });

    m_latestLibrarySections.clear();
    for (const PendingLatestLibrarySection& pending : sections) {
        auto model = std::make_unique<MovieGridModel>();
        QQmlEngine::setObjectOwnership(model.get(), QQmlEngine::CppOwnership);
        model->setMovies(pending.items);
        m_latestLibrarySections.push_back({ pending.order, pending.library, std::move(model) });
    }
}

QJsonObject HomeModelController::payloadFromSections(const std::vector<MovieItem>& resumeItems,
    const std::vector<MovieItem>& nextUpItems, const std::vector<PendingLatestLibrarySection>& sections) const
{
    QJsonArray latestRows;
    for (const PendingLatestLibrarySection& section : sections) {
        if (section.library.id.isEmpty() || section.items.empty())
            continue;
        latestRows.push_back(QJsonObject {
            { QStringLiteral("order"), section.order },
            { QStringLiteral("library"), metaToJson(section.library) },
            { QStringLiteral("items"), movieArrayToJson(section.items) },
        });
    }
    return {
        { QStringLiteral("schemaVersion"), 2 },
        { QStringLiteral("resumeItems"), movieArrayToJson(resumeItems) },
        { QStringLiteral("nextUpItems"), movieArrayToJson(nextUpItems) },
        { QStringLiteral("latestRows"), latestRows },
    };
}

} // namespace JellyfinNative
