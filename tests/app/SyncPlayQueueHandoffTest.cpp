#include "app/SyncPlayController.h"

#include "TestMain.h"

#include <cstdlib>
#include <iostream>

using namespace JellyfinNative;

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << message << '\n';
    std::exit(1);
}

} // namespace

JELLYFIN_TEST_MAIN("sync-play-queue-handoff")
{
    SyncPlayQueueHandoff handoff;
    handoff.arm();
    require(!handoff.canSend(false, false, true, true),
        "an old loaded session must not consume an unpause before the new queue update");

    handoff.observeQueueUpdate();
    require(!handoff.canSend(true, false, true, true), "queue resolution must block the unpause request");
    require(!handoff.canSend(false, true, true, true), "playback startup must block the unpause request");
    require(!handoff.canSend(false, false, true, false), "an unloaded file must block the unpause request");
    require(handoff.canSend(false, false, true, true), "the new loaded queue item should release the unpause request");

    handoff.cancel();
    require(!handoff.canSend(false, false, true, true), "a consumed or cancelled request must not be sent twice");

    SyncPlaySeekResume seekResume;
    seekResume.arm(true);
    require(!seekResume.takeWhenReady(QStringLiteral("Waiting"), QStringLiteral("Ready")),
        "seek resume must wait for the group to finish buffering");
    require(!seekResume.takeWhenReady(QStringLiteral("Paused"), QStringLiteral("Pause")),
        "an ordinary pause must not resume playback");
    require(seekResume.takeWhenReady(QStringLiteral("Paused"), QStringLiteral("Ready")),
        "the initiating client should unpause only after every participant is ready");
    require(!seekResume.takeWhenReady(QStringLiteral("Paused"), QStringLiteral("Ready")),
        "a completed seek must request unpause only once");

    seekResume.arm(false);
    require(!seekResume.takeWhenReady(QStringLiteral("Paused"), QStringLiteral("Ready")),
        "seeking a paused group must leave it paused");
    return 0;
}
