#include "NavigationState.h"

namespace JellyfinNative {

QString NavigationState::page() const
{
    return m_page;
}

void NavigationState::reset()
{
    m_page = QStringLiteral("login");
}

bool NavigationState::setPage(const QString& page)
{
    if (m_page == page)
        return false;
    m_page = page;
    return true;
}

} // namespace JellyfinNative
