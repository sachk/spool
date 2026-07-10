#include "common/MetaJson.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include <cstdlib>
#include <initializer_list>
#include <utility>

using JellyfinNative::DiscoveredServer;
using JellyfinNative::isPlayableItem;
using JellyfinNative::itemSubtitle;
using JellyfinNative::LibraryItem;
using JellyfinNative::MediaSegment;
using JellyfinNative::MediaSourceInfo;
using JellyfinNative::MediaStreamInfo;
using JellyfinNative::metaFromJson;
using JellyfinNative::MetaJsonKeyPolicy;
using JellyfinNative::metaListFromJson;
using JellyfinNative::metaListToJson;
using JellyfinNative::metaStringListFromJson;
using JellyfinNative::metaToJson;
using JellyfinNative::MovieItem;
using JellyfinNative::PersonItem;

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    qCritical() << message;
    std::exit(EXIT_FAILURE);
}

QJsonObject jsonObject(std::initializer_list<std::pair<QString, QJsonValue>> entries)
{
    QJsonObject object;
    for (const auto& entry : entries)
        object.insert(entry.first, entry.second);
    return object;
}

void requireDiscoveredServer(const DiscoveredServer& actual, const DiscoveredServer& expected, const char *message)
{
    require(actual.id == expected.id, message);
    require(actual.name == expected.name, message);
    require(actual.address == expected.address, message);
}

void requireLibraryItem(const LibraryItem& actual, const LibraryItem& expected, const char *message)
{
    require(actual.id == expected.id, message);
    require(actual.name == expected.name, message);
    require(actual.collectionType == expected.collectionType, message);
    require(actual.imageTag == expected.imageTag, message);
}

void requirePersonItem(const PersonItem& actual, const PersonItem& expected, const char *message)
{
    require(actual.id == expected.id, message);
    require(actual.name == expected.name, message);
    require(actual.type == expected.type, message);
    require(actual.role == expected.role, message);
    require(actual.imageTag == expected.imageTag, message);
}

void requireMediaStreamInfo(const MediaStreamInfo& actual, const MediaStreamInfo& expected, const char *message)
{
    require(actual.index == expected.index, message);
    require(actual.type == expected.type, message);
    require(actual.codec == expected.codec, message);
    require(actual.profile == expected.profile, message);
    require(actual.displayTitle == expected.displayTitle, message);
    require(actual.title == expected.title, message);
    require(actual.language == expected.language, message);
    require(actual.pixelFormat == expected.pixelFormat, message);
    require(actual.videoRange == expected.videoRange, message);
    require(actual.colorPrimaries == expected.colorPrimaries, message);
    require(actual.colorTransfer == expected.colorTransfer, message);
    require(actual.colorSpace == expected.colorSpace, message);
    require(actual.aspectRatio == expected.aspectRatio, message);
    require(actual.width == expected.width, message);
    require(actual.height == expected.height, message);
    require(actual.frameRate == expected.frameRate, message);
    require(actual.bitRate == expected.bitRate, message);
    require(actual.bitDepth == expected.bitDepth, message);
    require(actual.channels == expected.channels, message);
    require(actual.sampleRate == expected.sampleRate, message);
    require(actual.isDefault == expected.isDefault, message);
    require(actual.isForced == expected.isForced, message);
    require(actual.isExternal == expected.isExternal, message);
    require(actual.isInterlaced == expected.isInterlaced, message);
}

void requireMediaSourceInfo(const MediaSourceInfo& actual, const MediaSourceInfo& expected, const char *message)
{
    require(actual.id == expected.id, message);
    require(actual.name == expected.name, message);
    require(actual.path == expected.path, message);
    require(actual.container == expected.container, message);
    require(actual.protocol == expected.protocol, message);
    require(actual.videoType == expected.videoType, message);
    require(actual.size == expected.size, message);
    require(actual.bitRate == expected.bitRate, message);
    require(actual.runtimeTicks == expected.runtimeTicks, message);
    require(actual.streams.size() == expected.streams.size(), message);
    for (qsizetype i = 0; i < actual.streams.size(); ++i)
        requireMediaStreamInfo(actual.streams.at(i), expected.streams.at(i), message);
}

void requireMediaSegment(const MediaSegment& actual, const MediaSegment& expected, const char *message)
{
    require(actual.id == expected.id, message);
    require(actual.type == expected.type, message);
    require(actual.startTicks == expected.startTicks, message);
    require(actual.endTicks == expected.endTicks, message);
}

void requireMovieItem(const MovieItem& actual, const MovieItem& expected, const char *message)
{
    require(actual.id == expected.id, message);
    require(actual.title == expected.title, message);
    require(actual.overview == expected.overview, message);
    require(actual.posterTag == expected.posterTag, message);
    require(actual.itemType == expected.itemType, message);
    require(actual.playlistItemId == expected.playlistItemId, message);
    require(actual.seriesId == expected.seriesId, message);
    require(actual.seasonId == expected.seasonId, message);
    require(actual.seriesName == expected.seriesName, message);
    require(actual.seriesPrimaryImageTag == expected.seriesPrimaryImageTag, message);
    require(itemSubtitle(actual) == itemSubtitle(expected), message);
    require(actual.path == expected.path, message);
    require(actual.year == expected.year, message);
    require(actual.seasonNumber == expected.seasonNumber, message);
    require(actual.episodeNumber == expected.episodeNumber, message);
    require(actual.resumeTicks == expected.resumeTicks, message);
    require(actual.runtimeTicks == expected.runtimeTicks, message);
    require(isPlayableItem(actual) == isPlayableItem(expected), message);
    require(actual.favorite == expected.favorite, message);
    require(actual.played == expected.played, message);
    require(actual.backdropTag == expected.backdropTag, message);
    require(actual.logoTag == expected.logoTag, message);
    require(actual.bannerTag == expected.bannerTag, message);
    require(actual.thumbTag == expected.thumbTag, message);
    require(actual.genres == expected.genres, message);
    require(actual.tags == expected.tags, message);
    require(actual.studios == expected.studios, message);
    require(actual.officialRating == expected.officialRating, message);
    require(actual.communityRating == expected.communityRating, message);
    require(actual.criticRating == expected.criticRating, message);
    require(actual.premiereDate == expected.premiereDate, message);
    require(actual.endDate == expected.endDate, message);
    require(actual.people.size() == expected.people.size(), message);
    for (qsizetype i = 0; i < actual.people.size(); ++i)
        requirePersonItem(actual.people.at(i), expected.people.at(i), message);
    require(actual.mediaSources.size() == expected.mediaSources.size(), message);
    for (qsizetype i = 0; i < actual.mediaSources.size(); ++i)
        requireMediaSourceInfo(actual.mediaSources.at(i), expected.mediaSources.at(i), message);
}

MediaStreamInfo videoStream()
{
    MediaStreamInfo stream;
    stream.index = 0;
    stream.type = QStringLiteral("Video");
    stream.codec = QStringLiteral("hevc");
    stream.profile = QStringLiteral("Main 10");
    stream.displayTitle = QStringLiteral("1080p HEVC HDR");
    stream.title = QStringLiteral("Main video");
    stream.language = QStringLiteral("eng");
    stream.pixelFormat = QStringLiteral("yuv420p10le");
    stream.videoRange = QStringLiteral("HDR");
    stream.colorPrimaries = QStringLiteral("bt2020");
    stream.colorTransfer = QStringLiteral("smpte2084");
    stream.colorSpace = QStringLiteral("bt2020nc");
    stream.aspectRatio = QStringLiteral("16:9");
    stream.width = 1920;
    stream.height = 1080;
    stream.frameRate = 24.0;
    stream.bitRate = 12000000;
    stream.bitDepth = 10;
    stream.isDefault = true;
    stream.isInterlaced = true;
    return stream;
}

MediaStreamInfo audioStream()
{
    MediaStreamInfo stream;
    stream.index = 1;
    stream.type = QStringLiteral("Audio");
    stream.codec = QStringLiteral("aac");
    stream.displayTitle = QStringLiteral("English AAC stereo");
    stream.title = QStringLiteral("Stereo");
    stream.language = QStringLiteral("eng");
    stream.bitRate = 192000;
    stream.channels = 2;
    stream.sampleRate = 48000;
    stream.isForced = true;
    stream.isExternal = true;
    return stream;
}

MediaSourceInfo mediaSource()
{
    MediaSourceInfo source;
    source.id = QStringLiteral("source-1");
    source.name = QStringLiteral("Main source");
    source.path = QStringLiteral("/media/movie.mkv");
    source.container = QStringLiteral("mkv");
    source.protocol = QStringLiteral("File");
    source.videoType = QStringLiteral("VideoFile");
    source.size = 9876543210LL;
    source.bitRate = 12192000;
    source.runtimeTicks = 72001234567LL;
    source.streams = { videoStream(), audioStream() };
    return source;
}

void testDtoRoundTrips()
{
    DiscoveredServer server;
    server.id = QStringLiteral("server-id");
    server.name = QStringLiteral("Living Room Jellyfin");
    server.address = QStringLiteral("https://jellyfin.example.test");
    requireDiscoveredServer(metaFromJson<DiscoveredServer>(metaToJson(server)), server,
        "DiscoveredServer did not survive MetaJson round trip");

    LibraryItem library;
    library.id = QStringLiteral("library-id");
    library.name = QStringLiteral("Movies");
    library.collectionType = QStringLiteral("movies");
    library.imageTag = QStringLiteral("library-tag");
    requireLibraryItem(
        metaFromJson<LibraryItem>(metaToJson(library)), library, "LibraryItem did not survive MetaJson round trip");

    PersonItem person;
    person.id = QStringLiteral("person-id");
    person.name = QStringLiteral("Ada Actor");
    person.type = QStringLiteral("Actor");
    person.role = QStringLiteral("Detective");
    person.imageTag = QStringLiteral("person-tag");
    requirePersonItem(
        metaFromJson<PersonItem>(metaToJson(person)), person, "PersonItem did not survive MetaJson round trip");

    const MediaStreamInfo stream = videoStream();
    requireMediaStreamInfo(metaFromJson<MediaStreamInfo>(metaToJson(stream)), stream,
        "MediaStreamInfo did not survive MetaJson round trip");

    const MediaSourceInfo source = mediaSource();
    requireMediaSourceInfo(metaFromJson<MediaSourceInfo>(metaToJson(source)), source,
        "MediaSourceInfo did not survive MetaJson round trip");

    MediaSegment segment;
    segment.id = QStringLiteral("intro");
    segment.type = QStringLiteral("Intro");
    segment.startTicks = 10000000LL;
    segment.endTicks = 650000000LL;
    requireMediaSegment(
        metaFromJson<MediaSegment>(metaToJson(segment)), segment, "MediaSegment did not survive MetaJson round trip");
}

void testMovieRoundTripAndCamelCaseKey()
{
    PersonItem actor;
    actor.id = QStringLiteral("actor-1");
    actor.name = QStringLiteral("Ada Actor");
    actor.type = QStringLiteral("Actor");
    actor.role = QStringLiteral("Lead");
    actor.imageTag = QStringLiteral("actor-tag");

    PersonItem director;
    director.id = QStringLiteral("director-1");
    director.name = QStringLiteral("Drew Director");
    director.type = QStringLiteral("Director");
    director.role = QStringLiteral("Director");
    director.imageTag = QStringLiteral("director-tag");

    MovieItem movie;
    movie.id = QStringLiteral("movie-1");
    movie.title = QStringLiteral("A Generic Mapper");
    movie.overview = QStringLiteral("Round-trips DTOs through QMetaProperty.");
    movie.posterTag = QStringLiteral("poster-tag");
    movie.itemType = QStringLiteral("Episode");
    movie.playlistItemId = QStringLiteral("playlist-item-1");
    movie.seriesId = QStringLiteral("series-1");
    movie.seasonId = QStringLiteral("season-1");
    movie.seriesName = QStringLiteral("Mapper Anthology");
    movie.seriesPrimaryImageTag = QStringLiteral("series-tag");
    movie.path = QStringLiteral("/media/movie.mkv");
    movie.year = 2026;
    movie.seasonNumber = 2;
    movie.episodeNumber = 7;
    movie.resumeTicks = 4567890123LL;
    movie.runtimeTicks = 9876543210LL;
    movie.favorite = true;
    movie.played = true;
    movie.backdropTag = QStringLiteral("backdrop-tag");
    movie.logoTag = QStringLiteral("logo-tag");
    movie.bannerTag = QStringLiteral("banner-tag");
    movie.thumbTag = QStringLiteral("thumb-tag");
    movie.genres = { QStringLiteral("Mystery"), QStringLiteral("Drama") };
    movie.tags = { QStringLiteral("tag-a"), QStringLiteral("tag-b") };
    movie.studios = { QStringLiteral("Studio One"), QStringLiteral("Studio Two") };
    movie.officialRating = QStringLiteral("PG-13");
    movie.communityRating = 7.5;
    movie.criticRating = 8.25;
    movie.premiereDate = QStringLiteral("2026-01-02T03:04:05.0000000Z");
    movie.endDate = QStringLiteral("2026-01-02T04:05:06.0000000Z");
    movie.people = { actor, director };
    movie.mediaSources = { mediaSource() };

    const QJsonObject json = metaToJson(movie);
    require(json.value(QStringLiteral("movieId")).toString() == movie.id,
        "MovieItem id was not serialized with the movieId CamelCase key");
    require(!json.contains(QStringLiteral("id")), "MovieItem id leaked through an id key instead of movieId");
    require(!json.contains(QStringLiteral("playable")) && !json.contains(QStringLiteral("subtitle")),
        "MovieItem derived properties should not be serialized into cache payloads");
    require(json.value(QStringLiteral("people")).toArray().size() == movie.people.size(),
        "MovieItem people were not serialized as a nested array");
    require(json.value(QStringLiteral("mediaSources")).toArray().size() == movie.mediaSources.size(),
        "MovieItem mediaSources were not serialized as a nested array");
    require(json.value(QStringLiteral("mediaSources"))
                .toArray()
                .at(0)
                .toObject()
                .value(QStringLiteral("streams"))
                .toArray()
                .size()
            == movie.mediaSources.at(0).streams.size(),
        "MediaSourceInfo streams were not serialized as a nested array");

    requireMovieItem(metaFromJson<MovieItem>(json), movie, "MovieItem nested DTOs did not survive MetaJson round trip");
}

void testMetaListRoundTrip()
{
    MediaSegment intro;
    intro.id = QStringLiteral("intro");
    intro.type = QStringLiteral("Intro");
    intro.startTicks = 10000000LL;
    intro.endTicks = 610000000LL;

    MediaSegment credits;
    credits.id = QStringLiteral("credits");
    credits.type = QStringLiteral("Outro");
    credits.startTicks = 7200000000LL;
    credits.endTicks = 7350000000LL;

    const QList<MediaSegment> segments = { intro, credits };
    const QList<MediaSegment> restored = metaListFromJson<MediaSegment>(metaListToJson(segments));
    require(restored.size() == segments.size(), "MetaJson list round trip changed item count");
    for (qsizetype i = 0; i < restored.size(); ++i)
        requireMediaSegment(restored.at(i), segments.at(i), "MetaJson list round trip changed a MediaSegment");
}

void testLegacyStringTicksAndUnknownKeys()
{
    const QJsonObject movieJson = jsonObject({
        { QStringLiteral("movieId"), QStringLiteral("movie-legacy") },
        { QStringLiteral("resumeTicks"), QStringLiteral("4567890123") },
        { QStringLiteral("runtimeTicks"), QStringLiteral("9876543210") },
        { QStringLiteral("unknownFromServer"), QStringLiteral("ignored") },
    });
    const MovieItem movie = metaFromJson<MovieItem>(movieJson);
    require(movie.id == QStringLiteral("movie-legacy"),
        "MovieItem did not parse known keys when unknown keys were present");
    require(movie.resumeTicks == 4567890123LL, "MovieItem resumeTicks did not parse a legacy JSON string value");
    require(movie.runtimeTicks == 9876543210LL, "MovieItem runtimeTicks did not parse a legacy JSON string value");

    const QJsonObject segmentJson = jsonObject({
        { QStringLiteral("id"), QStringLiteral("chapter-1") },
        { QStringLiteral("type"), QStringLiteral("Recap") },
        { QStringLiteral("startTicks"), QStringLiteral("1234567890") },
        { QStringLiteral("endTicks"), QStringLiteral("2234567890") },
        { QStringLiteral("ignoredNestedObject"), QJsonObject({ { QStringLiteral("x"), 1 } }) },
    });
    const MediaSegment segment = metaFromJson<MediaSegment>(segmentJson);
    require(segment.id == QStringLiteral("chapter-1") && segment.type == QStringLiteral("Recap"),
        "MediaSegment did not parse known keys when unknown keys were present");
    require(segment.startTicks == 1234567890LL, "MediaSegment startTicks did not parse a legacy JSON string value");
    require(segment.endTicks == 2234567890LL, "MediaSegment endTicks did not parse a legacy JSON string value");
}

void testPascalCaseApiParsing()
{
    const QJsonObject movieJson = jsonObject({
        { QStringLiteral("Id"), QStringLiteral("movie-api") },
        { QStringLiteral("Name"), QStringLiteral("API Movie") },
        { QStringLiteral("Type"), QStringLiteral("Movie") },
        { QStringLiteral("ProductionYear"), 2026 },
        { QStringLiteral("Playable"), false },
    });
    const MovieItem apiMovie = metaFromJson<MovieItem>(movieJson, MetaJsonKeyPolicy::PascalCase);
    require(apiMovie.id == QStringLiteral("movie-api") && apiMovie.title == QStringLiteral("API Movie")
            && apiMovie.itemType == QStringLiteral("Movie") && apiMovie.year == 2026,
        "MovieItem did not map its PascalCase API aliases");
    require(isPlayableItem(apiMovie), "MovieItem playability should be derived instead of read from API JSON");

    const QJsonObject streamJson = jsonObject({
        { QStringLiteral("Index"), 3 },
        { QStringLiteral("Type"), QStringLiteral("Subtitle") },
        { QStringLiteral("Codec"), QStringLiteral("subrip") },
        { QStringLiteral("DisplayTitle"), QStringLiteral("English SRT") },
        { QStringLiteral("Language"), QStringLiteral("eng") },
        { QStringLiteral("IsDefault"), true },
        { QStringLiteral("IsForced"), true },
        { QStringLiteral("IsExternal"), true },
    });
    const MediaStreamInfo stream = metaFromJson<MediaStreamInfo>(streamJson, MetaJsonKeyPolicy::PascalCase);
    require(stream.index == 3 && stream.type == QStringLiteral("Subtitle") && stream.codec == QStringLiteral("subrip"),
        "MediaStreamInfo did not parse PascalCase identity fields");
    require(stream.displayTitle == QStringLiteral("English SRT") && stream.language == QStringLiteral("eng"),
        "MediaStreamInfo did not parse PascalCase display fields");
    require(stream.isDefault && stream.isForced && stream.isExternal,
        "MediaStreamInfo did not parse PascalCase boolean fields");

    const QJsonObject sourceJson = jsonObject({
        { QStringLiteral("Id"), QStringLiteral("source-api") },
        { QStringLiteral("Name"), QStringLiteral("API source") },
        { QStringLiteral("Path"), QStringLiteral("/api/movie.mkv") },
        { QStringLiteral("Container"), QStringLiteral("mkv") },
        { QStringLiteral("Protocol"), QStringLiteral("File") },
        { QStringLiteral("VideoType"), QStringLiteral("VideoFile") },
        { QStringLiteral("Size"), QStringLiteral("12345678901") },
        { QStringLiteral("BitRate"), 6543210 },
        { QStringLiteral("RuntimeTicks"), QStringLiteral("7654321098") },
        { QStringLiteral("Streams"), QJsonArray({ streamJson }) },
    });
    const MediaSourceInfo source = metaFromJson<MediaSourceInfo>(sourceJson, MetaJsonKeyPolicy::PascalCase);
    require(source.id == QStringLiteral("source-api") && source.name == QStringLiteral("API source")
            && source.path == QStringLiteral("/api/movie.mkv"),
        "MediaSourceInfo did not parse PascalCase identity fields");
    require(source.container == QStringLiteral("mkv") && source.protocol == QStringLiteral("File")
            && source.videoType == QStringLiteral("VideoFile"),
        "MediaSourceInfo did not parse PascalCase format fields");
    require(source.size == 12345678901LL && source.bitRate == 6543210 && source.runtimeTicks == 7654321098LL,
        "MediaSourceInfo did not parse PascalCase numeric fields");
    require(source.streams.size() == 1, "MediaSourceInfo did not parse PascalCase nested streams");
    requireMediaStreamInfo(source.streams.at(0), stream, "MediaSourceInfo changed a PascalCase nested stream");

    const QJsonObject studiosJson {
        { QStringLiteral("Studios"),
            QJsonArray { QJsonObject { { QStringLiteral("Name"), QStringLiteral("Studio One") } },
                QJsonObject { { QStringLiteral("Name"), QStringLiteral("Studio Two") } } } },
    };
    require(metaStringListFromJson(studiosJson, { QStringLiteral("Studios"), QStringLiteral("Name") })
            == QStringList({ QStringLiteral("Studio One"), QStringLiteral("Studio Two") }),
        "MetaJson did not flatten nested string paths");
}

} // namespace

int main()
{
    testDtoRoundTrips();
    testMovieRoundTripAndCamelCaseKey();
    testMetaListRoundTrip();
    testLegacyStringTicksAndUnknownKeys();
    testPascalCaseApiParsing();
    return EXIT_SUCCESS;
}
