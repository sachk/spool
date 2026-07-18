#include "platform/PlatformCapabilities.h"

namespace JellyfinNative {

const PlatformCapabilities& platformCapabilities()
{
    static const PlatformCapabilities capabilities {
        .deviceName = QStringLiteral("Linux Desktop"),
        .rendererName = QStringLiteral("libmpv OpenGL"),
    };
    return capabilities;
}

} // namespace JellyfinNative
