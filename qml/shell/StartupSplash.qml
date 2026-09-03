import QtQuick

// The frame Qt puts on screen before the shell exists, so it predates the
// singletons and reads the view's own context properties.
SplashContent {
    width: 1920
    height: 1080
    pixelsPerDp: startupSplashPixelsPerDp
    coreWidthDp: startupSplashCoreWidthDp
    coreWidthFraction: startupSplashCoreWidthFraction
    coreAspect: startupSplashCoreAspect
    coreSource: startupSplashImageUrl
}
