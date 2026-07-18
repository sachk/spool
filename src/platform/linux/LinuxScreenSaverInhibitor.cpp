#include "platform/ScreenSaverInhibitor.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>

namespace JellyfinNative {

struct ScreenSaverInhibitor::PlatformData {
    quint32 cookie = 0;
};

ScreenSaverInhibitor::ScreenSaverInhibitor()
    : m_platform(std::make_unique<PlatformData>())
{
}

ScreenSaverInhibitor::~ScreenSaverInhibitor()
{
    setInhibited(false);
}

void ScreenSaverInhibitor::setInhibited(bool inhibited)
{
    if (m_inhibited == inhibited)
        return;
    QDBusInterface screenSaver(QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("/ScreenSaver"),
        QStringLiteral("org.freedesktop.ScreenSaver"), QDBusConnection::sessionBus());
    if (inhibited) {
        const QDBusReply<quint32> reply = screenSaver.call(
            QStringLiteral("Inhibit"), QStringLiteral("Jellyfin Native"), QStringLiteral("Media playback is active"));
        if (!reply.isValid()) {
            qWarning() << "screensaver: freedesktop inhibit failed" << reply.error().message();
            return;
        }
        m_platform->cookie = reply.value();
    } else if (m_platform->cookie != 0) {
        screenSaver.call(QStringLiteral("UnInhibit"), m_platform->cookie);
        m_platform->cookie = 0;
    }
    m_inhibited = inhibited;
    qInfo() << "screensaver:" << (inhibited ? "inhibited for active playback" : "available while paused or idle");
}

bool ScreenSaverInhibitor::inhibited() const
{
    return m_inhibited;
}

} // namespace JellyfinNative
