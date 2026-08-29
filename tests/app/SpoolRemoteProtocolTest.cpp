#include "app/SpoolRemoteProtocol.h"

#include "TestMain.h"

#include <QJsonValue>

#include <cstdlib>
#include <iostream>

using namespace JellyfinNative;
using namespace JellyfinNative::SpoolRemoteProtocol;

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
}

} // namespace

JELLYFIN_TEST_MAIN("spool-remote-protocol")
{
    const Message preview = seekPreviewMessage(QStringLiteral("item-42"), 123'456'789, true);
    const QJsonObject arguments = encode(preview);

    require(envelopeCommand() == QStringLiteral("SendString"),
        "custom commands must use a Jellyfin GeneralCommand enum value");
    for (auto it = arguments.constBegin(); it != arguments.constEnd(); ++it)
        require(it.value().isString(), "Jellyfin command arguments must all be strings");
    require(arguments.value(QStringLiteral("String")).toString().isEmpty(),
        "the SendString compatibility payload must be harmless to non-Spool clients");

    // The envelope round-trips, and the typed view reads what was written.
    const auto decoded = decode(envelopeCommand(), arguments);
    require(decoded.has_value(), "a Spool envelope did not round-trip");
    require(decoded->command == Command::SeekPreview, "the command name changed");
    require(!decoded->fields.contains(QStringLiteral("SpoolCommand")),
        "envelope keys must not leak into a message's own fields");

    const auto roundTripped = seekPreview(*decoded);
    require(roundTripped.has_value(), "a seek preview payload did not round-trip");
    require(roundTripped->itemId == QStringLiteral("item-42"), "the seek preview item id changed");
    require(roundTripped->positionTicks == 123'456'789, "the seek preview position changed");
    require(roundTripped->active, "the active seek preview flag changed");

    const auto inactive
        = seekPreview(*decode(envelopeCommand(), encode(seekPreviewMessage(QStringLiteral("item-42"), -1, false))));
    require(inactive.has_value() && !inactive->active, "an inactive seek preview did not round-trip");
    require(inactive->positionTicks == 0, "negative seek preview positions must clamp to zero");

    require(
        !decode(QStringLiteral("SpoolSeekPreview"), arguments), "an unknown Jellyfin GeneralCommand name was accepted");

    QJsonObject malformed = arguments;
    malformed.insert(QStringLiteral("SpoolProtocol"), QStringLiteral("2"));
    require(!decode(envelopeCommand(), malformed), "an unsupported protocol version was accepted");

    malformed = arguments;
    malformed.insert(QStringLiteral("Active"), true);
    require(!decode(envelopeCommand(), malformed), "a non-string Jellyfin argument was accepted");

    malformed = arguments;
    malformed.insert(QStringLiteral("PositionTicks"), QStringLiteral("not-a-number"));
    require(!seekPreview(*decode(envelopeCommand(), malformed)), "a malformed seek position was accepted");

    malformed = arguments;
    malformed.remove(QStringLiteral("ItemId"));
    require(
        !seekPreview(*decode(envelopeCommand(), malformed)), "an active preview with nothing to preview was accepted");

    // The same message over a transport that carries bytes rather than
    // Jellyfin arguments must mean exactly the same thing.
    const auto viaBytes = deserialize(serialize(preview));
    require(viaBytes.has_value(), "a serialized message did not round-trip");
    const auto viaBytesPreview = seekPreview(*viaBytes);
    require(viaBytesPreview.has_value() && viaBytesPreview->itemId == QStringLiteral("item-42")
            && viaBytesPreview->positionTicks == 123'456'789 && viaBytesPreview->active,
        "the direct and server transports must carry the same message");
    require(!deserialize(QByteArrayLiteral("{\"c\":\"\"}")), "a message with no command was accepted");
    require(!deserialize(QByteArrayLiteral("not json")), "malformed bytes were accepted");

    const auto join = syncPlayJoin(*decode(envelopeCommand(), encode(syncPlayJoinMessage(QStringLiteral("group-7")))));
    require(
        join.has_value() && join->groupId == QStringLiteral("group-7"), "a SyncPlay join request did not round-trip");
    require(!syncPlayJoin(*decode(envelopeCommand(), encode(syncPlayJoinMessage(QString())))),
        "a join request naming no group was accepted");
    require(!syncPlayJoin(*decoded), "a seek preview was read as a join request");

    return EXIT_SUCCESS;
}
