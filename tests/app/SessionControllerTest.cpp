#include "app/SessionController.h"
#include "api/JellyfinApiFacade.h"
#include "app/AccountProfile.h"
#include "cache/DatabaseManager.h"

#include <QCoroTask>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QTemporaryDir>
#include <QThread>

#include <algorithm>
#include <cstdlib>
#include <functional>
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

bool waitUntil(QCoreApplication& app, const std::function<bool()>& condition)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < 2000) {
        app.processEvents();
        QThread::msleep(1);
    }
    return condition();
}

AccountProfile profile(const QString& serverId, const QString& serverUrl, const QString& userId,
    const QString& userName, const QString& token, qint64 lastUsedAt, qint64 createdAt)
{
    const QString canonical = canonicalServerUrl(serverUrl);
    return { accountProfileId(serverId, canonical, userId), serverId,
        serverId == QStringLiteral("home") ? QStringLiteral("Living Room") : QStringLiteral("Holiday Server"),
        canonical, userId, userName, token, QStringLiteral("avatar-tag"), lastUsedAt, createdAt, token.isEmpty() };
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

    require(canonicalServerUrl(QStringLiteral("HTTPS://Home.Example:443/jellyfin/"))
            == QStringLiteral("https://home.example/jellyfin"),
        "server URL should be canonicalized");
    const QString variantId = accountProfileId({}, QStringLiteral("https://HOME.example/"), QStringLiteral("alice"));
    const QString canonicalId = accountProfileId({}, QStringLiteral("https://home.example"), QStringLiteral("alice"));
    if (variantId != canonicalId)
        std::cerr << variantId.toStdString() << " != " << canonicalId.toStdString() << '\n';
    require(variantId == canonicalId, "canonical URL variants should identify the same account pair");

    const AccountProfile aliceHome = profile(QStringLiteral("home"), QStringLiteral("https://home.example/"),
        QStringLiteral("alice"), QStringLiteral("Alice"), QStringLiteral("alice-secret"), 100, 10);
    const AccountProfile bobHome = profile(QStringLiteral("home"), QStringLiteral("https://home.example"),
        QStringLiteral("bob"), QStringLiteral("Bob"), QStringLiteral("bob-secret"), 300, 20);
    const AccountProfile aliceAway = profile(QStringLiteral("away"), QStringLiteral("https://away.example"),
        QStringLiteral("alice"), QStringLiteral("Alice"), QStringLiteral("away-secret"), 200, 30);
    database.upsertAccountProfile(aliceHome);
    database.upsertAccountProfile(bobHome);
    database.upsertAccountProfile(aliceAway);

    std::vector<AccountProfile> stored = QCoro::waitFor(database.loadAccountProfilesAsync());
    require(stored.size() == 3, "two users on one server and one user on two servers should remain distinct");
    require(stored[0].profileId == bobHome.profileId && stored[1].profileId == aliceAway.profileId,
        "profiles should load by most recent use");
    require(
        stored[2].avatarTag == QStringLiteral("avatar-tag") && stored[2].accessToken == QStringLiteral("alice-secret"),
        "durable profile fields should round-trip");

    AccountProfile updatedAlice = aliceHome;
    updatedAlice.serverName = QStringLiteral("Renamed Home");
    updatedAlice.lastUsedAt = 400;
    database.upsertAccountProfile(updatedAlice);
    stored = QCoro::waitFor(database.loadAccountProfilesAsync());
    require(stored.size() == 3 && stored.front().serverName == QStringLiteral("Renamed Home"),
        "upsert should replace the matching account/server pair without duplicating it");
    require(stored.front().createdAt == aliceHome.createdAt, "upsert should preserve insertion order");

    QNetworkAccessManager network;
    JellyfinApiFacade api(&network);
    SessionController session(&database, &api);
    require(QCoro::waitFor(session.initializeAsync()), "saved profiles should be detected");
    require(!session.authenticated(), "startup should never activate a saved token");
    require(session.accountProfiles().size() == 3, "all account and server pairs should be displayed");
    const QByteArray summaries = QJsonDocument::fromVariant(session.accountProfiles()).toJson();
    require(!summaries.contains("secret"), "profile summaries exposed to QML must not contain tokens");

    session.activateProfile(bobHome.profileId);
    require(waitUntil(app, [&session]() { return session.authenticated(); }), "saved account should activate");
    require(session.activeProfileId() == bobHome.profileId, "activation should select a stable profile ID");
    require(session.activeProfileLabel() == QStringLiteral("Bob · Living Room"),
        "active profile should expose account and server identity");
    require(session.serverUrl() == QStringLiteral("https://home.example")
            && api.session().accessToken == QStringLiteral("bob-secret"),
        "activation should swap the server URL and token together");

    session.deactivate();
    require(!session.authenticated(), "switch user should clear the active session");
    require(session.accountProfiles().size() == 3, "switch user should preserve saved pairs");

    session.activateProfile(aliceAway.profileId);
    require(waitUntil(app, [&session]() { return session.authenticated(); }), "second server profile should activate");
    session.logout();
    require(!session.authenticated(), "sign out should clear the active token");
    require(session.accountProfiles().size() == 3, "sign out should keep the account tile");
    stored = QCoro::waitFor(database.loadAccountProfilesAsync());
    const auto expired = std::find_if(stored.cbegin(), stored.cend(),
        [&aliceAway](const AccountProfile& candidate) { return candidate.profileId == aliceAway.profileId; });
    require(expired != stored.cend() && expired->needsAuthentication && expired->accessToken.isEmpty(),
        "sign out should expire only the active profile");
    require(std::any_of(stored.cbegin(), stored.cend(),
                [&bobHome](const AccountProfile& candidate) {
                    return candidate.profileId == bobHome.profileId && !candidate.accessToken.isEmpty();
                }),
        "expiring one profile should not erase sibling tokens");

    session.activateProfile(aliceAway.profileId);
    require(waitUntil(app, [&session]() { return session.profileSignInRequired(); }),
        "expired profile activation should open exact-pair sign in");
    require(session.serverUrl() == aliceAway.serverUrl && session.username() == aliceAway.userName,
        "reauthentication should retain the selected server and user");

    session.removeProfile(aliceAway.profileId);
    stored = QCoro::waitFor(database.loadAccountProfilesAsync());
    require(stored.size() == 2, "remove from device should delete only the selected pair");

    database.clearAccountProfiles();
    JellyfinApiFacade emptyApi(&network);
    SessionController emptySession(&database, &emptyApi);
    require(!QCoro::waitFor(emptySession.initializeAsync()), "zero profiles should route to add account");
    require(!emptySession.authenticated(), "zero-profile startup should remain signed out");

    database.upsertAccountProfile(aliceHome);
    JellyfinApiFacade singleApi(&network);
    SessionController singleSession(&database, &singleApi);
    require(QCoro::waitFor(singleSession.initializeAsync()), "one profile should route to profile selection");
    require(!singleSession.authenticated(), "one saved profile must not auto-activate");

    database.shutdown();
    return EXIT_SUCCESS;
}
