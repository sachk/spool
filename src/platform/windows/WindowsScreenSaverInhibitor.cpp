#include "platform/ScreenSaverInhibitor.h"

#include <QDebug>

#include <windows.h>

namespace JellyfinNative {

struct ScreenSaverInhibitor::PlatformData { };

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
    const EXECUTION_STATE state = inhibited ? ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED : ES_CONTINUOUS;
    if (SetThreadExecutionState(state) == 0) {
        qWarning() << "screensaver: SetThreadExecutionState failed";
        return;
    }
    m_inhibited = inhibited;
    qInfo() << "screensaver:" << (inhibited ? "inhibited for active playback" : "available while paused or idle");
}

bool ScreenSaverInhibitor::inhibited() const
{
    return m_inhibited;
}

} // namespace JellyfinNative
