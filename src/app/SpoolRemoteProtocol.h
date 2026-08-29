#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVariantMap>

#include <optional>

namespace JellyfinNative::SpoolRemoteProtocol {

// Jellyfin validates a GeneralCommand's name against its own enum and carries
// its arguments as a string-to-string map, so every Spool-to-Spool message
// rides inside SendString with an empty payload: a client that does not know
// Spool is told to type nothing, which is harmless.
QString envelopeCommand();

namespace Command {
    inline const QString SeekPreview = QStringLiteral("SeekPreview");
    inline const QString SyncPlayJoin = QStringLiteral("SyncPlayJoin");
    inline const QString LinkOffer = QStringLiteral("LinkOffer");
    inline const QString LinkAnswer = QStringLiteral("LinkAnswer");
}

// One wire form for every command. Adding a message means naming it and
// listing its fields; the envelope, the version gate, the string-map rules and
// both transports stay exactly as they are.
struct Message {
    QString command;
    QVariantMap fields;

    QString text(const QString& key) const;
    std::optional<qint64> number(const QString& key) const;
    std::optional<bool> flag(const QString& key) const;
    bool isValid() const
    {
        return !command.isEmpty();
    }
};

QJsonObject encode(const Message& message);
std::optional<Message> decode(const QString& envelope, const QJsonObject& arguments);

// The same message as a self-contained blob, for a transport that carries
// bytes rather than Jellyfin command arguments.
QByteArray serialize(const Message& message);
std::optional<Message> deserialize(const QByteArray& payload);

// Typed views. Each one names only its own fields, because everything they
// would otherwise have in common already lives above.
struct SeekPreview {
    QString itemId;
    qint64 positionTicks = 0;
    bool active = false;
};
Message seekPreviewMessage(const QString& itemId, qint64 positionTicks, bool active);
std::optional<SeekPreview> seekPreview(const Message& message);

struct SyncPlayJoin {
    QString groupId;
};
Message syncPlayJoinMessage(const QString& groupId);
std::optional<SyncPlayJoin> syncPlayJoin(const Message& message);

} // namespace JellyfinNative::SpoolRemoteProtocol
