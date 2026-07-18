#include "platform/PlatformCapabilities.h"

namespace JellyfinNative {

const PlatformCapabilities& platformCapabilities()
{
    static const PlatformCapabilities capabilities {
        .deviceName = QStringLiteral("macOS Desktop"),
        .rendererName = QStringLiteral("libmpv OpenGL"),
    };
    return capabilities;
}

} // namespace JellyfinNative
