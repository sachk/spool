#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

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
    static QJsonObject buildDeviceProfile(qint64 maxStreamingBitrate);
};

} // namespace JellyfinNative
