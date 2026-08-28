#pragma once

#include <QString>

namespace JellyfinNative {

struct PlatformCapabilities {
    QString deviceName;
    QString rendererName;
    bool isTV = false;
    bool isWebOS = false;
    bool isAndroid = false;
    bool isMobile = false;
    bool hasSystemFonts = true;
    bool hasDesktopPointer = true;
    bool supportsMpvConfig = true;
    bool usesPerOutputAudioDelay = false;
};

const PlatformCapabilities& platformCapabilities();

} // namespace JellyfinNative
