#include "app/BrowseSessionController.h"

#include <QDebug>

#include <cstdlib>
#include <utility>

using JellyfinNative::BrowseKind;
using JellyfinNative::BrowseSessionController;
using JellyfinNative::LibraryItem;
using JellyfinNative::MovieItem;
using JellyfinNative::PagedMovieItems;

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    qCritical() << message;
    std::exit(EXIT_FAILURE);
}

void requireBrowse(const BrowseSessionController &session, BrowseKind kind,
                   const QString &id, const QString &name, const char *message)
{
    const auto descriptor = session.descriptor();
    require(descriptor.kind == kind, message);
    require(descriptor.id == id, message);
    require(descriptor.name == name, message);
}

MovieItem item(QString id, QString title, QString type)
{
    MovieItem result;
    result.id = std::move(id);
    result.title = std::move(title);
    result.itemType = std::move(type);
    return result;
}

} // namespace

int main()
{
    BrowseSessionController session(nullptr);

    LibraryItem library;
    library.id = QStringLiteral("lib");
    library.name = QStringLiteral("Movies");
    library.collectionType = QStringLiteral("movies");
    QVariantMap defaultQuery{{QStringLiteral("sortBy"), QStringLiteral("SortName")}};
    session.enterLibrary(library, QStringLiteral("Movies"), defaultQuery);
    requireBrowse(session, BrowseKind::Library, QStringLiteral("lib"), QStringLiteral("Movies"),
                  "library descriptor set");
    require(session.libraryId() == QStringLiteral("lib"), "library id set");
    require(session.query().value(QStringLiteral("sortBy")).toString() == QStringLiteral("SortName"),
            "default library query used");

    QVariantMap dateQuery{{QStringLiteral("sortBy"), QStringLiteral("DateCreated")}};
    require(session.setQuery(dateQuery), "query changed");
    session.enterLibrary(library, QStringLiteral("Movies"), defaultQuery);
    require(session.query().value(QStringLiteral("sortBy")).toString() == QStringLiteral("DateCreated"),
            "library query retained by library id");

    const MovieItem series = item(QStringLiteral("series"), QStringLiteral("Show"), QStringLiteral("Series"));
    session.enterSeries(series);
    requireBrowse(session, BrowseKind::SeriesSeasons, QStringLiteral("series"), QStringLiteral("Show"),
                  "series descriptor set");
    require(session.viewKind() == QStringLiteral("seasons"), "series view kind set");

    MovieItem season = item(QStringLiteral("season"), QStringLiteral("Season 1"), QStringLiteral("Season"));
    session.enterSeason(series.id, season);
    require(session.descriptor().kind == BrowseKind::SeasonEpisodes, "season descriptor set");
    require(session.descriptor().seriesId == QStringLiteral("series"), "season series id set");
    require(session.descriptor().seasonId == QStringLiteral("season"), "season id set");

    session.enterNamedCollection(QStringLiteral("genre"), QStringLiteral("Drama"));
    require(session.descriptor().kind == BrowseKind::Genre, "genre descriptor set");
    session.enterNamedCollection(QStringLiteral("studio"), QStringLiteral("Studio"));
    require(session.descriptor().kind == BrowseKind::Studio, "studio descriptor set");

    session.enterPlaylist(item(QStringLiteral("playlist"), QStringLiteral("Queue"), QStringLiteral("Playlist")));
    requireBrowse(session, BrowseKind::Playlist, QStringLiteral("playlist"), QStringLiteral("Queue"),
                  "playlist descriptor set");
    session.enterBoxSet(item(QStringLiteral("box"), QStringLiteral("Collection"), QStringLiteral("BoxSet")));
    requireBrowse(session, BrowseKind::BoxSet, QStringLiteral("box"), QStringLiteral("Collection"),
                  "box set descriptor set");
    session.enterFolder(item(QStringLiteral("folder"), QStringLiteral("Folder"), QStringLiteral("Folder")));
    requireBrowse(session, BrowseKind::FolderChildren, QStringLiteral("folder"), QStringLiteral("Folder"),
                  "folder descriptor set");

    PagedMovieItems page;
    page.items = {item(QStringLiteral("movie"), QStringLiteral("Movie"), QStringLiteral("Movie"))};
    page.totalRecordCount = 3;
    page.startIndex = 0;
    page.limit = 1;
    session.setPage(page, QStringLiteral("cache"), false);
    require(session.items()->count() == 1, "page stored one item");
    require(session.hasMore(), "page tracks remaining items");
    require(session.nextStartIndex() == 1, "next start index updated");
    session.reset();
    require(!session.descriptor().isValid(), "reset clears descriptor");
    require(session.items()->count() == 0, "reset clears items");

    return EXIT_SUCCESS;
}
