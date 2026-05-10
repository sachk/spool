pragma Singleton
import QtQuick

QtObject {
    property int userColumnOverride: 0
    property real userUiScale: 1.0
    property int userPosterSizeBias: 0

    function densityForWidth(w) {
        if (w >= 3840) return 3
        if (w >= 1920) return 2
        if (w >= 1280) return 1
        return 0
    }

    function railWidth(w) { return [48, 52, 56, 64][densityForWidth(w)] }
    function pageMargin(w) { return [24, 32, 44, 64][densityForWidth(w)] }
    function gap(w) { return [14, 18, 22, 28][densityForWidth(w)] }
    function basePosterWidth(w) { return [140, 180, 210, 240][densityForWidth(w)] + userPosterSizeBias * 24 }
    function homePosterWidth(w) { return [140, 180, 156, 180][densityForWidth(w)] + userPosterSizeBias * 18 }
    function homeLandscapeWidth(w) { return [260, 300, 248, 248][densityForWidth(w)] }
    function autoColumns(w) { return [5, 6, 8, 12][densityForWidth(w)] }
    function columns(w) { return userColumnOverride > 0 ? userColumnOverride : autoColumns(w) }

    function posterWidth(contentWidth, screenWidth) {
        const cols = columns(screenWidth)
        const g = gap(screenWidth)
        return Math.floor((contentWidth - g * (cols - 1)) / cols)
    }

    function titlePx(w) { return Math.round([26, 30, 34, 42][densityForWidth(w)] * userUiScale) }
    function bodyPx(w) { return Math.round([14, 15, 16, 19][densityForWidth(w)] * userUiScale) }
    function metaPx(w) { return Math.round([12, 13, 14, 16][densityForWidth(w)] * userUiScale) }
}
