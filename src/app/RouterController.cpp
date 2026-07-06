#include "RouterController.h"

namespace JellyfinNative {

RouterController::RouterController(QObject *parent) : QObject(parent) {}

QString RouterController::route() const { return m_route; }

QString RouterController::previousRoute() const { return m_previousRoute; }

QVariantMap RouterController::args() const { return m_args; }

QVariantList RouterController::stack() const { return m_stack; }

bool RouterController::canPop() const { return !m_stack.isEmpty(); }

QVariantMap RouterController::frame(const QString &route, const QVariantMap &args) const
{
    return {{QStringLiteral("route"), route}, {QStringLiteral("args"), args}};
}

void RouterController::setFrame(const QString &route, const QVariantMap &args,
                                const QString &previousRoute)
{
    const bool routeChanged = route != m_route || args != m_args || previousRoute != m_previousRoute;
    if (!routeChanged)
        return;
    m_previousRoute = previousRoute;
    m_route = route.isEmpty() ? QStringLiteral("home") : route;
    m_args = args;
    emit this->routeChanged();
}

void RouterController::reset(const QString &route, const QVariantMap &args)
{
    const bool hadStack = !m_stack.isEmpty();
    m_stack.clear();
    setFrame(route, args, QStringLiteral("home"));
    if (hadStack)
        emit stackChanged();
}

void RouterController::push(const QString &route, const QVariantMap &args)
{
    if (route.isEmpty() || (route == m_route && args == m_args))
        return;
    const QString previous = m_route;
    m_stack.push_back(frame(m_route, m_args));
    emit stackChanged();
    setFrame(route, args, previous);
}

void RouterController::replace(const QString &route, const QVariantMap &args)
{
    if (route.isEmpty())
        return;
    setFrame(route, args, m_route);
}

bool RouterController::pop(const QString &fallbackRoute)
{
    if (m_stack.isEmpty()) {
        replace(fallbackRoute.isEmpty() ? QStringLiteral("home") : fallbackRoute);
        return false;
    }
    const QVariantMap top = m_stack.takeLast().toMap();
    emit stackChanged();
    setFrame(top.value(QStringLiteral("route")).toString(),
             top.value(QStringLiteral("args")).toMap(),
             m_route);
    return true;
}

void RouterController::clearStack()
{
    if (m_stack.isEmpty())
        return;
    m_stack.clear();
    emit stackChanged();
}

void RouterController::setArgs(const QVariantMap &args)
{
    setFrame(m_route, args, m_previousRoute);
}

} // namespace JellyfinNative
