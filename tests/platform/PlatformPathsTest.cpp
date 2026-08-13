#include "platform/PlatformPaths.h"

#include "TestMain.h"

#include <QDir>

#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
}

} // namespace

JELLYFIN_TEST_MAIN("platform-paths")
{
    const QString appRoot = QDir::cleanPath(QDir::temp().filePath(QStringLiteral("spool-package-root")));
    require(JellyfinNative::bundledFontsPath(appRoot) == QDir(appRoot).filePath(QStringLiteral("fonts")),
        "bundled fonts must resolve to the platform-neutral appRoot/fonts directory");
    return EXIT_SUCCESS;
}
