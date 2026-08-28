#include "app/RemoteControlController.h"

#include "TestMain.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>

#include <cstdlib>
#include <iostream>

using JellyfinNative::RemoteControlController;

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
}

QJsonObject session(QString id, QString name, bool controllable, QString title = {})
{
    QJsonObject session {
        { QStringLiteral("Id"), id },
        { QStringLiteral("DeviceId"), QStringLiteral("device-") + id },
        { QStringLiteral("DeviceName"), name },
        { QStringLiteral("DeviceType"), QStringLiteral("TV") },
        { QStringLiteral("Client"), QStringLiteral("Spool for Jellyfin") },
        { QStringLiteral("UserName"), QStringLiteral("Sacha") },
        { QStringLiteral("Capabilities"),
            QJsonObject {
                { QStringLiteral("SupportsMediaControl"), controllable },
                { QStringLiteral("SupportedCommands"),
                    QJsonArray { QStringLiteral("SetVolume"), QStringLiteral("MoveUp") } },
            } },
    };
    if (!title.isEmpty()) {
        session.insert(QStringLiteral("NowPlayingItem"),
            QJsonObject {
                { QStringLiteral("Id"), QStringLiteral("item-1") },
                { QStringLiteral("Name"), title },
                { QStringLiteral("Type"), QStringLiteral("Movie") },
                { QStringLiteral("RunTimeTicks"), 1'200'000'000 },
            });
        session.insert(QStringLiteral("PlayState"),
            QJsonObject {
                { QStringLiteral("PositionTicks"), 300'000'000 },
                { QStringLiteral("IsPaused"), true },
                { QStringLiteral("VolumeLevel"), 37 },
                { QStringLiteral("IsMuted"), false },
            });
    }
    return session;
}

} // namespace

JELLYFIN_TEST_MAIN("remote-control-controller")
{
    QCoreApplication app(argc, argv);
    RemoteControlController remote(nullptr);

    remote.applySessions(QJsonArray {
        session(QStringLiteral("hidden"), QStringLiteral("Phone"), false),
        session(QStringLiteral("living-room"), QStringLiteral("Living Room"), true, QStringLiteral("Arrival")),
    });
    require(remote.targets().size() == 1, "non-controllable session was exposed as a Cast target");
    require(remote.targets().front().toMap().value(QStringLiteral("deviceName")).toString()
            == QStringLiteral("Living Room"),
        "controllable target metadata was not exposed");

    remote.selectTarget(QStringLiteral("living-room"));
    require(remote.targetSelected(), "available target was not selected");
    require(remote.selectedTargetName() == QStringLiteral("Living Room"), "selected target name was not retained");
    require(remote.nowPlayingItem().value(QStringLiteral("title")).toString() == QStringLiteral("Arrival"),
        "selected target now-playing item was not normalized");
    require(remote.positionTicks() == 300'000'000, "selected target position was not applied");
    require(remote.runtimeTicks() == 1'200'000'000, "selected target duration was not applied");
    require(remote.volume() == 37, "selected target volume was not applied");
    require(remote.supports(QStringLiteral("MoveUp")), "selected target commands were not applied");

    remote.applySessions(QJsonArray {});
    require(!remote.targetSelected(), "disappeared target remained selected");
    require(remote.nowPlayingItem().isEmpty(), "disappeared target retained stale playback state");
    return EXIT_SUCCESS;
}
