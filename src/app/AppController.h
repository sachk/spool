#pragma once

#include "../cache/DatabaseManager.h"
#include "../common/JellyfinTypes.h"
#include "../common/RequestGeneration.h"
#include "../discovery/DiscoveryController.h"
#include "../models/DiscoveredServerModel.h"
#include "../models/LibraryListModel.h"
#include "../models/MovieGridModel.h"
#include "QuickConnectController.h"
#include "SessionController.h"
#include "SettingsController.h"
#include "SearchController.h"
#include "../player/PlayerController.h"
#include "NavigationState.h"
#include "SyncPlayController.h"

#include <QObject>
#include <QJsonObject>
#include <QVariantList>

#include <vector>

namespace JellyfinNative {

class ArtworkService;
class JellyfinApiFacade;
class ContentModelController;
class CurrentItemsController;
class HomeModelController;
class LibraryPrefetchController;
class UserItemStateController;
class AppController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString page READ page NOTIFY pageChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString busyText READ busyText NOTIFY busyChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(bool hasDefaultProfile READ hasDefaultProfile NOTIFY defaultProfileChanged)
    Q_PROPERTY(QString currentLibraryName READ currentLibraryName NOTIFY currentLibraryNameChanged)
    Q_PROPERTY(QString currentContentLabel READ currentContentLabel NOTIFY currentLibraryNameChanged)
    Q_PROPERTY(QString currentViewKind READ currentViewKind NOTIFY currentLibraryNameChanged)
    Q_PROPERTY(QString currentLibraryId READ currentLibraryId NOTIFY currentLibraryNameChanged)
    Q_PROPERTY(QString currentLibraryCollectionType READ currentLibraryCollectionType NOTIFY currentLibraryNameChanged)
    Q_PROPERTY(QVariantMap libraryQuery READ libraryQuery NOTIFY libraryQueryChanged)
    Q_PROPERTY(QVariantMap libraryFilterOptions READ libraryFilterOptions NOTIFY libraryFilterOptionsChanged)
    Q_PROPERTY(int libraryFilterActiveCount READ libraryFilterActiveCount NOTIFY libraryQueryChanged)
    Q_PROPERTY(JellyfinNative::DiscoveredServerModel *discoveredServers READ discoveredServers CONSTANT)
    Q_PROPERTY(JellyfinNative::LibraryListModel *libraries READ libraries CONSTANT)
    Q_PROPERTY(JellyfinNative::MovieGridModel *movies READ movies CONSTANT)
    Q_PROPERTY(JellyfinNative::MovieGridModel *resumeItems READ resumeItems CONSTANT)
    Q_PROPERTY(JellyfinNative::MovieGridModel *nextUpItems READ nextUpItems CONSTANT)
    Q_PROPERTY(QVariantList latestLibraryRows READ latestLibraryRows NOTIFY latestLibraryRowsChanged)
    Q_PROPERTY(bool currentItemsLoadingMore READ currentItemsLoadingMore NOTIFY currentItemsPagingChanged)
    Q_PROPERTY(bool currentItemsHasMore READ currentItemsHasMore NOTIFY currentItemsPagingChanged)
    Q_PROPERTY(int currentItemsTotalCount READ currentItemsTotalCount NOTIFY currentItemsPagingChanged)
    Q_PROPERTY(JellyfinNative::MovieGridModel *detailSeasons READ detailSeasons CONSTANT)
    Q_PROPERTY(JellyfinNative::MovieGridModel *detailSimilarItems READ detailSimilarItems CONSTANT)
    Q_PROPERTY(bool detailRowsBusy READ detailRowsBusy NOTIFY detailRowsChanged)
    Q_PROPERTY(QVariantMap detailItem READ detailItem NOTIFY detailItemChanged)
    Q_PROPERTY(JellyfinNative::MovieGridModel *personItems READ personItems CONSTANT)
    Q_PROPERTY(bool personItemsBusy READ personItemsBusy NOTIFY personItemsChanged)
    Q_PROPERTY(JellyfinNative::SearchController *searchController READ searchController CONSTANT)
    Q_PROPERTY(JellyfinNative::PlayerController *player READ player CONSTANT)
    Q_PROPERTY(JellyfinNative::SyncPlayController *syncPlay READ syncPlay CONSTANT)
    Q_PROPERTY(JellyfinNative::SettingsController *settings READ settings CONSTANT)
    Q_PROPERTY(JellyfinNative::SessionController *session READ session CONSTANT)
    Q_PROPERTY(JellyfinNative::QuickConnectController *quickConnect READ quickConnect CONSTANT)

public:
    AppController(DatabaseManager *database,
                  DiscoveryController *discovery,
                  JellyfinApiFacade *api,
                  ArtworkService *artwork,
                  PlayerController *player,
                  QObject *parent = nullptr);

    QString page() const;
    bool busy() const;
    QString busyText() const;
    QString errorText() const;
    bool hasDefaultProfile() const;
    QString currentLibraryName() const;
    QString currentContentLabel() const;
    QString currentViewKind() const;
    QString currentLibraryId() const;
    QString currentLibraryCollectionType() const;
    QVariantMap libraryQuery() const;
    QVariantMap libraryFilterOptions() const;
    int libraryFilterActiveCount() const;
    DiscoveredServerModel *discoveredServers();
    LibraryListModel *libraries();
    MovieGridModel *movies();
    MovieGridModel *resumeItems();
    MovieGridModel *nextUpItems();
    QVariantList latestLibraryRows() const;
    bool currentItemsLoadingMore() const;
    bool currentItemsHasMore() const;
    int currentItemsTotalCount() const;
    MovieGridModel *detailSeasons();
    MovieGridModel *detailSimilarItems();
    QVariantMap detailItem() const;
    MovieGridModel *personItems();
    bool detailRowsBusy() const;
    bool personItemsBusy() const;
    SearchController *searchController();
    PlayerController *player();
    SyncPlayController *syncPlay();
    SettingsController *settings();
    SessionController *session();
    QuickConnectController *quickConnect();

    Q_INVOKABLE void initialize();
    void shutdown();
    Q_INVOKABLE void chooseDiscoveredServer(int index);
    Q_INVOKABLE void login();
    Q_INVOKABLE void useDefaultProfile();
    Q_INVOKABLE void switchUser();
    Q_INVOKABLE void logout();
    Q_INVOKABLE void goHome();
    Q_INVOKABLE void openLibrary(int index);
    Q_INVOKABLE void playMovie(int index, bool fromStart = false);
    Q_INVOKABLE void playResumeItem(int index, bool fromStart = false);
    Q_INVOKABLE void playNextUpItem(int index, bool fromStart = false);
    Q_INVOKABLE QObject *latestLibraryItems(int rowIndex);
    Q_INVOKABLE void playLatestLibraryItem(int rowIndex, int itemIndex, bool fromStart = false);
    Q_INVOKABLE void openSeriesById(const QString &seriesId, const QString &seriesName);
    Q_INVOKABLE void openSeasonById(const QString &seriesId, const QString &seasonId, const QString &seasonName);
    Q_INVOKABLE void playSearchResult(int index, bool fromStart = false);
    Q_INVOKABLE void playSuggestionItem(int index, bool fromStart = false);
    Q_INVOKABLE void loadMoreCurrentItems();
    Q_INVOKABLE void prefetchCurrentItems(int firstIndex, int lastIndex);
    Q_INVOKABLE void maybeLoadMoreCurrentItems(int visibleIndex);
    Q_INVOKABLE void setLibrarySort(const QString &sortBy, const QString &sortOrder);
    Q_INVOKABLE void setLibraryQueryListValue(const QString &key, const QString &value, bool enabled);
    Q_INVOKABLE void setLibraryQueryBoolValue(const QString &key, bool enabled);
    Q_INVOKABLE void setLibraryQueryNullableBoolValue(const QString &key, const QVariant &value);
    Q_INVOKABLE void clearLibraryFilters();
    Q_INVOKABLE void refreshCurrentLibrary();
    Q_INVOKABLE void loadDetailRows(const QString &itemId, const QString &itemType,
                                     const QString &seriesId = {}, const QString &seasonId = {});
    Q_INVOKABLE void loadItemDetail(const QString &itemId);
    Q_INVOKABLE void openDetailSeason(int index);
    Q_INVOKABLE void playDetailSeasonItem(int index, bool fromStart = false);
    Q_INVOKABLE void openGenre(const QString &genre);
    Q_INVOKABLE void openStudio(const QString &studio);
    Q_INVOKABLE void playDetailSimilarItem(int index, bool fromStart = false);
    Q_INVOKABLE void loadPersonItems(const QString &personId);
    Q_INVOKABLE void playPersonItem(int index, bool fromStart = false);
    Q_INVOKABLE void setFavorite(const QString &itemId, bool favorite);
    Q_INVOKABLE void setPlayed(const QString &itemId, bool played);
    Q_INVOKABLE void clearProgress(const QString &itemId);
    Q_INVOKABLE void onMemoryPressure(const QString &level);
    Q_INVOKABLE void clearError();

signals:
    void pageChanged();
    void busyChanged();
    void errorTextChanged();
    void defaultProfileChanged();
    void currentLibraryNameChanged();
    void detailRowsChanged();
    void detailItemChanged();
    void latestLibraryRowsChanged();
    void currentItemsPagingChanged();
    void libraryQueryChanged();
    void libraryFilterOptionsChanged();
    void personItemsChanged();
    void itemFavoriteChanged(const QString &itemId, bool favorite);
    void aggressiveMemoryPressure();
    void itemPlayedChanged(const QString &itemId, bool played);

private:
    void setPage(const QString &page);
    void setBusy(bool busy, const QString &busyText = {});
    void setErrorText(const QString &errorText);
    void resetApplicationState();
    void applyDiscoveredServersCache();
    void loadLibraries();
    void refreshHomeRows();
    QString homePayloadCacheKey() const;
    void applyCachedHomePayload();
    void saveHomePayload(const QJsonObject &payload);
    void showCurrentItems(const std::vector<MovieItem> &items, const QString &cacheKey = {});
    void showCurrentItemsPage(const PagedMovieItems &page, const QString &cacheKey, bool append);
    void setLibraryQuery(const QVariantMap &query);
    void loadLibraryFilterOptions(RequestGeneration::Token generation, const LibraryItem &library);
    void openSeries(const MovieItem &series);
    void openSeason(const MovieItem &season);
    void playMediaItem(const MovieItem &item, bool fromStart = false);
    // Series/Season open their child listing; everything else plays directly.
    void playOrOpen(const MovieItem &item, bool fromStart = false);
    void handlePlaybackStopped(const QString &itemId, qint64 positionTicks, bool completed);
    void playNextEpisodeAfter(const MovieItem &episode);

    DatabaseManager *m_database = nullptr;
    DiscoveryController *m_discovery = nullptr;
    JellyfinApiFacade *m_api = nullptr;
    ArtworkService *m_artwork = nullptr;
    PlayerController *m_player = nullptr;
    SyncPlayController *m_syncPlay = nullptr;
    ContentModelController *m_content = nullptr;
    CurrentItemsController *m_currentItems = nullptr;
    HomeModelController *m_home = nullptr;
    QuickConnectController *m_quickConnect = nullptr;
    SettingsController *m_settings = nullptr;
    SessionController *m_session = nullptr;
    LibraryPrefetchController *m_prefetch = nullptr;
    UserItemStateController *m_itemState = nullptr;
    SearchController *m_search = nullptr;
    DiscoveredServerModel m_discoveredServers;
    LibraryListModel m_libraries;
    NavigationState m_navigation;
    MovieItem m_activePlaybackItem;
    bool m_busy = false;
    bool m_hasDefaultProfile = false;
    QString m_busyText;
    QString m_errorText;
    RequestGeneration m_libraryLoadGeneration;
    bool m_shuttingDown = false;
};

} // namespace JellyfinNative
