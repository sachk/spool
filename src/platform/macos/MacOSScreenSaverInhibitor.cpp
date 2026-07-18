#include "platform/ScreenSaverInhibitor.h"

#include <QDebug>

#include <IOKit/pwr_mgt/IOPMLib.h>

namespace JellyfinNative {

struct ScreenSaverInhibitor::PlatformData {
    IOPMAssertionID assertion = kIOPMNullAssertionID;
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
    if (inhibited) {
        const IOReturn result = IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleDisplaySleep,
            kIOPMAssertionLevelOn, CFSTR("Jellyfin Native media playback"), &m_platform->assertion);
        if (result != kIOReturnSuccess) {
            qWarning() << "screensaver: macOS idle assertion failed" << result;
            return;
        }
    } else if (m_platform->assertion != kIOPMNullAssertionID) {
        IOPMAssertionRelease(m_platform->assertion);
        m_platform->assertion = kIOPMNullAssertionID;
    }
    m_inhibited = inhibited;
    qInfo() << "screensaver:" << (inhibited ? "inhibited for active playback" : "available while paused or idle");
}

bool ScreenSaverInhibitor::inhibited() const
{
    return m_inhibited;
}

} // namespace JellyfinNative
