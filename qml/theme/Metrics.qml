pragma Singleton
import QtQuick

QtObject {
    property real refWidth: 1920
    readonly property int uiScalePercent: Math.max(80, Math.min(180, Number(Settings.uiScalePercent || 115)))
    readonly property real uiScale: uiScalePercent / 100
    readonly property int density: densityForWidth(refWidth)
    readonly property int topBarHeightPx: scaled([52, 56, 62, 72][density])
    // Page margins track the zoom (uiScale) with only a slight density ramp;
    // proportional-to-screen margins looked far too wide on 4K panels.
    readonly property int pageMarginPx: scaled([24, 28, 30, 32][density])
    readonly property int gapPx: scaled([14, 18, 22, 28][density])
    readonly property int controlHeightPx: scaled([42, 46, 48, 54][density])
    readonly property int sectionGapPx: scaled([22, 26, 28, 34][density])
    readonly property int iconSizePx: scaled([18, 20, 22, 26][density])
    readonly property int titleSizePx: scaled([26, 30, 34, 42][density])
    readonly property int bodySizePx: scaled([14, 15, 16, 19][density])
    readonly property int metaSizePx: scaled([12, 13, 14, 16][density])

    function scaled(value) {
        return Math.max(1, Math.round(value * uiScale))
    }

    function densityForWidth(width) {
        if (width >= 3840)
            return 3
        if (width >= 1920)
            return 2
        if (width >= 1280)
            return 1
        return 0
    }

    function pageMargin(width) {
        return scaled([24, 28, 30, 32][densityForWidth(width)])
    }
    function gap(width) {
        return scaled([14, 18, 22, 28][densityForWidth(width)])
    }
    function detailRowPosterWidth(width) {
        return scaled([132, 152, 176, 208][densityForWidth(width)])
    }
    function detailHeroHeight(height) {
        return Math.max(scaled(420), Math.min(scaled(660), Math.round(height * 0.64)))
    }
    function homePosterWidth(width) {
        return scaled([140, 180, 156, 180][densityForWidth(width)])
    }
    function homeLandscapeWidth(width) {
        return scaled([260, 300, 248, 248][densityForWidth(width)])
    }
    function columns(width) {
        const density = densityForWidth(width)
        const targetWidth = scaled([120, 176, 190, 290][density])
        const available = Math.max(targetWidth, width - pageMargin(width) * 2)
        return Math.max(2, Math.min(12, Math.floor((available + gap(width)) / (targetWidth + gap(width)))))
    }
}
