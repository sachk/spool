#include "app/NavigationState.h"

#include <QDebug>

#include <cstdlib>

using JellyfinNative::BrowseDescriptor;
using JellyfinNative::BrowseKind;
using JellyfinNative::LibraryItem;
using JellyfinNative::MovieItem;
using JellyfinNative::NavigationState;

namespace {

void require(bool condition, const char *message) {
  if (condition)
    return;
  qCritical() << message;
  std::exit(EXIT_FAILURE);
}

void requireDescriptor(const BrowseDescriptor &descriptor, BrowseKind kind,
                       const QString &id, const QString &name,
                       const QString &collectionType,
                       const QString &seriesId, const QString &seasonId,
                       const char *message) {
  require(descriptor.kind == kind, message);
  require(descriptor.id == id, message);
  require(descriptor.name == name, message);
  require(descriptor.collectionType == collectionType, message);
  require(descriptor.seriesId == seriesId, message);
  require(descriptor.seasonId == seasonId, message);
}

} // namespace

int main() {
  NavigationState navigation;
  require(navigation.page() == QStringLiteral("login"),
          "initial route was not login");

  LibraryItem library;
  library.id = QStringLiteral("movies");
  library.name = QStringLiteral("Movies");
  library.collectionType = QStringLiteral("movies");
  const QVariantMap defaults{
      {QStringLiteral("sortBy"), QStringLiteral("SortName")}};
  navigation.enterLibrary(library, QStringLiteral("Movies"), defaults);
  require(navigation.libraryId() == library.id,
          "library context was not retained");
  require(navigation.query() == defaults, "default query was not applied");
  requireDescriptor(navigation.browseDescriptor(), BrowseKind::Library,
                    library.id, library.name, library.collectionType,
                    QString(), QString(),
                    "library transition did not create a library descriptor");

  QVariantMap changed = defaults;
  changed.insert(QStringLiteral("sortOrder"), QStringLiteral("Descending"));
  require(navigation.setQuery(changed), "changed query was rejected");

  MovieItem series;
  series.id = QStringLiteral("series");
  series.title = QStringLiteral("Series");
  navigation.enterSeries(series);
  require(navigation.viewKind() == QStringLiteral("seasons"),
          "series transition did not select seasons");

  requireDescriptor(navigation.browseDescriptor(), BrowseKind::SeriesSeasons,
                    series.id, series.title, QString(), series.id, QString(),
                    "series transition did not create a series descriptor");
  MovieItem season;
  season.id = QStringLiteral("season");
  season.title = QStringLiteral("Season 1");
  season.itemType = QStringLiteral("Season");
  navigation.enterSeason(series.id, season);
  require(navigation.viewKind() == QStringLiteral("episodes"),
          "season transition did not select episodes");
  require(navigation.seriesId() == series.id,
          "season transition lost the series");

  requireDescriptor(navigation.browseDescriptor(), BrowseKind::SeasonEpisodes,
                    season.id, season.title, QString(), series.id, season.id,
                    "season transition did not create a season descriptor");
  navigation.enterLibrary(library, QStringLiteral("Movies"), defaults);
  require(navigation.query() == changed,
          "library-specific query was not restored");

  navigation.enterNamedCollection(QStringLiteral("genre"),
                                  QStringLiteral("Drama"));
  require(navigation.libraryId().isEmpty(),
          "named collection retained a library id");
  require(navigation.title() == QStringLiteral("Drama"),
          "named collection title was not applied");
  requireDescriptor(navigation.browseDescriptor(), BrowseKind::Genre, QString(),
                    QStringLiteral("Drama"), QString(), QString(), QString(),
                    "genre transition did not create a genre descriptor");

  navigation.enterNamedCollection(QStringLiteral("studio"),
                                  QStringLiteral("Warner Bros"));
  requireDescriptor(navigation.browseDescriptor(), BrowseKind::Studio,
                    QString(), QStringLiteral("Warner Bros"), QString(),
                    QString(), QString(),
                    "studio transition did not create a studio descriptor");

  MovieItem playlist;
  playlist.id = QStringLiteral("playlist");
  playlist.title = QStringLiteral("Playlist");
  navigation.enterPlaylist(playlist);
  requireDescriptor(navigation.browseDescriptor(), BrowseKind::Playlist,
                    playlist.id, playlist.title, QString(), QString(),
                    QString(),
                    "playlist transition did not create a playlist descriptor");

  MovieItem boxSet;
  boxSet.id = QStringLiteral("boxset");
  boxSet.title = QStringLiteral("Box Set");
  navigation.enterBoxSet(boxSet);
  requireDescriptor(navigation.browseDescriptor(), BrowseKind::BoxSet,
                    boxSet.id, boxSet.title, QString(), QString(), QString(),
                    "box set transition did not create a box set descriptor");

  MovieItem folder;
  folder.id = QStringLiteral("folder");
  folder.title = QStringLiteral("Folder");
  navigation.enterFolder(folder);
  requireDescriptor(navigation.browseDescriptor(), BrowseKind::FolderChildren,
                    folder.id, folder.title, QString(), QString(), QString(),
                    "folder transition did not create a folder descriptor");

  navigation.reset();
  require(navigation.page() == QStringLiteral("login"),
          "reset did not return to login");
  require(navigation.viewKind().isEmpty(), "reset retained view context");
  require(!navigation.browseDescriptor().isValid(),
          "reset retained a browse descriptor");
  return EXIT_SUCCESS;
}
