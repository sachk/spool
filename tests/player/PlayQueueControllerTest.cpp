#include "player/PlayQueueController.h"

#include "TestMain.h"

#include <QCoreApplication>

#include <cstdlib>
#include <iostream>
#include <vector>

using namespace JellyfinNative;

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
}

MovieItem item(QString id, QString title, QString playlistItemId = {})
{
    MovieItem movie;
    movie.id = std::move(id);
    movie.title = std::move(title);
    movie.itemType = QStringLiteral("Episode");
    movie.playlistItemId = std::move(playlistItemId);
    return movie;
}

QString idAt(const PlayQueueController& queue, int index)
{
    return queue.get(index).value(QStringLiteral("movieId")).toString();
}

} // namespace

JELLYFIN_TEST_MAIN("play-queue-controller")
{
    QCoreApplication app(argc, argv);

    PlayQueueController queue;
    std::vector<MovieItem> entries = {
        item(QStringLiteral("a"), QStringLiteral("Episode A"), QStringLiteral("pl-a")),
        item(QStringLiteral("b"), QStringLiteral("Episode B"), QStringLiteral("pl-b")),
        item(QStringLiteral("c"), QStringLiteral("Episode C"), QStringLiteral("pl-c")),
    };
    entries[1].seriesName = QStringLiteral("Queue Show");
    entries[1].year = 2024;
    entries[1].seasonNumber = 3;
    entries[1].episodeNumber = 7;
    entries[1].title = QStringLiteral("Episode 7");
    entries[1].runtimeTicks = 1'000'000'000;
    entries[1].resumeTicks = 200'000'000;
    entries[1].album = QStringLiteral("Queue Album");
    entries[1].albumId = QStringLiteral("album-id");
    entries[1].albumArtist = QStringLiteral("Queue Artist");
    entries[1].albumPrimaryImageTag = QStringLiteral("album-tag");

    require(queue.playNow(entries, 1), "playNow should accept a playable item vector");
    require(queue.count() == 3, "playNow should keep all playable items");
    require(queue.currentIndex() == 1, "playNow should start at requested natural index");
    require(queue.currentItem().id == QStringLiteral("b"), "current item should be requested item");
    const QVariantMap episodeSnapshot = queue.get(1);
    require(episodeSnapshot.value(QStringLiteral("seriesName")).toString() == QStringLiteral("Queue Show")
            && episodeSnapshot.value(QStringLiteral("episodeCode")).toString() == QStringLiteral("S03E07")
            && episodeSnapshot.value(QStringLiteral("year")).toInt() == 2024,
        "queue snapshots should expose series context, episode code, and year");
    require(episodeSnapshot.value(QStringLiteral("genericEpisodeTitle")).toBool(),
        "queue snapshots should identify generic episode titles");
    require(episodeSnapshot.value(QStringLiteral("album")).toString() == QStringLiteral("Queue Album")
            && episodeSnapshot.value(QStringLiteral("albumId")).toString() == QStringLiteral("album-id")
            && episodeSnapshot.value(QStringLiteral("albumArtist")).toString() == QStringLiteral("Queue Artist")
            && episodeSnapshot.value(QStringLiteral("albumPrimaryImageTag")).toString() == QStringLiteral("album-tag"),
        "queue snapshots should preserve album metadata and artwork");
    require(queue.data(queue.index(1), PlayQueueController::YearRole).toInt() == 2024,
        "the queue model should expose the production year role");
    require(queue.canGoPrevious() && queue.canGoNext(), "middle current item should allow both directions");
    require(episodeSnapshot.value(QStringLiteral("resumeTicks")).toLongLong() == 200'000'000
            && episodeSnapshot.value(QStringLiteral("runtimeTicks")).toLongLong() == 1'000'000'000,
        "queue snapshots should expose playback progress");
    require(
        queue.updateResumeTicks(QStringLiteral("b"), 350'000'000), "queue progress should update the matching item");
    require(queue.currentItem().resumeTicks == 350'000'000
            && queue.get(1).value(QStringLiteral("resumeTicks")).toLongLong() == 350'000'000,
        "queue playback should use the current resume position");

    const std::vector<PlaybackQueueItem> reported = queue.nowPlayingQueue();
    require(reported.size() == 3, "reported queue should include all queued entries");
    require(reported[1].itemId == QStringLiteral("b") && reported[1].playlistItemId == QStringLiteral("pl-b"),
        "reported queue should preserve item and playlist ids");
    require(episodeSnapshot.value(QStringLiteral("displayTitle")).toString() == QStringLiteral("Episode 7"),
        "episode queue rows should use episode titles rather than series names");

    require(queue.playNext(item(QStringLiteral("d"), QStringLiteral("Episode D"), QStringLiteral("pl-d"))),
        "playNext should accept playable item");
    require(queue.addToQueue(item(QStringLiteral("e"), QStringLiteral("Episode E"), QStringLiteral("pl-e"))),
        "addToQueue should accept playable item");
    require(queue.count() == 5, "queue additions should append rows");
    require(queue.next(), "next should move to playNext item");
    require(queue.currentItem().id == QStringLiteral("d"), "playNext item should be first upcoming item");
    require(queue.next(), "next should move past playNext item");
    require(queue.currentItem().id == QStringLiteral("c"), "natural successor should follow inserted playNext item");
    require(queue.next(), "next should move to appended queue item");
    require(queue.currentItem().id == QStringLiteral("e"), "addToQueue item should play after existing upcoming items");
    require(queue.previous(), "previous should move to prior queue entry");
    require(queue.currentItem().id == QStringLiteral("c"), "previous should restore the prior queue entry");
    require(queue.playAt(0), "playAt should accept natural row indexes");
    require(queue.currentItem().id == QStringLiteral("a"), "playAt should switch to requested row");
    require(!queue.playAt(99), "playAt should reject out-of-range rows");
    require(queue.playAt(2), "playAt should restore the natural successor");

    require(queue.moveItem(2, 0), "queue rows should be reorderable");
    require(idAt(queue, 0) == QStringLiteral("c") && queue.currentIndex() == 0,
        "moving the current row should preserve the active item at its new index");
    require(queue.moveItem(2, 1), "non-current queue rows should be reorderable");
    require(idAt(queue, 1) == QStringLiteral("b") && queue.currentItem().id == QStringLiteral("c"),
        "reordering another row should preserve the active item");
    require(queue.removeItem(1), "non-current queue rows should be removable");
    require(
        queue.count() == 4 && idAt(queue, 1) == QStringLiteral("a"), "removing a queue row should close the model gap");
    require(!queue.removeItem(queue.currentIndex()), "the active queue row should not be removable");
    queue.setShuffled(true);
    require(queue.shuffled(), "setShuffled(true) should enable shuffled mode");
    require(queue.currentItem().id == QStringLiteral("c"), "shuffle should keep current item active");
    queue.setShuffled(false);
    require(!queue.shuffled(), "setShuffled(false) should disable shuffled mode");
    require(queue.currentIndex() == 0, "unshuffle should restore the reordered current index");
    require(idAt(queue, 0) == QStringLiteral("c") && idAt(queue, 3) == QStringLiteral("e"),
        "unshuffle should preserve the explicitly reordered rows");

    queue.clear();
    require(queue.count() == 0 && queue.currentIndex() == -1, "clear should empty queue and reset current index");

    MovieItem blocked = item(QStringLiteral("x"), QStringLiteral("Blocked"));
    blocked.itemType = QStringLiteral("Series");
    require(!queue.playNow(blocked), "unplayable item should not become the queue");

    return EXIT_SUCCESS;
}
