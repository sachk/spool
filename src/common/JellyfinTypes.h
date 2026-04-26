#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <exception>

namespace JellyfinNative {

struct DiscoveredServer {
    QString id;
    QString name;
    QString address;
};

struct LibraryItem {
    QString id;
    QString name;
    QString collectionType;
};

struct MovieItem {
    QString id;
    QString title;
    QString overview;
    QString posterUrl;
    QString posterTag;
    QString itemType;
    QString seriesId;
    QString subtitle;
    int year = 0;
    int seasonNumber = 0;
    int episodeNumber = 0;
    bool playable = true;
};

struct AuthSession {
    QString userId;
    QString userName;
    QString accessToken;
    QString serverId;
};

struct PlaybackSession {
    QString itemId;
    QString title;
    QString url;
    QString mediaSourceId;
    QString playSessionId;
    QString container;
};

QJsonObject toJson(const DiscoveredServer &server);
QJsonObject toJson(const LibraryItem &library);
QJsonObject toJson(const MovieItem &movie);

DiscoveredServer discoveredServerFromJson(const QJsonObject &object);
LibraryItem libraryFromJson(const QJsonObject &object);
MovieItem movieFromJson(const QJsonObject &object);

QString exceptionMessage(const std::exception_ptr &exception);

} // namespace JellyfinNative
