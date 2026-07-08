#include "cache/DatabaseManager.h"
#include <QCoroTask>

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QThread>

#include <cstdlib>
#include <iostream>

using namespace JellyfinNative;

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << message << '\n';
    std::exit(1);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    require(directory.isValid(), "temporary directory should be available");

    DatabaseManager database;
    require(database.initialize(directory.filePath(QStringLiteral("cache.sqlite"))), "database should initialize");
    require(QCoro::waitFor(database.schemaVersionAsync()) == 4, "schema should migrate to version 4");

    const QJsonObject homePayload {
        { QStringLiteral("title"), QStringLiteral("Continue Watching") },
        { QStringLiteral("count"), 2 },
    };
    database.saveHomePayload(QStringLiteral("server/user"), 1, homePayload);
    require(QCoro::waitFor(database.loadHomePayloadAsync(QStringLiteral("server/user"), 1)) == homePayload,
        "home payload should load for matching schema");
    require(QCoro::waitFor(database.loadHomePayloadAsync(QStringLiteral("server/user"), 2)).isEmpty(),
        "home payload should not load for a different schema");
    database.invalidateHomePayloads();
    require(QCoro::waitFor(database.loadHomePayloadAsync(QStringLiteral("server/user"), 1)).isEmpty(),
        "home payload invalidation should remove cached payloads");

    database.saveCacheEntry(QStringLiteral("test"), QStringLiteral("fresh"), QByteArrayLiteral("value"), 5000);
    require(QCoro::waitFor(database.loadCacheEntryAsync(QStringLiteral("test"), QStringLiteral("fresh")))
            == QByteArrayLiteral("value"),
        "fresh cache entry should load");

    database.invalidateCacheNamespace(QStringLiteral("test"));
    require(QCoro::waitFor(database.loadCacheEntryAsync(QStringLiteral("test"), QStringLiteral("fresh"))).isEmpty(),
        "namespace invalidation should remove entries");

    database.saveCacheEntry(QStringLiteral("test"), QStringLiteral("expired"), QByteArrayLiteral("value"), 1);
    QThread::msleep(5);
    require(QCoro::waitFor(database.loadCacheEntryAsync(QStringLiteral("test"), QStringLiteral("expired"))).isEmpty(),
        "expired cache entry should not load");

    database.saveCacheEntry(QStringLiteral("test"), QStringLiteral("old"), QByteArrayLiteral("old"));
    QThread::msleep(2);
    database.saveCacheEntry(QStringLiteral("test"), QStringLiteral("new"), QByteArrayLiteral("new"));
    database.evictCacheEntries(1);
    require(QCoro::waitFor(database.loadCacheEntryAsync(QStringLiteral("test"), QStringLiteral("old"))).isEmpty(),
        "least recently used entry should be evicted");
    require(QCoro::waitFor(database.loadCacheEntryAsync(QStringLiteral("test"), QStringLiteral("new")))
            == QByteArrayLiteral("new"),
        "newest cache entry should remain");

    database.shutdown();
    return 0;
}
