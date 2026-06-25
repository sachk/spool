#pragma once

#include "../cache/DatabaseManager.h"
#include "../common/JellyfinTypes.h"
#include "../common/RequestGeneration.h"
#include "../discovery/DiscoveryController.h"
#include "../models/DiscoveredServerModel.h"
#include "../models/LibraryListModel.h"
#include "../models/MovieGridModel.h"
#include "../player/PlayerController.h"
#include "NavigationState.h"
#include "SyncPlayController.h"

#include <QObject>
#include <QVariantList>

#include <vector>

namespace JellyfinNative {

class JellyfinApiFacade;
class ContentModelController;
class CurrentItemsController;
class HomeModelController;
class LibraryPrefetchController;
class QuickConnectController;
class SettingsController;
class SessionController;
class UserItemStateController;
class AppController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString page READ page NOTIFY pageChanged)
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY usernameChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString busyText READ busyText NOTIFY busyChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(QString quickConnectCode READ quickConnectCode NOTIFY quickConnectChanged)
    Q_PROPERTY(QString quickConnectStatus READ quickConnectStatus NOTIFY quickConnectChanged)
    Q_PROPERTY(bool quickConnectActive READ quickConnectActive NOTIFY quickConnectChanged)
    Q_PROPERTY(bool hasDefaultProfile READ hasDefaultProfile NOTIFY defaultProfileChanged)
    Q_PROPERTY(QString currentLibraryName READ currentLibraryName NOTIFY currentLibraryNameChanged)
    Q_PROPERTY(QString currentContentLabel READ currentContentLabel NOTIFY currentLibraryNameChanged)
    Q_PROPERTY(QString currentViewKind READ currentViewKind NOTIFY currentLibraryNameChanged)
    Q_PROPERTY(QString currentLibraryId READ currentLibraryId NOTIFY currentLibraryNameChanged)
    Q_PROPERTY(QString currentLibraryCollectionType READ currentLibraryCollectionType NOTIFY currentLibraryNameChanged)
    Q_PROPERTY(QVariantMap libraryQuery READ libraryQuery NOTIFY libraryQueryChanged)
    Q_PROPERTY(QVariantMap libraryFilterOptions READ libraryFilterOptions NOTIFY libraryFilterOptionsChanged)
    Q_PROPERTY(int libraryFilterActiveCount READ libraryFilterActiveCount NOTIFY libraryQueryChanged)
    Q_PROPERTY(bool settingsVisible READ settingsVisible NOTIFY settingsVisibleChanged)
    Q_PROPERTY(bool nightModeEnabled READ nightModeEnabled WRITE setNightModeEnabled NOTIFY nightModeEnabledChanged)
    Q_PROPERTY(bool toneMappingVisualizationEnabled READ toneMappingVisualizationEnabled
               WRITE setToneMappingVisualizationEnabled
               NOTIFY toneMappingVisualizationEnabledChanged)
    Q_PROPERTY(int maxStreamingBitrateMbps READ maxStreamingBitrateMbps
               WRITE setMaxStreamingBitrateMbps
               NOTIFY playbackPreferencesChanged)
    Q_PROPERTY(bool preferRemux READ preferRemux WRITE setPreferRemux
               NOTIFY playbackPreferencesChanged)
    Q_PROPERTY(int audioDelayMs READ audioDelayMs WRITE setAudioDelayMs NOTIFY audioDelayMsChanged)
    Q_PROPERTY(QString audioOutputMode READ audioOutputMode WRITE setAudioOutputMode NOTIFY audioOutputModeChanged)
    Q_PROPERTY(QStringList subtitleLanguageOptions READ subtitleLanguageOptions NOTIFY subtitleSettingsChanged)
    Q_PROPERTY(int subtitleLanguageIndex READ subtitleLanguageIndex NOTIFY subtitleSettingsChanged)
    Q_PROPERTY(QString subtitleMode READ subtitleMode NOTIFY subtitleSettingsChanged)
    Q_PROPERTY(QString subtitleBurnIn READ subtitleBurnIn NOTIFY subtitleSettingsChanged)
    Q_PROPERTY(bool subtitleRenderPgs READ subtitleRenderPgs NOTIFY subtitleSettingsChanged)
    Q_PROPERTY(bool subtitleAlwaysBurnIn READ subtitleAlwaysBurnIn NOTIFY subtitleSettingsChanged)
    Q_PROPERTY(QString subtitleStyling READ subtitleStyling NOTIFY subtitleSettingsChanged)
    Q_PROPERTY(QString subtitleTextSize READ subtitleTextSize NOTIFY subtitleSettingsChanged)
    Q_PROPERTY(QString subtitleTextWeight READ subtitleTextWeight NOTIFY subtitleSettingsChanged)
    Q_PROPERTY(QString subtitleFont READ subtitleFont NOTIFY subtitleSettingsChanged)
    Q_PROPERTY(QString subtitleTextColor READ subtitleTextColor NOTIFY subtitleSettingsChanged)
    Q_PROPERTY(QString subtitleDropShadow READ subtitleDropShadow NOTIFY subtitleSettingsChanged)
    Q_PROPERTY(int subtitleVerticalPosition READ subtitleVerticalPosition NOTIFY subtitleSettingsChanged)
    Q_PROPERTY(QString redButtonAction READ redButtonAction WRITE setRedButtonAction NOTIFY buttonRemapChanged)
    Q_PROPERTY(QString greenButtonAction READ greenButtonAction WRITE setGreenButtonAction NOTIFY buttonRemapChanged)
    Q_PROPERTY(QString yellowButtonAction READ yellowButtonAction WRITE setYellowButtonAction NOTIFY buttonRemapChanged)
    Q_PROPERTY(QString blueButtonAction READ blueButtonAction WRITE setBlueButtonAction NOTIFY buttonRemapChanged)
    Q_PROPERTY(QStringList availableButtonActions READ availableButtonActions CONSTANT)
    Q_PROPERTY(JellyfinNative::DiscoveredServerModel *discoveredServers READ discoveredServers CONSTANT)
    Q_PROPERTY(JellyfinNative::LibraryListModel *libraries READ libraries CONSTANT)
    Q_PROPERTY(JellyfinNative::MovieGridModel *movies READ movies CONSTANT)
    Q_PROPERTY(JellyfinNative::MovieGridModel *resumeItems READ resumeItems CONSTANT)
    Q_PROPERTY(JellyfinNative::MovieGridModel *nextUpItems READ nextUpItems CONSTANT)
    Q_PROPERTY(QVariantList latestLibraryRows READ latestLibraryRows NOTIFY latestLibraryRowsChanged)
    Q_PROPERTY(JellyfinNative::MovieGridModel *searchResults READ searchResults CONSTANT)
    Q_PROPERTY(JellyfinNative::MovieGridModel *searchSuggestions READ searchSuggestions CONSTANT)
    Q_PROPERTY(bool searchSuggestionsBusy READ searchSuggestionsBusy NOTIFY searchSuggestionsChanged)
    Q_PROPERTY(bool currentItemsLoadingMore READ currentItemsLoadingMore NOTIFY currentItemsPagingChanged)
    Q_PROPERTY(bool currentItemsHasMore READ currentItemsHasMore NOTIFY currentItemsPagingChanged)
    Q_PROPERTY(int currentItemsTotalCount READ currentItemsTotalCount NOTIFY currentItemsPagingChanged)
    Q_PROPERTY(JellyfinNative::MovieGridModel *detailSeasons READ detailSeasons CONSTANT)
    Q_PROPERTY(JellyfinNative::MovieGridModel *detailSimilarItems READ detailSimilarItems CONSTANT)
    Q_PROPERTY(bool detailRowsBusy READ detailRowsBusy NOTIFY detailRowsChanged)
    Q_PROPERTY(JellyfinNative::MovieGridModel *personItems READ personItems CONSTANT)
    Q_PROPERTY(bool personItemsBusy READ personItemsBusy NOTIFY personItemsChanged)
    Q_PROPERTY(bool searchBusy READ searchBusy NOTIFY searchChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery NOTIFY searchChanged)
    Q_PROPERTY(JellyfinNative::PlayerController *player READ player CONSTANT)
    Q_PROPERTY(JellyfinNative::SyncPlayController *syncPlay READ syncPlay CONSTANT)

public:
    AppController(DatabaseManager *database,
                  DiscoveryController *discovery,
                  JellyfinApiFacade *api,
                  PlayerController *player,
                  QObject *parent = nullptr);

    QString page() const;
    QString serverUrl() const;
    QString username() const;
    QString password() const;
    bool busy() const;
    QString busyText() const;
    QString errorText() const;
    QString quickConnectCode() const;
    QString quickConnectStatus() const;
    bool quickConnectActive() const;
    bool hasDefaultProfile() const;
    QString currentLibraryName() const;
    QString currentContentLabel() const;
    QString currentViewKind() const;
    QString currentLibraryId() const;
    QString currentLibraryCollectionType() const;
    QVariantMap libraryQuery() const;
    QVariantMap libraryFilterOptions() const;
    int libraryFilterActiveCount() const;
    bool settingsVisible() const;
    bool nightModeEnabled() const;
    bool toneMappingVisualizationEnabled() const;
    int maxStreamingBitrateMbps() const;
    bool preferRemux() const;
    int audioDelayMs() const;
    QString audioOutputMode() const;
    QStringList subtitleLanguageOptions() const;
    int subtitleLanguageIndex() const;
    QString subtitleMode() const;
    QString subtitleBurnIn() const;
    bool subtitleRenderPgs() const;
    bool subtitleAlwaysBurnIn() const;
    QString subtitleStyling() const;
    QString subtitleTextSize() const;
    QString subtitleTextWeight() const;
    QString subtitleFont() const;
    QString subtitleTextColor() const;
    QString subtitleDropShadow() const;
    int subtitleVerticalPosition() const;
    QString redButtonAction() const;
    QString greenButtonAction() const;
    QString yellowButtonAction() const;
    QString blueButtonAction() const;
    QStringList availableButtonActions() const;

    DiscoveredServerModel *discoveredServers();
    LibraryListModel *libraries();
    MovieGridModel *movies();
    MovieGridModel *resumeItems();
    MovieGridModel *nextUpItems();
    QVariantList latestLibraryRows() const;
    MovieGridModel *searchResults();
    MovieGridModel *searchSuggestions();
    bool searchSuggestionsBusy() const;
    bool currentItemsLoadingMore() const;
    bool currentItemsHasMore() const;
    int currentItemsTotalCount() const;
    MovieGridModel *detailSeasons();
    MovieGridModel *detailSimilarItems();
    MovieGridModel *personItems();
    bool detailRowsBusy() const;
    bool personItemsBusy() const;
    bool searchBusy() const;
    QString searchQuery() const;
    PlayerController *player();
    SyncPlayController *syncPlay();

    Q_INVOKABLE void initialize();
    void shutdown();
    Q_INVOKABLE void setServerUrl(const QString &serverUrl);
    Q_INVOKABLE void setUsername(const QString &username);
    Q_INVOKABLE void setPassword(const QString &password);
    Q_INVOKABLE void chooseDiscoveredServer(int index);
    Q_INVOKABLE void login();
    Q_INVOKABLE void useDefaultProfile();
    Q_INVOKABLE void switchUser();
    Q_INVOKABLE void logout();
    Q_INVOKABLE void startQuickConnect();
    Q_INVOKABLE void cancelQuickConnect();
    Q_INVOKABLE void goHome();
    Q_INVOKABLE void openLibrary(int index);
    Q_INVOKABLE void playMovie(int index, bool fromStart = false);
    Q_INVOKABLE void playResumeItem(int index, bool fromStart = false);
    Q_INVOKABLE void playNextUpItem(int index, bool fromStart = false);
    Q_INVOKABLE QObject *latestLibraryItems(int rowIndex);
    Q_INVOKABLE void playLatestLibraryItem(int rowIndex, int itemIndex, bool fromStart = false);
    Q_INVOKABLE void openSeriesById(const QString &seriesId, const QString &seriesName);
    Q_INVOKABLE void openSeasonById(const QString &seriesId, const QString &seasonId, const QString &seasonName);
    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void clearSearch();
    Q_INVOKABLE void playSearchResult(int index, bool fromStart = false);
    Q_INVOKABLE void loadSearchSuggestions();
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
    Q_INVOKABLE void loadDetailRows(const QString &itemId, const QString &itemType);
    Q_INVOKABLE void openDetailSeason(int index);
    Q_INVOKABLE void openGenre(const QString &genre);
    Q_INVOKABLE void openStudio(const QString &studio);
    Q_INVOKABLE void playDetailSimilarItem(int index, bool fromStart = false);
    Q_INVOKABLE void loadPersonItems(const QString &personId);
    Q_INVOKABLE void playPersonItem(int index, bool fromStart = false);
    Q_INVOKABLE void setFavorite(const QString &itemId, bool favorite);
    Q_INVOKABLE void setPlayed(const QString &itemId, bool played);
    Q_INVOKABLE void clearProgress(const QString &itemId);
    Q_INVOKABLE void back();
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void openSettings();
    Q_INVOKABLE void closeSettings();
    Q_INVOKABLE void toggleNightMode();
    Q_INVOKABLE void setNightModeEnabled(bool enabled);
    Q_INVOKABLE void setToneMappingVisualizationEnabled(bool enabled);
    Q_INVOKABLE void setMaxStreamingBitrateMbps(int bitrateMbps);
    Q_INVOKABLE void setPreferRemux(bool enabled);
    Q_INVOKABLE void setAudioDelayMs(int delayMs);
    Q_INVOKABLE void setAudioOutputMode(const QString &mode);
    Q_INVOKABLE void setSubtitleLanguageIndex(int index);
    Q_INVOKABLE void setSubtitleMode(const QString &mode);
    Q_INVOKABLE void setSubtitleBurnIn(const QString &mode);
    Q_INVOKABLE void setSubtitleRenderPgs(bool enabled);
    Q_INVOKABLE void setSubtitleAlwaysBurnIn(bool enabled);
    Q_INVOKABLE void setSubtitleStyling(const QString &styling);
    Q_INVOKABLE void setSubtitleTextSize(const QString &size);
    Q_INVOKABLE void setSubtitleTextWeight(const QString &weight);
    Q_INVOKABLE void setSubtitleFont(const QString &font);
    Q_INVOKABLE void setSubtitleTextColor(const QString &color);
    Q_INVOKABLE void setSubtitleDropShadow(const QString &shadow);
    Q_INVOKABLE void setSubtitleVerticalPosition(int position);
    Q_INVOKABLE void setRedButtonAction(const QString &action);
    Q_INVOKABLE void setGreenButtonAction(const QString &action);
    Q_INVOKABLE void setYellowButtonAction(const QString &action);
    Q_INVOKABLE void setBlueButtonAction(const QString &action);
    Q_INVOKABLE QString buttonActionLabel(const QString &action) const;

signals:
    void pageChanged();
    void serverUrlChanged();
    void usernameChanged();
    void passwordChanged();
    void busyChanged();
    void errorTextChanged();
    void quickConnectChanged();
    void defaultProfileChanged();
    void currentLibraryNameChanged();
    void settingsVisibleChanged();
    void nightModeEnabledChanged();
    void toneMappingVisualizationEnabledChanged();
    void playbackPreferencesChanged();
    void audioDelayMsChanged();
    void audioOutputModeChanged();
    void subtitleSettingsChanged();
    void buttonRemapChanged();
    void searchChanged();
    void searchSuggestionsChanged();
    void detailRowsChanged();
    void latestLibraryRowsChanged();
    void currentItemsPagingChanged();
    void libraryQueryChanged();
    void libraryFilterOptionsChanged();
    void personItemsChanged();
    void itemFavoriteChanged(const QString &itemId, bool favorite);
    void itemPlayedChanged(const QString &itemId, bool played);

private:
    void setPage(const QString &page);
    void setBusy(bool busy, const QString &busyText = {});
    void setErrorText(const QString &errorText);
    void resetApplicationState();
    void applyDiscoveredServersCache();
    void loadLibraries();
    void refreshHomeRows();
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
