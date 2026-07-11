pragma Singleton
import QtQuick

QtObject {
    readonly property int uiScalePercent: Math.max(95, Math.min(140, Number(Settings.uiScalePercent || 115)))
    readonly property real uiScale: uiScalePercent / 100

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

    function topBarHeight(width) {
        return scaled([52, 56, 62, 72][densityForWidth(width)])
    }
    function pageMargin(width) {
        return scaled([24, 32, 44, 64][densityForWidth(width)])
    }
    function gap(width) {
        return scaled([14, 18, 22, 28][densityForWidth(width)])
    }
    function controlHeight(width) {
        return scaled([42, 46, 48, 54][densityForWidth(width)])
    }
    function detailRowPosterWidth(width) {
        return scaled([132, 152, 176, 208][densityForWidth(width)])
    }
    function detailHeroHeight(height) {
        return Math.max(scaled(420), Math.min(scaled(660), Math.round(height * 0.64)))
    }
    function sectionGap(width) {
        return scaled([22, 26, 28, 34][densityForWidth(width)])
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

    function iconPx(width) {
        return scaled([18, 20, 22, 26][densityForWidth(width)])
    }

    function titlePx(width) {
        return scaled([26, 30, 34, 42][densityForWidth(width)])
    }
    function bodyPx(width) {
        return scaled([14, 15, 16, 19][densityForWidth(width)])
    }
    function metaPx(width) {
        return scaled([12, 13, 14, 16][densityForWidth(width)])
    }
}
