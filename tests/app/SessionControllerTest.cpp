#include "app/SessionController.h"
#include "api/JellyfinApiFacade.h"
#include "cache/DatabaseManager.h"

#include <QCoroTask>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>

using namespace JellyfinNative;

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    require(directory.isValid(), "temporary profile directory should be available");

    DatabaseManager database;
    require(database.initialize(directory.filePath(QStringLiteral("profiles.sqlite"))),
        "profile database should initialize");

    const QJsonArray profiles {
        QJsonObject { { QStringLiteral("id"), QStringLiteral("alice-home") },
            { QStringLiteral("serverId"), QStringLiteral("home-id") },
            { QStringLiteral("serverName"), QStringLiteral("Living Room") },
            { QStringLiteral("serverUrl"), QStringLiteral("https://home.example") },
            { QStringLiteral("userId"), QStringLiteral("alice-id") },
            { QStringLiteral("userName"), QStringLiteral("Alice") },
            { QStringLiteral("accessToken"), QStringLiteral("alice-token") }, { QStringLiteral("lastUsedAt"), 2000 } },
        QJsonObject { { QStringLiteral("id"), QStringLiteral("bob-away") },
            { QStringLiteral("serverId"), QStringLiteral("away-id") },
            { QStringLiteral("serverName"), QStringLiteral("Holiday Server") },
            { QStringLiteral("serverUrl"), QStringLiteral("https://away.example/") },
            { QStringLiteral("userId"), QStringLiteral("bob-id") },
            { QStringLiteral("userName"), QStringLiteral("Bob") },
            { QStringLiteral("accessToken"), QStringLiteral("bob-token") }, { QStringLiteral("lastUsedAt"), 1000 } },
    };
    database.saveSetting(QStringLiteral("profiles/accounts-v1"),
        QString::fromUtf8(QJsonDocument(profiles).toJson(QJsonDocument::Compact)));

    QNetworkAccessManager network;
    JellyfinApiFacade api(&network);
    SessionController session(&database, &api);
    require(QCoro::waitFor(session.initializeAsync()), "saved profiles should be detected");
    require(!session.authenticated(), "launch should wait for an explicit account and server selection");
    require(session.accountProfiles().size() == 2, "both account and server pairs should be displayed");

    const QVariantMap first = session.accountProfiles().constFirst().toMap();
    require(first.value(QStringLiteral("userName")).toString() == QStringLiteral("Alice"),
        "profile should expose its account name");
    require(first.value(QStringLiteral("serverName")).toString() == QStringLiteral("Living Room"),
        "profile should expose its paired server name");
    require(first.value(QStringLiteral("serverUrl")).toString() == QStringLiteral("https://home.example"),
        "profile should expose its paired server URL");

    require(session.activateProfile(QStringLiteral("bob-away")), "saved account should activate");
    require(session.authenticated(), "activating a profile should restore its token");
    require(session.serverName() == QStringLiteral("Holiday Server"), "activation should restore the paired server");
    require(session.serverUrl() == QStringLiteral("https://away.example"),
        "activation should normalize the paired server URL");

    session.deactivate();
    require(!session.authenticated(), "switch account should deactivate without removing the pair");
    require(session.accountProfiles().size() == 2, "switch account should preserve all saved pairs");

    const QJsonArray singleProfile { profiles.at(0) };
    database.saveSetting(QStringLiteral("profiles/accounts-v1"),
        QString::fromUtf8(QJsonDocument(singleProfile).toJson(QJsonDocument::Compact)));

    JellyfinApiFacade singleApi(&network);
    SessionController singleSession(&database, &singleApi);
    require(QCoro::waitFor(singleSession.initializeAsync()), "single saved profile should be detected");
    require(singleSession.authenticated(), "a single saved profile should activate automatically");
    require(singleSession.serverUrl() == QStringLiteral("https://home.example"),
        "automatic profile activation should restore its server");
    require(
        singleSession.username() == QStringLiteral("Alice"), "automatic profile activation should restore its account");

    database.shutdown();
    return EXIT_SUCCESS;
}
