#include "app/NavigationState.h"

#include <QDebug>

#include <cstdlib>

using JellyfinNative::NavigationState;

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    qCritical() << message;
    std::exit(EXIT_FAILURE);
}

} // namespace

int main()
{
    NavigationState state;
    require(state.page() == QStringLiteral("login"), "default route is login");
    require(state.setPage(QStringLiteral("home")), "changing page reports true");
    require(state.page() == QStringLiteral("home"), "page changed");
    require(!state.setPage(QStringLiteral("home")), "same page reports false");
    state.reset();
    require(state.page() == QStringLiteral("login"), "reset returns to login");
    return EXIT_SUCCESS;
}
