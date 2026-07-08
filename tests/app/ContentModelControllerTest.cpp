#include "app/ContentModelController.h"
#include "api/JellyfinApiFacade.h"
#include "app/LibraryPrefetchController.h"
#include "common/AsyncTask.h"

#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <utility>

using JellyfinNative::AuthSession;
using JellyfinNative::BrowseDescriptor;
using JellyfinNative::ContentModelController;
using JellyfinNative::JellyfinApiFacade;
using JellyfinNative::LibraryPrefetchController;
using JellyfinNative::MovieGridModel;
using JellyfinNative::MovieItem;
using JellyfinNative::PagedMovieItems;

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    qCritical() << message;
    std::exit(EXIT_FAILURE);
}

QByteArray jsonBytes(const QJsonObject& object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QJsonObject episodeObject()
{
    return {
        { QStringLiteral("Id"), QStringLiteral("episode-row") },
        { QStringLiteral("Name"), QStringLiteral("The Loaded Episode") },
        { QStringLiteral("Type"), QStringLiteral("Episode") },
        { QStringLiteral("SeriesId"), QStringLiteral("series-1") },
        { QStringLiteral("SeasonId"), QStringLiteral("season-1") },
        { QStringLiteral("SeriesName"), QStringLiteral("Series One") },
        { QStringLiteral("ParentIndexNumber"), 2 },
        { QStringLiteral("IndexNumber"), 7 },
    };
}

QJsonObject playlistMovieObject()
{
    return {
        { QStringLiteral("Id"), QStringLiteral("playlist-movie-1") },
        { QStringLiteral("Name"), QStringLiteral("Playlist Movie") },
        { QStringLiteral("Type"), QStringLiteral("Movie") },
        { QStringLiteral("PlaylistItemId"), QStringLiteral("playlist-item-1") },
    };
}

QJsonObject collectionObject()
{
    return {
        { QStringLiteral("Id"), QStringLiteral("boxset-1") },
        { QStringLiteral("Name"), QStringLiteral("A Collection") },
        { QStringLiteral("Type"), QStringLiteral("BoxSet") },
    };
}

QJsonObject boxSetChildObject()
{
    return {
        { QStringLiteral("Id"), QStringLiteral("boxset-child-1") },
        { QStringLiteral("Name"), QStringLiteral("Collection Child") },
        { QStringLiteral("Type"), QStringLiteral("Movie") },
        { QStringLiteral("ProductionYear"), 1999 },
    };
}

class MemoryReply final : public QNetworkReply {
public:
    MemoryReply(const QNetworkRequest& request, QNetworkAccessManager::Operation operation, QByteArray payload,
        int statusCode, QObject *parent)
        : QNetworkReply(parent)
        , m_payload(std::move(payload))
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(operation);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, statusCode);
        setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        QTimer::singleShot(0, this, [this]() {
            emit readyRead();
            emit finished();
        });
    }

    void abort() override { }

    qint64 bytesAvailable() const override
    {
        return static_cast<qint64>(m_payload.size() - m_offset) + QNetworkReply::bytesAvailable();
    }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        if (m_offset >= m_payload.size() || maxSize <= 0)
            return -1;

        const qint64 length = std::min<qint64>(maxSize, m_payload.size() - m_offset);
        std::memcpy(data, m_payload.constData() + m_offset, static_cast<size_t>(length));
        m_offset += length;
        return length;
    }

private:
    QByteArray m_payload;
    qsizetype m_offset = 0;
};

class FakeNetworkAccessManager final : public QNetworkAccessManager {
public:
    QVector<QUrl> requestedUrls;

protected:
    QNetworkReply *createRequest(Operation operation, const QNetworkRequest& request, QIODevice *outgoingData) override
    {
        Q_UNUSED(outgoingData);
        requestedUrls.push_back(request.url());

        const QUrl url = request.url();
        const QUrlQuery query(url);
        if (operation == GetOperation && url.path() == QStringLiteral("/Shows/series-1/Episodes")
            && query.queryItemValue(QStringLiteral("seasonId")) == QStringLiteral("season-1")) {
            return new MemoryReply(request, operation,
                jsonBytes({ { QStringLiteral("Items"), QJsonArray { episodeObject() } } }), 200, this);
        }

        if (operation == GetOperation && url.path() == QStringLiteral("/Shows/series-1/Episodes")) {
            return new MemoryReply(
                request, operation, jsonBytes({ { QStringLiteral("Items"), QJsonArray {} } }), 200, this);
        }

        if (operation == GetOperation && url.path() == QStringLiteral("/Playlists/playlist-1/Items")) {
            return new MemoryReply(request, operation,
                jsonBytes({ { QStringLiteral("Items"), QJsonArray { playlistMovieObject() } },
                    { QStringLiteral("TotalRecordCount"), 1 } }),
                200, this);
        }

        if (operation == GetOperation && url.path() == QStringLiteral("/Items")
            && query.queryItemValue(QStringLiteral("parentId")) == QStringLiteral("movies-id")
            && query.queryItemValue(QStringLiteral("includeItemTypes")) == QStringLiteral("BoxSet")
            && !query.hasQueryItem(QStringLiteral("mediaTypes"))) {
            return new MemoryReply(request, operation,
                jsonBytes({ { QStringLiteral("Items"), QJsonArray { collectionObject() } },
                    { QStringLiteral("TotalRecordCount"), 1 } }),
                200, this);
        }

        if (operation == GetOperation && url.path() == QStringLiteral("/Items")
            && query.queryItemValue(QStringLiteral("parentId")) == QStringLiteral("boxset-1")) {
            return new MemoryReply(request, operation,
                jsonBytes({ { QStringLiteral("Items"), QJsonArray { boxSetChildObject() } },
                    { QStringLiteral("TotalRecordCount"), 1 } }),
                200, this);
        }

        if (operation == GetOperation
            && (url.path() == QStringLiteral("/Items/episode-1/Similar")
                || url.path() == QStringLiteral("/Items/boxset-1/Similar"))) {
            return new MemoryReply(
                request, operation, jsonBytes({ { QStringLiteral("Items"), QJsonArray {} } }), 200, this);
        }

        return new MemoryReply(
            request, operation, jsonBytes({ { QStringLiteral("Items"), QJsonArray {} } }), 404, this);
    }
};

bool waitForDetailRowsIdle(ContentModelController& controller, int timeoutMs)
{
    if (!controller.detailRowsBusy())
        return true;

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&controller, &ContentModelController::detailRowsChanged, &loop, [&]() {
        if (!controller.detailRowsBusy())
            loop.quit();
    });

    timeout.start(timeoutMs);
    loop.exec();
    return !controller.detailRowsBusy();
}

bool waitForBrowsePage(JellyfinApiFacade& api, const BrowseDescriptor& descriptor, const QVariantMap& queryOptions,
    PagedMovieItems& page, QString& error, int timeoutMs)
{
    bool finished = false;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    JellyfinNative::Async::runDetached(
        api.fetchBrowsePage(descriptor, 0, 72, queryOptions),
        [&page, &finished, &loop](PagedMovieItems value) {
            page = std::move(value);
            finished = true;
            loop.quit();
        },
        [&error, &finished, &loop](const std::exception_ptr& exception) {
            error = JellyfinNative::exceptionMessage(exception);
            finished = true;
            loop.quit();
        },
        "test fetchBrowsePage");

    timeout.start(timeoutMs);
    if (!finished)
        loop.exec();
    return finished && error.isEmpty();
}

bool requestedOrderedPlaylistItems(const QVector<QUrl>& urls)
{
    return std::any_of(urls.cbegin(), urls.cend(), [](const QUrl& url) {
        const QUrlQuery query(url);
        return url.path() == QStringLiteral("/Playlists/playlist-1/Items")
            && !query.hasQueryItem(QStringLiteral("parentId")) && !query.hasQueryItem(QStringLiteral("sortBy"));
    });
}

bool requestedMovieCollections(const QVector<QUrl>& urls)
{
    return std::any_of(urls.cbegin(), urls.cend(), [](const QUrl& url) {
        const QUrlQuery query(url);
        return url.path() == QStringLiteral("/Items")
            && query.queryItemValue(QStringLiteral("parentId")) == QStringLiteral("movies-id")
            && query.queryItemValue(QStringLiteral("includeItemTypes")) == QStringLiteral("BoxSet")
            && query.queryItemValue(QStringLiteral("recursive")) == QStringLiteral("false")
            && !query.hasQueryItem(QStringLiteral("mediaTypes"));
    });
}

bool requestedBoxSetChildren(const QVector<QUrl>& urls)
{
    return std::any_of(urls.cbegin(), urls.cend(), [](const QUrl& url) {
        const QUrlQuery query(url);
        const QStringList types
            = query.queryItemValue(QStringLiteral("includeItemTypes")).split(QLatin1Char(','), Qt::SkipEmptyParts);
        return url.path() == QStringLiteral("/Items")
            && query.queryItemValue(QStringLiteral("parentId")) == QStringLiteral("boxset-1")
            && query.queryItemValue(QStringLiteral("recursive")) == QStringLiteral("false")
            && types.contains(QStringLiteral("Movie")) && types.contains(QStringLiteral("Series"))
            && types.contains(QStringLiteral("Episode"));
    });
}

bool requestedPathWithSeason(const QVector<QUrl>& urls)
{
    return std::any_of(urls.cbegin(), urls.cend(), [](const QUrl& url) {
        const QUrlQuery query(url);
        return url.path() == QStringLiteral("/Shows/series-1/Episodes")
            && query.queryItemValue(QStringLiteral("seasonId")) == QStringLiteral("season-1");
    });
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    FakeNetworkAccessManager network;
    JellyfinApiFacade api(&network);
    api.setServerUrl(QStringLiteral("http://jellyfin.test"));
    api.setSession(AuthSession {
        QStringLiteral("user-1"), QStringLiteral("Tester"), QStringLiteral("token-1"), QStringLiteral("server-1") });
    LibraryPrefetchController prefetch(&api);
    ContentModelController controller(&api, &prefetch);

    PagedMovieItems playlistPage;
    QString browseError;
    require(waitForBrowsePage(api,
                BrowseDescriptor::playlist(QStringLiteral("playlist-1"), QStringLiteral("Ordered Playlist")), {},
                playlistPage, browseError, 1000),
        "playlist browse page was not fetched");
    require(requestedOrderedPlaylistItems(network.requestedUrls),
        "playlist browse did not use the ordered playlist items endpoint");
    require(playlistPage.items.size() == 1, "playlist browse response was not exposed as one item");

    MovieGridModel playlistModel;
    playlistModel.setMovies(playlistPage.items);
    const MovieItem playlistRow = playlistModel.get(0);
    require(playlistRow.id == QStringLiteral("playlist-movie-1"),
        "playlist item id was not populated from the API response");
    require(playlistRow.playlistItemId == QStringLiteral("playlist-item-1"),
        "playlist item snapshot did not preserve PlaylistItemId");

    PagedMovieItems collectionsPage;
    browseError.clear();
    require(
        waitForBrowsePage(api,
            BrowseDescriptor::library(QStringLiteral("movies-id"), QStringLiteral("movies"), QStringLiteral("Films")),
            QVariantMap { { QStringLiteral("includeItemTypes"), QStringList { QStringLiteral("BoxSet") } } },
            collectionsPage, browseError, 1000),
        "movie collection-filter browse page was not fetched");
    require(requestedMovieCollections(network.requestedUrls),
        "movie collection filter did not request BoxSet without a video media type");
    require(collectionsPage.items.size() == 1 && collectionsPage.items.front().itemType == QStringLiteral("BoxSet"),
        "movie collection-filter browse did not expose BoxSet rows");

    controller.loadDetailRows(
        QStringLiteral("episode-1"), QStringLiteral("Episode"), QStringLiteral("series-1"), QStringLiteral("season-1"));

    require(waitForDetailRowsIdle(controller, 1000), "episode detail rows did not finish loading");
    require(requestedPathWithSeason(network.requestedUrls),
        "episode detail rows did not request the selected season's episodes");

    MovieGridModel *episodes = controller.detailSeasons();
    require(episodes->rowCount() == 1, "episode detail rows did not expose fetched episodes");

    const MovieItem row = episodes->get(0);
    require(row.id == QStringLiteral("episode-row"), "episode detail row id was not populated from the API response");
    require(row.itemType == QStringLiteral("Episode"), "episode detail row type was not preserved");
    require(row.seriesId == QStringLiteral("series-1"), "episode detail row series context was not preserved");
    require(row.seasonId == QStringLiteral("season-1"), "episode detail row season context was not preserved");
    require(episodes->data(episodes->index(0, 0), MovieGridModel::DisplaySubtitleRole).toString()
            == QStringLiteral("S02:E07 · The Loaded Episode"),
        "episode detail row did not use the episode display metadata");

    controller.loadDetailRows(QStringLiteral("boxset-1"), QStringLiteral("BoxSet"), QString(), QString());

    require(waitForDetailRowsIdle(controller, 1000), "box set detail rows did not finish loading");
    require(requestedBoxSetChildren(network.requestedUrls),
        "box set detail rows did not request collection children through browse");

    MovieGridModel *boxSetChildren = controller.detailSeasons();
    require(boxSetChildren->rowCount() == 1, "box set detail rows did not expose collection children");

    const MovieItem boxSetRow = boxSetChildren->get(0);
    require(boxSetRow.id == QStringLiteral("boxset-child-1"),
        "box set child id was not populated from the browse response");
    require(boxSetRow.itemType == QStringLiteral("Movie"), "box set child type was not preserved");

    return EXIT_SUCCESS;
}
