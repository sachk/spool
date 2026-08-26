pragma Singleton
import QtQuick

QtObject {
    property real refWidth: 1920
    readonly property int uiScalePercent: Math.max(80, Math.min(180, Number(Settings.uiScalePercent || 100)))
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
    readonly property real landscapeCardRatio: 1.6
    // Where chrome sits at rest: a 1080p 16:9 window, the size the player's
    // controls were drawn against.
    readonly property real chromeBaselinePx: 1440
    readonly property real chromeBaselineScale: 0.78

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
    // One card width serves the library grid and every row that quotes it, so a
    // home row and the grid behind it land on the same size at any window size
    // rather than each rounding a table of their own.
    function cardWidth(width) {
        const count = columns(width)
        const available = Math.max(1, width - pageMargin(width) * 2)
        return Math.max(1, Math.floor((available - gap(width) * (count - 1)) / count))
    }
    // A 16:9 card beside a 2:3 one at landscapeCardRatio stands exactly three
    // fifths as tall, which is the proportion the rows have always had.
    function landscapeCardWidth(width) {
        return Math.round(cardWidth(width) * landscapeCardRatio)
    }
    function columns(width) {
        const density = densityForWidth(width)
        const targetWidth = scaled([120, 176, 190, 290][density])
        const available = Math.max(targetWidth, width - pageMargin(width) * 2)
        return Math.max(2, Math.min(12, Math.floor((available + gap(width)) / (targetWidth + gap(width)))))
    }
    // Chrome that floats over content — the player's controls — sizes off the
    // window's shape rather than its height alone, so it keeps its proportion
    // to a page's cards when the window is made narrow instead of short. The
    // geometric mean is that shape in one number, and chrome follows its cube
    // root: a window twice the size carries controls about a quarter larger,
    // which is enough to stay in proportion without a small window shrinking
    // them past reading or a large one turning them into furniture.
    function chromeScale(width, height) {
        const reference = Math.sqrt(Math.max(1, width) * Math.max(1, height))
        return Math.max(0.65, Math.min(1, chromeBaselineScale * Math.cbrt(reference / chromeBaselinePx)))
    }
}
