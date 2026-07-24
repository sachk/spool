#include "app/SettingsController.h"
#include "cache/DatabaseManager.h"

#include <QCoreApplication>
#include <QCoroTask>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>

using JellyfinNative::DatabaseManager;
using JellyfinNative::SettingsController;

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
    require(settings.value(QStringLiteral("playback/forwardCacheSizeMiB")).toInt() == 32,
        "fresh profile did not use the 32 MB forward cache default");
    require(settings.playerControlTooltipsEnabled(), "fresh profile unexpectedly hid player control tooltips");

    settings.setValue(QStringLiteral("playback/forwardCacheSizeMiB"), QStringLiteral("256"));
    require(settings.value(QStringLiteral("playback/forwardCacheSizeMiB")).toString() == QStringLiteral("256"),
        "forward cache size was not updated");
    require(QCoro::waitFor(database.loadSettingAsync(QStringLiteral("playback/forwardCacheSizeMiB")))
            == QStringLiteral("256"),
        "forward cache size was not persisted");

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

    settings.setValue(QStringLiteral("subtitles/styling"), QStringLiteral("Custom"));
    settings.setValue(QStringLiteral("subtitles/textSize"), QStringLiteral("xlarge"));
    settings.setValue(QStringLiteral("subtitles/textColor"), QStringLiteral("#00ff00"));
    settings.setValue(QStringLiteral("subtitles/dimInHdr"), false);
    settings.resetSubtitleAppearance();
    require(settings.value(QStringLiteral("subtitles/styling")).toString() == QStringLiteral("Auto"),
        "subtitle appearance reset did not restore automatic styling");
    require(settings.value(QStringLiteral("subtitles/textSize")).toString().isEmpty(),
        "subtitle appearance reset did not restore normal text size");
    require(settings.value(QStringLiteral("subtitles/textColor")).toString() == QStringLiteral("#ffffff"),
        "subtitle appearance reset did not restore white text");
    require(settings.value(QStringLiteral("subtitles/dimInHdr")).toBool(),
        "subtitle appearance reset did not restore HDR dimming");

    settings.completePlayerControlTooltipSession();
    settings.completePlayerControlTooltipSession();
    settings.completePlayerControlTooltipSession();
    require(
        !settings.playerControlTooltipsEnabled(), "control tooltips remained enabled after three playback sessions");
    require(QCoro::waitFor(database.loadSettingAsync(QStringLiteral("player/controlTooltipSessions")))
            == QStringLiteral("3"),
        "completed control-tooltip sessions were not persisted");

    SettingsController restored(&database, nullptr, nullptr);
    QCoro::waitFor(restored.loadLocalAsync());
    require(restored.uiScalePercent() == 135, "persisted UI scale was not restored");
    require(restored.uiScaleSetupComplete(), "persisted setup completion was not restored");
    require(restored.audioDelayMs() == 120, "persisted global desktop audio delay was not restored");
    require(restored.value(QStringLiteral("playback/forwardCacheSizeMiB")).toString() == QStringLiteral("256"),
        "persisted forward cache size was not restored");
    require(!restored.playerControlTooltipsEnabled(), "persisted control-tooltip sessions were not restored");

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
