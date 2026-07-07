#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

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

enum class BrowseKind {
    None,
    Library,
    FolderChildren,
    Person,
    Genre,
    Studio,
    SeriesSeasons,
    SeasonEpisodes,
    Playlist,
    BoxSet,
    ArtistAlbums,
};

struct BrowseDescriptor {
    BrowseKind kind = BrowseKind::None;
    QString id;
    QString name;
    QString collectionType;
    QString seriesId;
    QString seasonId;

    static BrowseDescriptor library(QString libraryId, QString collectionType, QString name = {});
    static BrowseDescriptor folderChildren(QString folderId, QString name = {});
    static BrowseDescriptor person(QString personId, QString name = {});
    static BrowseDescriptor genre(QString name);
    static BrowseDescriptor studio(QString name);
    static BrowseDescriptor seriesSeasons(QString seriesId, QString seriesName = {});
    static BrowseDescriptor seasonEpisodes(QString seriesId, QString seasonId = {}, QString seasonName = {});
    static BrowseDescriptor playlist(QString playlistId, QString name = {});
    static BrowseDescriptor boxSet(QString boxSetId, QString name = {});
    static BrowseDescriptor artistAlbums(QString artistId, QString artistName = {});

    bool isValid() const;
    QString kindKey() const;
    QString cacheKey(const QVariantMap &query = {}) const;
    QVariantMap toVariantMap() const;
    static BrowseDescriptor fromVariantMap(const QVariantMap &map);
};

struct PersonItem {
    QString id;
    QString name;
    QString type;
    QString role;
    QString imageUrl;
    QString imageTag;
};

struct MediaStreamInfo {
    int index = -1;
    QString type;
    QString codec;
    QString profile;
    QString displayTitle;
    QString title;
    QString language;
    QString pixelFormat;
    QString videoRange;
    QString colorPrimaries;
    QString colorTransfer;
    QString colorSpace;
    QString aspectRatio;
    int width = 0;
    int height = 0;
    double frameRate = 0.0;
    int bitRate = 0;
    int bitDepth = 0;
    int channels = 0;
    int sampleRate = 0;
    bool isDefault = false;
    bool isForced = false;
    bool isExternal = false;
    bool isInterlaced = false;
};

struct MediaSourceInfo {
    QString id;
    QString name;
    QString path;
    QString container;
    QString protocol;
    QString videoType;
    qint64 size = 0;
    int bitRate = 0;
    qint64 runtimeTicks = 0;
    std::vector<MediaStreamInfo> streams;
};

struct MovieItem {
    QString id;
    QString title;
    QString overview;
    QString posterUrl;
    QString posterTag;
    QString itemType;
    QString playlistItemId;
    QString seriesId;
    QString seasonId;
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
    QString landscapeCardUrl;
    QStringList genres;
    QStringList tags;
    QStringList studios;
    QString officialRating;
    double communityRating = 0.0;
    double criticRating = 0.0;
    QString premiereDate;
    QString endDate;
    std::vector<PersonItem> people;
    std::vector<MediaSourceInfo> mediaSources;
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

struct PlaybackQueueItem {
    QString itemId;
    QString playlistItemId;
};

struct PlaybackSession {
    QString itemId;
    QString title;
    QString itemType;
    QString url;
    QString mediaSourceId;
    QString playSessionId;
    QString playMethod = QStringLiteral("DirectPlay");
    QString container;
    qint64 startTimeTicks = 0;
    qint64 runtimeTicks = 0;
    std::vector<MediaStreamInfo> mediaStreams;
    std::vector<MediaSegment> segments;
    TrickplayInfo trickplay;
    std::vector<PlaybackQueueItem> nowPlayingQueue;
};

QJsonObject toJson(const DiscoveredServer &server);
QJsonObject toJson(const LibraryItem &library);
QJsonObject toJson(const PersonItem &person);
QJsonObject toJson(const MovieItem &movie);
QVariantMap toVariantMap(const BrowseDescriptor &descriptor);
BrowseDescriptor browseDescriptorFromVariantMap(const QVariantMap &map);

DiscoveredServer discoveredServerFromJson(const QJsonObject &object);
LibraryItem libraryFromJson(const QJsonObject &object);
PersonItem personFromJson(const QJsonObject &object);
MovieItem movieFromJson(const QJsonObject &object);

QString exceptionMessage(const std::exception_ptr &exception);
QString normalizedAudioOutputMode(const QString &mode);
QString sanitizedDiagnosticUrl(QString url, qsizetype maxLength = -1);
bool isMeaningfulResumePosition(qint64 resumeTicks, qint64 runtimeTicks);
qint64 normalizedResumeTicks(qint64 resumeTicks, qint64 runtimeTicks);

} // namespace JellyfinNative
