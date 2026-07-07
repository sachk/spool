#include "player/PlayQueueController.h"

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
    movie.playable = true;
    return movie;
}

QString idAt(const PlayQueueController &queue, int index)
{
    return queue.get(index).value(QStringLiteral("movieId")).toString();
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    PlayQueueController queue;
    std::vector<MovieItem> entries = {
        item(QStringLiteral("a"), QStringLiteral("Episode A"), QStringLiteral("pl-a")),
        item(QStringLiteral("b"), QStringLiteral("Episode B"), QStringLiteral("pl-b")),
        item(QStringLiteral("c"), QStringLiteral("Episode C"), QStringLiteral("pl-c")),
    };

    require(queue.playNow(entries, 1), "playNow should accept a playable item vector");
    require(queue.count() == 3, "playNow should keep all playable items");
    require(queue.currentIndex() == 1, "playNow should start at requested natural index");
    require(queue.currentItem().id == QStringLiteral("b"), "current item should be requested item");
    require(queue.canGoPrevious() && queue.canGoNext(), "middle current item should allow both directions");

    const std::vector<PlaybackQueueItem> reported = queue.nowPlayingQueue();
    require(reported.size() == 3, "reported queue should include all queued entries");
    require(reported[1].itemId == QStringLiteral("b") &&
            reported[1].playlistItemId == QStringLiteral("pl-b"),
            "reported queue should preserve item and playlist ids");

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

    queue.setShuffled(true);
    require(queue.shuffled(), "setShuffled(true) should enable shuffled mode");
    require(queue.currentItem().id == QStringLiteral("c"), "shuffle should keep current item active");
    queue.setShuffled(false);
    require(!queue.shuffled(), "setShuffled(false) should disable shuffled mode");
    require(queue.currentIndex() == 2, "unshuffle should restore current natural index");
    require(idAt(queue, 0) == QStringLiteral("a") && idAt(queue, 4) == QStringLiteral("e"),
            "unshuffle should preserve natural row order");

    queue.removeAt(queue.currentIndex());
    require(queue.count() == 4, "removeAt should remove one row");
    require(queue.currentItem().id == QStringLiteral("d"), "removing current should advance to next queue entry");
    queue.removeAt(queue.currentIndex());
    require(queue.currentItem().id == QStringLiteral("e"), "removing current again should keep a valid successor");
    queue.clear();
    require(queue.count() == 0 && queue.currentIndex() == -1, "clear should empty queue and reset current index");

    MovieItem blocked = item(QStringLiteral("x"), QStringLiteral("Blocked"));
    blocked.playable = false;
    require(!queue.playNow(blocked), "unplayable item should not become the queue");

    return EXIT_SUCCESS;
}
