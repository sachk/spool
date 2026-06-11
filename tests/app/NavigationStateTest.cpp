#include "app/NavigationState.h"

#include <QDebug>

#include <cstdlib>

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

  QVariantMap changed = defaults;
  changed.insert(QStringLiteral("sortOrder"), QStringLiteral("Descending"));
  require(navigation.setQuery(changed), "changed query was rejected");

  MovieItem series;
  series.id = QStringLiteral("series");
  series.title = QStringLiteral("Series");
  navigation.enterSeries(series);
  require(navigation.viewKind() == QStringLiteral("seasons"),
          "series transition did not select seasons");

  MovieItem season;
  season.id = QStringLiteral("season");
  season.title = QStringLiteral("Season 1");
  season.itemType = QStringLiteral("Season");
  navigation.enterSeason(series.id, season);
  require(navigation.viewKind() == QStringLiteral("episodes"),
          "season transition did not select episodes");
  require(navigation.seriesId() == series.id,
          "season transition lost the series");

  navigation.enterLibrary(library, QStringLiteral("Movies"), defaults);
  require(navigation.query() == changed,
          "library-specific query was not restored");

  navigation.enterNamedCollection(QStringLiteral("genre"),
                                  QStringLiteral("Drama"));
  require(navigation.libraryId().isEmpty(),
          "named collection retained a library id");
  require(navigation.title() == QStringLiteral("Drama"),
          "named collection title was not applied");

  navigation.reset();
  require(navigation.page() == QStringLiteral("login"),
          "reset did not return to login");
  require(navigation.viewKind().isEmpty(), "reset retained view context");
  return EXIT_SUCCESS;
}
