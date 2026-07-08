#pragma once

#include "../common/JellyfinTypes.h"
#include "../common/RequestGeneration.h"
#include "../models/MovieGridModel.h"
#include <QJsonObject>

#include <QObject>
#include <QStringList>
#include <QVariantList>

#include <memory>
#include <vector>

namespace JellyfinNative {

class JellyfinApiFacade;
class LibraryPrefetchController;

class HomeModelController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(JellyfinNative::MovieGridModel *resumeItems READ resumeItems CONSTANT)
    Q_PROPERTY(JellyfinNative::MovieGridModel *nextUpItems READ nextUpItems CONSTANT)
    Q_PROPERTY(QVariantList latestLibraryRows READ latestLibraryRows NOTIFY latestLibraryRowsChanged)

public:
    HomeModelController(JellyfinApiFacade *api, LibraryPrefetchController *prefetch, QObject *parent = nullptr);

    MovieGridModel *resumeItems();
    MovieGridModel *nextUpItems();
    QVariantList latestLibraryRows() const;
    Q_INVOKABLE QObject *latestLibraryItems(int rowIndex);

    MovieItem resumeItemAt(int index) const;
    MovieItem nextUpItemAt(int index) const;
    MovieItem latestLibraryItemAt(int rowIndex, int itemIndex) const;

    bool applyCachedPayload(const QJsonObject& payload);
    QJsonObject currentPayload() const;
    void refresh(const std::vector<LibraryItem>& libraries);
    void recordLibraryUse(const LibraryItem& library);
    void upsertResumeItem(MovieItem item, qint64 positionTicks);
    void updateResumeTicks(const QString& itemId, qint64 positionTicks);
    void updateFavorite(const QString& itemId, bool favorite);
    void updatePlayed(const QString& itemId, bool played);
    void reset();

signals:
    void latestLibraryRowsChanged();
    void homePayloadReady(const QJsonObject& payload);

private:
    struct LatestLibrarySection {
        int order = 0;
        LibraryItem library;
        std::unique_ptr<MovieGridModel> model;
    };
    struct PendingLatestLibrarySection {
        int order = 0;
        LibraryItem library;
        std::vector<MovieItem> items;
    };
    struct PendingHomeRefresh {
        RequestGeneration::Token generation;
        int remaining = 0;
        std::vector<MovieItem> resumeItems;
        std::vector<MovieItem> nextUpItems;
        std::vector<PendingLatestLibrarySection> latestSections;
        std::vector<LibraryItem> librariesForPrefetch;
    };

    void finishHomeRefresh(const std::shared_ptr<PendingHomeRefresh>& refresh);
    void replaceLatestLibraryRows(std::vector<PendingLatestLibrarySection> sections);
    QJsonObject payloadFromSections(const std::vector<MovieItem>& resumeItems,
        const std::vector<MovieItem>& nextUpItems, const std::vector<PendingLatestLibrarySection>& sections) const;

    JellyfinApiFacade *m_api = nullptr;
    LibraryPrefetchController *m_prefetch = nullptr;
    MovieGridModel m_resumeItems;
    MovieGridModel m_nextUpItems;
    std::vector<LatestLibrarySection> m_latestLibrarySections;
    RequestGeneration m_generation;
    bool m_refreshInFlight = false;
    bool m_loaded = false;
    QStringList m_recentLibraryIds;
};

} // namespace JellyfinNative
