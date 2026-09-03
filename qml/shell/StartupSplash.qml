import QtQuick

// The frame Qt puts on screen before the shell exists. It is the same picture
// the shell's own launch overlay draws, so nothing moves when the shell
// replaces it.
SplashContent {
    width: 1920
    height: 1080
    pixelsPerDp: startupSplashPixelsPerDp
    coreWidthDp: startupSplashCoreWidthDp
    coreWidthFraction: startupSplashCoreWidthFraction
    coreAspect: startupSplashCoreAspect
    coreSource: startupSplashImageUrl
}
