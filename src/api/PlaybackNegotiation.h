#pragma once
#include "../common/JellyfinTypes.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace JellyfinNative {

struct PlaybackSelection {
    QJsonObject source;
    QString playMethod;
};

class PlaybackNegotiation final {
public:
    static PlaybackSelection selectSource(const QJsonArray& mediaSources, bool preferRemux);
    static QString buildUrl(const QString& serverUrl, const QString& itemId, const QString& accessToken,
        const PlaybackSelection& selection);
    static TrickplayInfo selectTrickplay(
        const QJsonObject& trickplay, const QString& mediaSourceId, int preferredWidth);
    static QJsonObject buildDeviceProfile(
        qint64 maxStreamingBitrate, const QStringList& videoCodecs = {}, bool restrictVideoCodecs = false);
};

} // namespace JellyfinNative
