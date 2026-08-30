#include "app/UpdateManifest.h"

#include "TestMain.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

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

QJsonObject release(const QString& channel, int versionCode, const QString& version)
{
    const QString tag = QStringLiteral("v") + version;
    return {
        { QStringLiteral("channel"), channel },
        { QStringLiteral("version"), version },
        { QStringLiteral("versionCode"), versionCode },
        { QStringLiteral("notes"), QStringLiteral("Security fixes first.\n\nMajor features next.") },
        { QStringLiteral("releaseUrl"), QStringLiteral("https://github.com/sachk/spool/releases/tag/") + tag },
        { QStringLiteral("assets"),
            QJsonObject {
                { QStringLiteral("phone-arm64-v8a"),
                    QJsonObject {
                        { QStringLiteral("url"),
                            QStringLiteral("https://github.com/sachk/spool/releases/download/") + tag
                                + QStringLiteral("/spool-phone-arm64-v8a.apk") },
                        { QStringLiteral("sha256"), QString(64, QLatin1Char('a')) },
                        { QStringLiteral("size"), 12000000 },
                    } },
            } },
    };
}

QByteArray manifest(std::initializer_list<QJsonObject> releases)
{
    QJsonArray array;
    for (const QJsonObject& item : releases)
        array.append(item);
    return QJsonDocument(QJsonObject {
                             { QStringLiteral("schemaVersion"), 1 },
                             { QStringLiteral("releases"), array },
                         })
        .toJson(QJsonDocument::Compact);
}

void stableChannelExcludesPrereleases()
{
    const UpdateManifestResult result
        = selectAndroidUpdate(manifest({ release(QStringLiteral("release"), 700099, QStringLiteral("0.7.0")),
                                  release(QStringLiteral("prerelease"), 800001, QStringLiteral("0.8.0-beta.1")) }),
            600099, false, QStringLiteral("phone-arm64-v8a"));
    require(result.error.isEmpty(), "valid stable manifest was rejected");
    require(result.release.has_value(), "stable update was not selected");
    require(result.release->versionCode == 700099, "stable channel selected a prerelease");
}

void prereleaseChannelSelectsLargestEligibleBuild()
{
    UpdateManifestResult result
        = selectAndroidUpdate(manifest({ release(QStringLiteral("prerelease"), 800001, QStringLiteral("0.8.0-beta.1")),
                                  release(QStringLiteral("release"), 700099, QStringLiteral("0.7.0")) }),
            600099, true, QStringLiteral("phone-arm64-v8a"));
    require(result.release && result.release->versionCode == 800001,
        "prerelease channel did not choose the largest eligible build");

    result
        = selectAndroidUpdate(manifest({ release(QStringLiteral("prerelease"), 800001, QStringLiteral("0.8.0-beta.1")),
                                  release(QStringLiteral("release"), 800099, QStringLiteral("0.8.0")) }),
            800001, true, QStringLiteral("phone-arm64-v8a"));
    require(result.release && result.release->versionCode == 800099,
        "prerelease channel did not upgrade to a larger release build");
}

void currentOrOlderBuildsAreIgnored()
{
    const UpdateManifestResult result
        = selectAndroidUpdate(manifest({ release(QStringLiteral("release"), 700099, QStringLiteral("0.7.0")) }), 700099,
            false, QStringLiteral("phone-arm64-v8a"));
    require(result.error.isEmpty(), "current release produced a manifest error");
    require(!result.release, "current release was offered as an update");
}

void untrustedOrIncompleteAssetsAreRejected()
{
    QJsonObject item = release(QStringLiteral("release"), 700099, QStringLiteral("0.7.0"));
    QJsonObject assets = item.value(QStringLiteral("assets")).toObject();
    QJsonObject apk = assets.value(QStringLiteral("phone-arm64-v8a")).toObject();
    apk.insert(QStringLiteral("url"), QStringLiteral("https://example.com/spool.apk"));
    assets.insert(QStringLiteral("phone-arm64-v8a"), apk);
    item.insert(QStringLiteral("assets"), assets);
    require(!selectAndroidUpdate(manifest({ item }), 600099, false, QStringLiteral("phone-arm64-v8a")).error.isEmpty(),
        "off-repository APK URL was accepted");

    item = release(QStringLiteral("release"), 700099, QStringLiteral("0.7.0"));
    assets = item.value(QStringLiteral("assets")).toObject();
    apk = assets.value(QStringLiteral("phone-arm64-v8a")).toObject();
    apk.insert(QStringLiteral("sha256"), QStringLiteral("abcd"));
    assets.insert(QStringLiteral("phone-arm64-v8a"), apk);
    item.insert(QStringLiteral("assets"), assets);
    require(!selectAndroidUpdate(manifest({ item }), 600099, false, QStringLiteral("phone-arm64-v8a")).error.isEmpty(),
        "short SHA-256 was accepted");
}

} // namespace

JELLYFIN_TEST_MAIN("update-manifest")
{
    QCoreApplication app(argc, argv);
    stableChannelExcludesPrereleases();
    prereleaseChannelSelectsLargestEligibleBuild();
    currentOrOlderBuildsAreIgnored();
    untrustedOrIncompleteAssetsAreRejected();
    return EXIT_SUCCESS;
}
