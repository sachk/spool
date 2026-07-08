#include "app/RouterController.h"

#include <QCoreApplication>
#include <QVariantMap>

#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    JellyfinNative::RouterController router;

    require(router.route() == QStringLiteral("login"), "initial route is login");
    router.reset(QStringLiteral("home"));
    require(router.route() == QStringLiteral("home"), "reset route");
    require(!router.canPop(), "reset clears stack");

    router.push(QStringLiteral("libraryGrid"), { { QStringLiteral("libraryId"), QStringLiteral("abc") } });
    require(router.route() == QStringLiteral("libraryGrid"), "push route");
    require(router.previousRoute() == QStringLiteral("home"), "push previous route");
    require(router.canPop(), "push creates stack frame");
    require(router.args().value(QStringLiteral("libraryId")).toString() == QStringLiteral("abc"), "push args");

    router.replace(QStringLiteral("libraries"));
    require(router.route() == QStringLiteral("libraries"), "replace route");
    require(router.previousRoute() == QStringLiteral("libraryGrid"), "replace previous route");
    require(router.canPop(), "replace keeps stack");

    router.push(QStringLiteral("itemDetails"), { { QStringLiteral("itemId"), QStringLiteral("m1") } });
    require(router.route() == QStringLiteral("itemDetails"), "second push route");
    require(router.previousRoute() == QStringLiteral("libraries"), "second push previous route");

    require(router.pop(QStringLiteral("home")), "pop returns stacked frame");
    require(router.route() == QStringLiteral("libraries"), "pop restores previous frame");
    require(router.canPop(), "first pop leaves original frame");
    require(router.pop(QStringLiteral("home")), "second pop returns stacked frame");
    require(router.route() == QStringLiteral("home"), "second pop restores home");
    require(!router.canPop(), "pop consumes stack");

    require(!router.pop(QStringLiteral("settings")), "empty pop uses fallback");
    require(router.route() == QStringLiteral("settings"), "empty pop fallback route");

    return 0;
}
