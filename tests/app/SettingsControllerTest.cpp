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
    require(settings.uiScalePercent() == 100, "UI scale default was not 100 percent");
    require(!settings.uiScaleSetupComplete(), "fresh profile unexpectedly skipped scale setup");

    settings.setUiScalePercent(65);
    require(settings.uiScalePercent() == 80, "UI scale setter did not clamp to its lower bound");
    settings.completeUiScaleSetup(115);
    require(settings.uiScalePercent() == 115, "setup did not apply the selected scale");
    require(settings.uiScaleSetupComplete(), "setup completion marker was not exposed");

    require(
        QCoro::waitFor(database.loadSettingAsync(QStringLiteral("appearance/uiScalePercent"))) == QStringLiteral("115"),
        "selected UI scale was not persisted");
    require(QCoro::waitFor(database.loadSettingAsync(QStringLiteral("appearance/uiScaleSetupVersion")))
            == QStringLiteral("1"),
        "scale setup completion was not persisted");

    SettingsController restored(&database, nullptr, nullptr);
    QCoro::waitFor(restored.loadLocalAsync());
    require(restored.uiScalePercent() == 115, "persisted UI scale was not restored");
    require(restored.uiScaleSetupComplete(), "persisted setup completion was not restored");

    database.shutdown();
    return EXIT_SUCCESS;
}
