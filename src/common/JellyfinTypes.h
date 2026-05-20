#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <exception>
#include <vector>

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
    QString imageUrl;
    QString imageTag;
};

struct MovieItem {
    QString id;
    QString title;
    QString overview;
    QString posterUrl;
    QString posterTag;
    QString itemType;
    QString seriesId;
    QString seriesName;
    QString subtitle;
    QString path;
    int year = 0;
    int seasonNumber = 0;
    int episodeNumber = 0;
    qint64 resumeTicks = 0;
    qint64 runtimeTicks = 0;
    bool playable = true;
};

struct AuthSession {
    QString userId;
    QString userName;
    QString accessToken;
    QString serverId;
};

struct MediaSegment {
    QString id;
    QString type; // "Intro", "Outro", "Recap", "Preview", "Commercial"
    qint64 startTicks = 0;
    qint64 endTicks = 0;
};

struct PlaybackSession {
    QString itemId;
    QString title;
    QString url;
    QString mediaSourceId;
    QString playSessionId;
    QString container;
    qint64 startTimeTicks = 0;
    std::vector<MediaSegment> segments;
};

QJsonObject toJson(const DiscoveredServer &server);
QJsonObject toJson(const LibraryItem &library);
QJsonObject toJson(const MovieItem &movie);

DiscoveredServer discoveredServerFromJson(const QJsonObject &object);
LibraryItem libraryFromJson(const QJsonObject &object);
MovieItem movieFromJson(const QJsonObject &object);

QString exceptionMessage(const std::exception_ptr &exception);

} // namespace JellyfinNative
