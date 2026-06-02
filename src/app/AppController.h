#pragma once

#include "../cache/DatabaseManager.h"
#include "../common/JellyfinTypes.h"
#include "../discovery/DiscoveryController.h"
#include "../models/DiscoveredServerModel.h"
#include "../models/LibraryListModel.h"
#include "../models/MovieGridModel.h"
#include "../player/PlayerController.h"
#include "SyncPlayController.h"

#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantList>

#include <memory>

namespace JellyfinNative {

class JellyfinApiFacade;
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
    Q_PROPERTY(QString currentLibraryName READ currentLibraryName NOTIFY currentLibraryNameChanged)
    Q_PROPERTY(QString currentContentLabel READ currentContentLabel NOTIFY currentLibraryNameChanged)
    Q_PROPERTY(bool settingsVisible READ settingsVisible NOTIFY settingsVisibleChanged)
    Q_PROPERTY(bool nightModeEnabled READ nightModeEnabled WRITE setNightModeEnabled NOTIFY nightModeEnabledChanged)
    Q_PROPERTY(int audioDelayMs READ audioDelayMs WRITE setAudioDelayMs NOTIFY audioDelayMsChanged)
    Q_PROPERTY(QString audioOutputMode READ audioOutputMode WRITE setAudioOutputMode NOTIFY audioOutputModeChanged)
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
    Q_PROPERTY(JellyfinNative::MovieGridModel *latestItems READ latestItems CONSTANT)
    Q_PROPERTY(QVariantList latestLibraryRows READ latestLibraryRows NOTIFY latestLibraryRowsChanged)
    Q_PROPERTY(JellyfinNative::MovieGridModel *searchResults READ searchResults CONSTANT)
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
    QString currentLibraryName() const;
    QString currentContentLabel() const;
    bool settingsVisible() const;
    bool nightModeEnabled() const;
    int audioDelayMs() const;
    QString audioOutputMode() const;
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
    MovieGridModel *latestItems();
    QVariantList latestLibraryRows() const;
    MovieGridModel *searchResults();
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
    Q_INVOKABLE void logout();
    Q_INVOKABLE void startQuickConnect();
    Q_INVOKABLE void cancelQuickConnect();
    Q_INVOKABLE void goHome();
    Q_INVOKABLE void openLibrary(int index);
    Q_INVOKABLE void playMovie(int index);
    Q_INVOKABLE void playResumeItem(int index);
    Q_INVOKABLE void playNextUpItem(int index);
    Q_INVOKABLE void playLatestItem(int index);
    Q_INVOKABLE QObject *latestLibraryItems(int rowIndex);
    Q_INVOKABLE void playLatestLibraryItem(int rowIndex, int itemIndex);
    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void clearSearch();
    Q_INVOKABLE void playSearchResult(int index);
    Q_INVOKABLE void loadDetailRows(const QString &itemId, const QString &itemType);
    Q_INVOKABLE void openDetailSeason(int index);
    Q_INVOKABLE void playDetailSimilarItem(int index);
    Q_INVOKABLE void loadPersonItems(const QString &personId);
    Q_INVOKABLE void playPersonItem(int index);
    Q_INVOKABLE void setFavorite(const QString &itemId, bool favorite);
    Q_INVOKABLE void setPlayed(const QString &itemId, bool played);
    Q_INVOKABLE void back();
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void openSettings();
    Q_INVOKABLE void closeSettings();
    Q_INVOKABLE void toggleNightMode();
    Q_INVOKABLE void setNightModeEnabled(bool enabled);
    Q_INVOKABLE void setAudioDelayMs(int delayMs);
    Q_INVOKABLE void setAudioOutputMode(const QString &mode);
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
    void currentLibraryNameChanged();
    void settingsVisibleChanged();
    void nightModeEnabledChanged();
    void audioDelayMsChanged();
    void audioOutputModeChanged();
    void buttonRemapChanged();
    void searchChanged();
    void detailRowsChanged();
    void latestLibraryRowsChanged();
    void personItemsChanged();
    void itemFavoriteChanged(const QString &itemId, bool favorite);
    void itemPlayedChanged(const QString &itemId, bool played);

private:
    struct LatestLibrarySection {
        int order = 0;
        LibraryItem library;
        std::unique_ptr<MovieGridModel> model;
    };

    void setPage(const QString &page);
    void setBusy(bool busy, const QString &busyText = {});
    void setErrorText(const QString &errorText);
    void applyDiscoveredServersCache();
    void applyLibrariesCache();
    void applyMoviesCache(const QString &libraryId);
    void loadLibraries();
    void refreshHomeRows();
    void schedulePostPlaybackRefresh();
    void refreshCurrentItems(const QString &viewKind,
                             const QString &libraryId,
                             const QString &seriesId,
                             const QString &seasonId);
    void applyPlaybackPosition(const QString &itemId, qint64 positionTicks);
    void applyFavoriteState(const QString &itemId, bool favorite);
    void applyPlayedState(const QString &itemId, bool played);
    void clearLatestLibraryRows();
    void addLatestLibraryRow(int generation, int order, const LibraryItem &library, const std::vector<MovieItem> &items);
    void handleHomeRowLoaded(int generation);
    void scheduleLibraryPrefetch(int generation);
    void startNextLibraryPrefetch();
    void pollQuickConnect();
    void prefetchMoviePosters(const std::vector<MovieItem> &movies);
    void finishDetailRowLoad(int generation);
    void setCurrentItems(const std::vector<MovieItem> &items, const QString &cacheKey = {});
    void openSeries(const MovieItem &series);
    void openSeason(const MovieItem &season);
    void playMediaItem(const MovieItem &item);

    DatabaseManager *m_database = nullptr;
    DiscoveryController *m_discovery = nullptr;
    JellyfinApiFacade *m_api = nullptr;
    PlayerController *m_player = nullptr;
    SyncPlayController *m_syncPlay = nullptr;
    DiscoveredServerModel m_discoveredServers;
    LibraryListModel m_libraries;
    MovieGridModel m_movies;
    MovieGridModel m_resumeItems;
    MovieGridModel m_nextUpItems;
    MovieGridModel m_latestItems;
    MovieGridModel m_searchResults;
    MovieGridModel m_detailSeasons;
    MovieGridModel m_detailSimilarItems;
    MovieGridModel m_personItems;
    std::vector<LatestLibrarySection> m_latestLibrarySections;
    QTimer m_quickConnectTimer;
    QTimer m_libraryPrefetchTimer;
    QString m_page = QStringLiteral("login");
    QString m_serverUrl;
    QString m_username;
    QString m_password;
    bool m_busy = false;
    QString m_busyText;
    QString m_errorText;
    QString m_quickConnectCode;
    QString m_quickConnectStatus;
    QString m_quickConnectSecret;
    int m_quickConnectPollAttempts = 0;
    int m_quickConnectPollErrors = 0;
    QString m_currentLibraryId;
    QString m_currentLibraryName;
    QString m_currentContentLabel = QStringLiteral("Movies");
    QString m_currentViewKind;
    QString m_currentSeriesId;
    QString m_currentSeriesName;
    QString m_currentSeasonId;
    bool m_settingsVisible = false;
    bool m_nightModeEnabled = false;
    int m_audioDelayMs = 0;
    QString m_audioOutputMode = QStringLiteral("alsa");
    QString m_redButtonAction = QStringLiteral("none");
    QString m_greenButtonAction = QStringLiteral("skipBackAndEnableSubs");
    QString m_yellowButtonAction = QStringLiteral("none");
    QString m_blueButtonAction = QStringLiteral("none");
    QString m_searchQuery;
    bool m_searchBusy = false;
    int m_searchGeneration = 0;
    bool m_detailRowsBusy = false;
    int m_detailRowsGeneration = 0;
    int m_detailRowsPending = 0;
    bool m_personItemsBusy = false;
    int m_personItemsGeneration = 0;
    int m_libraryLoadGeneration = 0;
    int m_homeLoadGeneration = 0;
    int m_homeLoadsPending = 0;
    int m_libraryPrefetchGeneration = 0;
    int m_libraryPrefetchIndex = 0;
    bool m_libraryPrefetchActive = false;
    bool m_shuttingDown = false;
    std::vector<LibraryItem> m_libraryPrefetchQueue;
    QSet<QString> m_prefetchedLibraryKeys;
};

} // namespace JellyfinNative
