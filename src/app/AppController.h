#pragma once

#include "../cache/DatabaseManager.h"
#include "../common/JellyfinTypes.h"
#include "../common/RequestGeneration.h"
#include "../discovery/DiscoveryController.h"
#include "../models/DiscoveredServerModel.h"
#include "../models/LibraryListModel.h"
#include "../models/MovieGridModel.h"
#include "../player/PlayQueueController.h"
#include "../player/PlayerController.h"
#include "BrowseSessionController.h"
#include "ContentModelController.h"
#include "HomeModelController.h"
#include "NavigationState.h"
#include "QuickConnectController.h"
#include "SearchController.h"
#include "SessionController.h"
#include "SettingsController.h"
#include "SyncPlayController.h"

#include <QJsonObject>
#include <QObject>
#include <QVariantList>

#include <vector>

namespace JellyfinNative {

class ArtworkService;
class JellyfinApiFacade;
class LibraryPrefetchController;
class UserItemStateController;
class AppController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString page READ page NOTIFY pageChanged)
    Q_PROPERTY(bool busy MEMBER m_busy NOTIFY busyChanged)
    Q_PROPERTY(QString busyText MEMBER m_busyText NOTIFY busyChanged)
    Q_PROPERTY(QString errorText MEMBER m_errorText NOTIFY errorTextChanged)
    Q_PROPERTY(bool hasDefaultProfile MEMBER m_hasDefaultProfile NOTIFY defaultProfileChanged)
    Q_PROPERTY(QString currentViewKind READ currentViewKind NOTIFY currentLibraryNameChanged)
    Q_PROPERTY(bool currentUserCanManagePlaylists MEMBER m_currentUserCanManagePlaylists NOTIFY managementPolicyChanged)
    Q_PROPERTY(
        bool currentUserCanManageCollections MEMBER m_currentUserCanManageCollections NOTIFY managementPolicyChanged)
    Q_PROPERTY(bool currentUserCanRenameItems MEMBER m_currentUserCanRenameItems NOTIFY managementPolicyChanged)
    Q_PROPERTY(bool currentUserCanDeleteItems MEMBER m_currentUserCanDeleteItems NOTIFY managementPolicyChanged)
    Q_PROPERTY(QVariantList playlistTargets MEMBER m_playlistTargets NOTIFY managementTargetsChanged)
    Q_PROPERTY(QVariantList collectionTargets MEMBER m_collectionTargets NOTIFY managementTargetsChanged)

public:
    AppController(DatabaseManager *database, DiscoveryController *discovery, JellyfinApiFacade *api,
        ArtworkService *artwork, PlayerController *player, QObject *parent = nullptr);

    QString page() const;
    QString currentViewKind() const;
    DiscoveredServerModel *discoveredServers();
    LibraryListModel *libraries();
    BrowseSessionController *browse();
    HomeModelController *home();
    ContentModelController *content();
    SearchController *search();
    PlayerController *player();
    PlayQueueController *playQueue();
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
    Q_INVOKABLE void playFromModel(MovieGridModel *model, int index, bool fromStart = false);
    Q_INVOKABLE void playQueueNext();
    Q_INVOKABLE void playQueuePrevious();
    Q_INVOKABLE void playQueueItem(int index);
    Q_INVOKABLE void playNextFromItem(const MovieItem& item);
    Q_INVOKABLE void addToQueueFromItem(const MovieItem& item);
    Q_INVOKABLE void openSeriesById(const QString& seriesId, const QString& seriesName);
    Q_INVOKABLE void openSeasonById(const QString& seriesId, const QString& seasonId, const QString& seasonName);
    Q_INVOKABLE void loadMoreCurrentItems();
    Q_INVOKABLE void prefetchCurrentItems(int firstIndex, int lastIndex);
    Q_INVOKABLE void maybeLoadMoreCurrentItems(int visibleIndex);
    Q_INVOKABLE void setLibrarySort(const QString& sortBy, const QString& sortOrder);
    Q_INVOKABLE void setLibraryQueryListValue(const QString& key, const QString& value, bool enabled);
    Q_INVOKABLE void setLibraryQueryBoolValue(const QString& key, bool enabled);
    Q_INVOKABLE void setLibraryQueryNullableBoolValue(const QString& key, const QVariant& value);
    Q_INVOKABLE void clearLibraryFilters();
    Q_INVOKABLE void refreshCurrentLibrary();
    Q_INVOKABLE void openDetailSeason(int index);
    Q_INVOKABLE void playDetailContext(bool shuffled = false);
    Q_INVOKABLE void openGenre(const QString& genre);
    Q_INVOKABLE void openStudio(const QString& studio);
    Q_INVOKABLE void setFavorite(const QString& itemId, bool favorite);
    Q_INVOKABLE void setPlayed(const QString& itemId, bool played);
    Q_INVOKABLE void clearProgress(const QString& itemId);
    Q_INVOKABLE void refreshManagementTargets(const QString& kind);
    Q_INVOKABLE void createPlaylistForItem(const QString& name, const MovieItem& item);
    Q_INVOKABLE void addItemToPlaylist(const QString& playlistId, const MovieItem& item);
    Q_INVOKABLE void createCollectionForItem(const QString& name, const MovieItem& item);
    Q_INVOKABLE void addItemToCollection(const QString& collectionId, const MovieItem& item);
    Q_INVOKABLE void removeItemFromCurrentParent(const MovieItem& item);
    Q_INVOKABLE void movePlaylistItemInCurrent(const MovieItem& item, int delta);
    Q_INVOKABLE void renameManagedItem(const MovieItem& item, const QString& name);
    Q_INVOKABLE void deleteManagedItem(const MovieItem& item);
    Q_INVOKABLE void onMemoryPressure(const QString& level);
    Q_INVOKABLE void clearError();

signals:
    void pageChanged();
    void busyChanged();
    void errorTextChanged();
    void defaultProfileChanged();
    void currentLibraryNameChanged();
    void itemFavoriteChanged(const QString& itemId, bool favorite);
    void aggressiveMemoryPressure();
    void itemPlayedChanged(const QString& itemId, bool played);
    void managementPolicyChanged();
    void managementTargetsChanged();
    void managementOperationSucceeded(const QString& action);

private:
    void setPage(const QString& page);
    void setBusy(bool busy, const QString& busyText = {});
    void setErrorText(const QString& errorText);
    void resetApplicationState();
    void applyDiscoveredServersCache();
    void loadLibraries();
    void refreshHomeRows();
    QString homePayloadCacheKey() const;
    void applyCachedHomePayload();
    void saveHomePayload(const QJsonObject& payload);
    void showCurrentItemsPage(const PagedMovieItems& page, const QString& cacheKey, bool append);
    void setLibraryQuery(const QVariantMap& query);
    void loadLibraryFilterOptions(RequestGeneration::Token generation, const LibraryItem& library);
    void loadCurrentBrowsePage(const QString& loadingText);
    void loadCurrentUserPolicy();
    void setManagementTargets(const QString& kind, const std::vector<MovieItem>& items);
    QStringList itemIdsForManagement(const MovieItem& item) const;
    void refreshAfterManagementMutation(const QString& changedItemId = {});
    void openSeries(const MovieItem& series);
    void openSeason(const MovieItem& season);
    void openPlaylist(const MovieItem& playlist);
    void openBoxSet(const MovieItem& boxSet);
    void openFolder(const MovieItem& folder);
    void playMediaItem(const MovieItem& item, bool fromStart = false);
    void playQueuedItems(const std::vector<MovieItem>& items, int startIndex, bool fromStart = false);
    void playQueuedModel(MovieGridModel *model, bool shuffled = false);
    void playQueuedItem(const MovieItem& item, bool fromStart = false);
    void playQueueCurrent(bool fromStart = false);
    bool queueMutationAllowed();
    void enqueueEpisodeSuccessors(const MovieItem& episode);
    // Series/Season open their child listing; everything else plays directly.
    void playOrOpen(const MovieItem& item, bool fromStart = false);
    void playOrOpenFromModel(MovieGridModel *model, int index, bool fromStart = false);
    void handlePlaybackStopped(const QString& itemId, qint64 positionTicks, bool completed);
    bool authenticatedForManagement();
    bool playlistMutationAllowed();
    bool collectionMutationAllowed();
    bool renameMutationAllowed(const QString& itemType);
    bool deleteMutationAllowed();

    DatabaseManager *m_database = nullptr;
    DiscoveryController *m_discovery = nullptr;
    JellyfinApiFacade *m_api = nullptr;
    ArtworkService *m_artwork = nullptr;
    PlayerController *m_player = nullptr;
    SyncPlayController *m_syncPlay = nullptr;
    PlayQueueController *m_playQueue = nullptr;
    ContentModelController *m_content = nullptr;
    BrowseSessionController *m_browse = nullptr;
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
    QVariantList m_playlistTargets;
    QVariantList m_collectionTargets;
    bool m_currentUserCanManagePlaylists = false;
    bool m_currentUserCanManageCollections = false;
    bool m_currentUserCanRenameItems = false;
    bool m_currentUserCanDeleteItems = false;
    bool m_shuttingDown = false;
};

} // namespace JellyfinNative
