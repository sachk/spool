#include "SpoolRemoteProtocol.h"

#include <QJsonDocument>

#include <algorithm>

namespace JellyfinNative::SpoolRemoteProtocol {
namespace {

    const QString kProtocolKey = QStringLiteral("SpoolProtocol");
    const QString kCommandKey = QStringLiteral("SpoolCommand");
    const QString kStringKey = QStringLiteral("String");
    const QString kProtocolVersion = QStringLiteral("1");

    bool isEnvelopeKey(const QString& key)
    {
        return key == kProtocolKey || key == kCommandKey || key == kStringKey;
    }

    // Jellyfin rejects a command whose arguments are not all strings, so every
    // field is written as one and read back with the type it was given.
    QString fieldToString(const QVariant& value)
    {
        if (value.typeId() == QMetaType::Bool)
            return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        return value.toString();
    }

} // namespace

QString envelopeCommand()
{
    return QStringLiteral("SendString");
}

QString Message::text(const QString& key) const
{
    return fields.value(key).toString();
}

std::optional<qint64> Message::number(const QString& key) const
{
    const auto field = fields.constFind(key);
    if (field == fields.constEnd())
        return std::nullopt;
    bool ok = false;
    const qint64 value = field->toString().toLongLong(&ok);
    if (!ok)
        return std::nullopt;
    return value;
}

std::optional<bool> Message::flag(const QString& key) const
{
    const QString value = fields.value(key).toString();
    if (value == QLatin1String("true"))
        return true;
    if (value == QLatin1String("false"))
        return false;
    return std::nullopt;
}

QJsonObject encode(const Message& message)
{
    QJsonObject arguments {
        { kProtocolKey, kProtocolVersion },
        { kCommandKey, message.command },
        { kStringKey, QString() },
    };
    for (auto field = message.fields.cbegin(); field != message.fields.cend(); ++field) {
        if (isEnvelopeKey(field.key()))
            continue;
        arguments.insert(field.key(), fieldToString(field.value()));
    }
    return arguments;
}

std::optional<Message> decode(const QString& envelope, const QJsonObject& arguments)
{
    if (envelope != envelopeCommand() || arguments.value(kProtocolKey).toString() != kProtocolVersion)
        return std::nullopt;

    Message message;
    message.command = arguments.value(kCommandKey).toString();
    if (message.command.isEmpty())
        return std::nullopt;

    for (auto argument = arguments.constBegin(); argument != arguments.constEnd(); ++argument) {
        // Anything that is not a string did not come from a Spool client, and
        // reading it as one would invent a value nobody sent.
        if (!argument.value().isString())
            return std::nullopt;
        if (isEnvelopeKey(argument.key()))
            continue;
        message.fields.insert(argument.key(), argument.value().toString());
    }
    return message;
}

QByteArray serialize(const Message& message)
{
    QJsonObject fields;
    for (auto field = message.fields.cbegin(); field != message.fields.cend(); ++field)
        fields.insert(field.key(), fieldToString(field.value()));
    const QJsonObject object {
        { QStringLiteral("c"), message.command },
        { QStringLiteral("f"), fields },
    };
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

std::optional<Message> deserialize(const QByteArray& payload)
{
    QJsonParseError error {};
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;

    const QJsonObject object = document.object();
    Message message;
    message.command = object.value(QStringLiteral("c")).toString();
    if (message.command.isEmpty())
        return std::nullopt;

    const QJsonObject fields = object.value(QStringLiteral("f")).toObject();
    for (auto field = fields.constBegin(); field != fields.constEnd(); ++field) {
        if (!field.value().isString())
            return std::nullopt;
        message.fields.insert(field.key(), field.value().toString());
    }
    return message;
}

Message seekPreviewMessage(const QString& itemId, qint64 positionTicks, bool active)
{
    return Message { Command::SeekPreview,
        {
            { QStringLiteral("ItemId"), itemId },
            { QStringLiteral("PositionTicks"), QString::number(std::max<qint64>(0, positionTicks)) },
            { QStringLiteral("Active"), active },
        } };
}

std::optional<SeekPreview> seekPreview(const Message& message)
{
    if (message.command != Command::SeekPreview)
        return std::nullopt;
    const std::optional<qint64> positionTicks = message.number(QStringLiteral("PositionTicks"));
    const std::optional<bool> active = message.flag(QStringLiteral("Active"));
    if (!positionTicks || !active)
        return std::nullopt;

    SeekPreview preview;
    preview.itemId = message.text(QStringLiteral("ItemId"));
    preview.positionTicks = std::max<qint64>(0, *positionTicks);
    preview.active = *active;
    // A preview to show has to say what it is showing; one being torn down
    // does not.
    if (preview.active && preview.itemId.isEmpty())
        return std::nullopt;
    return preview;
}

Message syncPlayJoinMessage(const QString& groupId)
{
    return Message { Command::SyncPlayJoin, { { QStringLiteral("GroupId"), groupId } } };
}

std::optional<SyncPlayJoin> syncPlayJoin(const Message& message)
{
    if (message.command != Command::SyncPlayJoin)
        return std::nullopt;
    SyncPlayJoin join;
    join.groupId = message.text(QStringLiteral("GroupId"));
    if (join.groupId.isEmpty())
        return std::nullopt;
    return join;
}

} // namespace JellyfinNative::SpoolRemoteProtocol
