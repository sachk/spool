#include "app/SettingsController.h"
#include "cache/DatabaseManager.h"

#include <QCoreApplication>
#include <QCoroTask>
#include <QDebug>
#include <QTemporaryDir>

#include <cstdlib>

using JellyfinNative::DatabaseManager;
using JellyfinNative::SettingsController;

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    qCritical() << message;
    std::exit(EXIT_FAILURE);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    require(directory.isValid(), "temporary settings directory was not created");

    DatabaseManager database;
    require(database.initialize(directory.filePath(QStringLiteral("settings.sqlite"))),
        "settings database did not initialize");

    SettingsController settings(&database, nullptr, nullptr);
    QCoro::waitFor(settings.loadLocalAsync());
    require(settings.uiScalePercent() == 115, "UI scale default was not 115 percent");
    require(!settings.uiScaleSetupComplete(), "fresh profile unexpectedly skipped scale setup");
    require(!settings.value(QStringLiteral("playback/manualStreamingBitrate")).toBool(),
        "fresh profile unexpectedly enabled the manual streaming limit");
    require(!settings.value(QStringLiteral("playback/unlimitedLocalBitrate")).toBool(),
        "fresh profile unexpectedly enabled unlimited local-network playback");

    settings.setAudioDelayMs(120);
    require(settings.audioDelayMs() == 120, "audio delay setter did not update the global desktop value");
    require(QCoro::waitFor(database.loadSettingAsync(QStringLiteral("settings/audioDelayMs"))) == QStringLiteral("120"),
        "global desktop audio delay was not persisted");

    settings.setUiScalePercent(70);
    require(settings.uiScalePercent() == 80, "UI scale setter did not clamp to its lower bound");
    settings.completeUiScaleSetup(135);
    require(settings.uiScalePercent() == 135, "setup did not apply the selected scale");
    require(settings.uiScaleSetupComplete(), "setup completion marker was not exposed");

    require(
        QCoro::waitFor(database.loadSettingAsync(QStringLiteral("appearance/uiScalePercent"))) == QStringLiteral("135"),
        "selected UI scale was not persisted");
    require(QCoro::waitFor(database.loadSettingAsync(QStringLiteral("appearance/uiScaleSetupVersion")))
            == QStringLiteral("2"),
        "scale setup completion was not persisted");

    SettingsController restored(&database, nullptr, nullptr);
    QCoro::waitFor(restored.loadLocalAsync());
    require(restored.uiScalePercent() == 135, "persisted UI scale was not restored");
    require(restored.uiScaleSetupComplete(), "persisted setup completion was not restored");
    require(restored.audioDelayMs() == 120, "persisted global desktop audio delay was not restored");

    database.saveSetting(QStringLiteral("appearance/uiScalePercent"), QStringLiteral("100"));
    database.saveSetting(QStringLiteral("appearance/uiScaleSetupVersion"), QStringLiteral("1"));
    SettingsController migrated(&database, nullptr, nullptr);
    QCoro::waitFor(migrated.loadLocalAsync());
    require(migrated.uiScalePercent() == 115, "legacy UI scale was not rebased");
    require(QCoro::waitFor(database.loadSettingAsync(QStringLiteral("appearance/uiScaleSetupVersion")))
            == QStringLiteral("2"),
        "UI scale migration version was not persisted");

    database.shutdown();
    return EXIT_SUCCESS;
}
