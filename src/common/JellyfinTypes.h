#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

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

struct PersonItem {
    QString id;
    QString name;
    QString type;
    QString role;
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
    QString seriesPosterUrl;
    QString subtitle;
    QString path;
    int year = 0;
    int seasonNumber = 0;
    int episodeNumber = 0;
    qint64 resumeTicks = 0;
    qint64 runtimeTicks = 0;
    bool playable = true;
    bool favorite = false;
    bool played = false;
    QString backdropUrl;
    QString logoUrl;
    QString bannerUrl;
    QString thumbUrl;
    QStringList genres;
    QStringList tags;
    QStringList studios;
    QString officialRating;
    double communityRating = 0.0;
    double criticRating = 0.0;
    QString premiereDate;
    QString endDate;
    std::vector<PersonItem> people;
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

// Per-width trickplay manifest matching Jellyfin's
// item.Trickplay[mediaSourceId][width] structure.
struct TrickplayInfo {
    int width = 0;
    int height = 0;
    int tileWidth = 0;     // tiles across in one image
    int tileHeight = 0;    // tiles down in one image
    int thumbnailCount = 0;
    int intervalMs = 0;
    int bandwidth = 0;
};

struct SubtitlePreferences {
    QString language;
    QString mode = QStringLiteral("Default");
    QString burnInMode;
    bool renderPgs = false;
    bool alwaysBurnInWhenTranscoding = false;
    QString styling = QStringLiteral("Auto");
    QString textSize;
    QString textWeight = QStringLiteral("normal");
    QString font;
    QString textColor = QStringLiteral("#ffffff");
    QString dropShadow;
    QString textBackground = QStringLiteral("transparent");
    int verticalPosition = -3;
};

struct PagedMovieItems {
    std::vector<MovieItem> items;
    int totalRecordCount = 0;
    int startIndex = 0;
    int limit = 0;
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
    TrickplayInfo trickplay;
};

QJsonObject toJson(const DiscoveredServer &server);
QJsonObject toJson(const LibraryItem &library);
QJsonObject toJson(const PersonItem &person);
QJsonObject toJson(const MovieItem &movie);

DiscoveredServer discoveredServerFromJson(const QJsonObject &object);
LibraryItem libraryFromJson(const QJsonObject &object);
PersonItem personFromJson(const QJsonObject &object);
MovieItem movieFromJson(const QJsonObject &object);

QString exceptionMessage(const std::exception_ptr &exception);

} // namespace JellyfinNative
