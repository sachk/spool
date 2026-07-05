#include "api/JellyfinApiFacade.h"
#include "app/ContentModelController.h"
#include "app/LibraryPrefetchController.h"

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

using JellyfinNative::AuthSession;
using JellyfinNative::ContentModelController;
using JellyfinNative::JellyfinApiFacade;
using JellyfinNative::LibraryPrefetchController;
using JellyfinNative::MovieGridModel;

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    qCritical() << message;
    std::exit(EXIT_FAILURE);
}

QByteArray jsonBytes(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QJsonObject episodeObject()
{
    return {
        {QStringLiteral("Id"), QStringLiteral("episode-row")},
        {QStringLiteral("Name"), QStringLiteral("The Loaded Episode")},
        {QStringLiteral("Type"), QStringLiteral("Episode")},
        {QStringLiteral("SeriesId"), QStringLiteral("series-1")},
        {QStringLiteral("SeasonId"), QStringLiteral("season-1")},
        {QStringLiteral("SeriesName"), QStringLiteral("Series One")},
        {QStringLiteral("ParentIndexNumber"), 2},
        {QStringLiteral("IndexNumber"), 7},
    };
}

class MemoryReply final : public QNetworkReply {
public:
    MemoryReply(const QNetworkRequest &request, QNetworkAccessManager::Operation operation,
                QByteArray payload, int statusCode, QObject *parent)
        : QNetworkReply(parent)
        , m_payload(std::move(payload))
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(operation);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, statusCode);
        setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json"));
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        QTimer::singleShot(0, this, [this]() {
            emit readyRead();
            emit finished();
        });
    }

    void abort() override {}

    qint64 bytesAvailable() const override
    {
        return static_cast<qint64>(m_payload.size() - m_offset) +
               QNetworkReply::bytesAvailable();
    }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        if (m_offset >= m_payload.size() || maxSize <= 0)
            return -1;

        const qint64 length =
            std::min<qint64>(maxSize, m_payload.size() - m_offset);
        std::memcpy(data, m_payload.constData() + m_offset,
                    static_cast<size_t>(length));
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
    QNetworkReply *createRequest(Operation operation,
                                 const QNetworkRequest &request,
                                 QIODevice *outgoingData) override
    {
        Q_UNUSED(outgoingData);
        requestedUrls.push_back(request.url());

        const QUrl url = request.url();
        const QUrlQuery query(url);
        if (operation == GetOperation &&
            url.path() == QStringLiteral("/Shows/series-1/Episodes") &&
            query.queryItemValue(QStringLiteral("seasonId")) ==
                QStringLiteral("season-1")) {
            return new MemoryReply(request, operation,
                                   jsonBytes({{QStringLiteral("Items"),
                                               QJsonArray{episodeObject()}}}),
                                   200, this);
        }

        if (operation == GetOperation &&
            url.path() == QStringLiteral("/Shows/series-1/Episodes")) {
            return new MemoryReply(
                request, operation,
                jsonBytes({{QStringLiteral("Items"), QJsonArray{}}}), 200,
                this);
        }

        if (operation == GetOperation &&
            url.path() == QStringLiteral("/Items/episode-1/Similar")) {
            return new MemoryReply(
                request, operation,
                jsonBytes({{QStringLiteral("Items"), QJsonArray{}}}), 200,
                this);
        }

        return new MemoryReply(request, operation,
                               jsonBytes({{QStringLiteral("Items"),
                                           QJsonArray{}}}),
                               404, this);
    }
};

bool waitForDetailRowsIdle(ContentModelController &controller, int timeoutMs)
{
    if (!controller.detailRowsBusy())
        return true;

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&controller, &ContentModelController::detailRowsChanged,
                     &loop, [&]() {
                         if (!controller.detailRowsBusy())
                             loop.quit();
                     });

    timeout.start(timeoutMs);
    loop.exec();
    return !controller.detailRowsBusy();
}

bool requestedPathWithSeason(const QVector<QUrl> &urls)
{
    return std::any_of(urls.cbegin(), urls.cend(), [](const QUrl &url) {
        const QUrlQuery query(url);
        return url.path() == QStringLiteral("/Shows/series-1/Episodes") &&
               query.queryItemValue(QStringLiteral("seasonId")) ==
                   QStringLiteral("season-1");
    });
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    FakeNetworkAccessManager network;
    JellyfinApiFacade api(&network);
    api.setServerUrl(QStringLiteral("http://jellyfin.test"));
    api.setSession(AuthSession{QStringLiteral("user-1"),
                               QStringLiteral("Tester"),
                               QStringLiteral("token-1"),
                               QStringLiteral("server-1")});
    LibraryPrefetchController prefetch(&api);
    ContentModelController controller(&api, &prefetch);

    controller.loadDetailRows(QStringLiteral("episode-1"),
                              QStringLiteral("Episode"),
                              QStringLiteral("series-1"),
                              QStringLiteral("season-1"));

    require(waitForDetailRowsIdle(controller, 1000),
            "episode detail rows did not finish loading");
    require(requestedPathWithSeason(network.requestedUrls),
            "episode detail rows did not request the selected season's episodes");

    MovieGridModel *episodes = controller.detailSeasons();
    require(episodes->rowCount() == 1,
            "episode detail rows did not expose fetched episodes");

    const QVariantMap row = episodes->get(0);
    require(row.value(QStringLiteral("movieId")).toString() ==
                QStringLiteral("episode-row"),
            "episode detail row id was not populated from the API response");
    require(row.value(QStringLiteral("itemType")).toString() ==
                QStringLiteral("Episode"),
            "episode detail row type was not preserved");
    require(row.value(QStringLiteral("seriesId")).toString() ==
                QStringLiteral("series-1"),
            "episode detail row series context was not preserved");
    require(row.value(QStringLiteral("seasonId")).toString() ==
                QStringLiteral("season-1"),
            "episode detail row season context was not preserved");
    require(row.value(QStringLiteral("displaySubtitle")).toString() ==
                QStringLiteral("S02:E07 · The Loaded Episode"),
            "episode detail row did not use the episode display metadata");

    return EXIT_SUCCESS;
}
