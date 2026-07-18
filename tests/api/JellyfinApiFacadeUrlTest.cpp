#include "api/JellyfinApiFacade.h"
#include "app/ArtworkService.h"
#include "common/TlsTrust.h"

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QTemporaryDir>
#include <QUrl>
#include <QUrlQuery>

#include <cstdlib>
#include <iostream>

using namespace JellyfinNative;

namespace {

void fail(const char *message, const QString& value)
{
    std::cerr << message << ": " << value.toStdString() << '\n';
    std::exit(1);
}

void require(bool condition, const char *message, const QString& value = {})
{
    if (condition)
        return;
    fail(message, value);
}

void requireQueryValue(const QUrlQuery& query, const QString& key, const QString& expected, const char *message)
{
    const QString actual = query.queryItemValue(key);
    require(query.hasQueryItem(key) && actual == expected, message, actual);
}

void requireMissingQueryValue(const QUrlQuery& query, const QString& key, const char *message)
{
    require(!query.hasQueryItem(key), message, query.toString(QUrl::FullyEncoded));
}

void requireUrlPathBytes(const QString& url, const QString& expectedPath, const char *message)
{
    const QString actual = url.section(QLatin1Char('?'), 0, 0);
    const QString expected = QStringLiteral("https://media.example.test") + expectedPath;
    require(actual == expected, message, actual);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QNetworkAccessManager network;
    TlsTrustController tlsTrust;
    JellyfinApiFacade api(&network, &tlsTrust);
    api.setServerUrl(QStringLiteral("https://media.example.test/jellyfin/root/"));

    QTemporaryDir cacheDirectory;
    require(cacheDirectory.isValid(), "artwork test cache should be available");
    ArtworkService artwork(cacheDirectory.path(), 1024 * 1024, 1024 * 1024, 1, &tlsTrust);
    artwork.setServerUrl(QStringLiteral("https://media.example.test/jellyfin/root/"));
    MovieItem imageItem;
    imageItem.id = QStringLiteral("folder/item 1");
    imageItem.thumbTag = QStringLiteral("tag/one two");

    const QString imageUrl = artwork.url(QVariant::fromValue(imageItem), QStringLiteral("landscape"), 320);
    const QUrl parsedImage(imageUrl);
    requireUrlPathBytes(imageUrl, QStringLiteral("/jellyfin/root/Items/folder%2Fitem%201/Images/Thumb"),
        "artwork URLs should retain the server base path and encode path segments");

    const QUrlQuery imageQuery(parsedImage);
    requireQueryValue(
        imageQuery, QStringLiteral("fillWidth"), QStringLiteral("320"), "filled image URLs should include fill width");
    requireQueryValue(imageQuery, QStringLiteral("fillHeight"), QStringLiteral("180"),
        "filled image URLs should include fill height");
    requireMissingQueryValue(
        imageQuery, QStringLiteral("maxWidth"), "filled image URLs should not also request max width");
    requireQueryValue(imageQuery, QStringLiteral("quality"), QStringLiteral("68"), "image URLs should include quality");
    requireQueryValue(imageQuery, QStringLiteral("format"), QStringLiteral("webp"), "image URLs should include format");
    requireQueryValue(imageQuery, QStringLiteral("tag"), QStringLiteral("tag/one two"),
        "image URLs should include the image tag query item");

    imageItem.thumbTag.clear();
    require(artwork.url(QVariant::fromValue(imageItem), QStringLiteral("landscape"), 320).isEmpty(),
        "image URLs should be omitted when the image tag is empty");

    const QString trickplayUrl = api.trickplayTileUrl(QStringLiteral("episode/id 2"), 320, 7);
    requireUrlPathBytes(trickplayUrl, QStringLiteral("/jellyfin/root/Videos/episode%2Fid%202/Trickplay/320/7.jpg"),
        "trickplay tile URLs should retain the server base path and encode item ids as one path segment");
    const QUrl parsedTrickplay(trickplayUrl);
    const QUrlQuery trickplayQuery(parsedTrickplay);
    requireMissingQueryValue(
        trickplayQuery, QStringLiteral("api_key"), "trickplay tile URLs should not include bearer tokens");

    return 0;
}
