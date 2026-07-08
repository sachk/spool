pragma Singleton
import QtQuick

QtObject {
    property int userColumnOverride: 0
    property real userUiScale: 1.0
    property int userPosterSizeBias: 0

    function densityForWidth(w) {
        if (w >= 3840)
            return 3
        if (w >= 1920)
            return 2
        if (w >= 1280)
            return 1
        return 0
    }

    function topBarHeight(w) {
        return [52, 56, 62, 72][densityForWidth(w)]
    }
    function pageMargin(w) {
        return [24, 32, 44, 64][densityForWidth(w)]
    }
    function gap(w) {
        return [14, 18, 22, 28][densityForWidth(w)]
    }
    function controlHeight(w) {
        return [42, 46, 48, 54][densityForWidth(w)]
    }
    function detailRowPosterWidth(w) {
        return [132, 152, 176, 208][densityForWidth(w)]
    }
    function detailHeroHeight(h) {
        return Math.max(500, Math.min(660, Math.round(h * 0.64)))
    }
    function sectionGap(w) {
        return [22, 26, 28, 34][densityForWidth(w)]
    }
    function homePosterWidth(w) {
        return [140, 180, 156, 180][densityForWidth(w)] + userPosterSizeBias * 18
    }
    function homeLandscapeWidth(w) {
        return [260, 300, 248, 248][densityForWidth(w)]
    }
    function autoColumns(w) {
        return [5, 6, 8, 12][densityForWidth(w)]
    }
    function columns(w) {
        return userColumnOverride > 0 ? userColumnOverride : autoColumns(w)
    }

    function titlePx(w) {
        return Math.round([26, 30, 34, 42][densityForWidth(w)] * userUiScale)
    }
    function bodyPx(w) {
        return Math.round([14, 15, 16, 19][densityForWidth(w)] * userUiScale)
    }
    function metaPx(w) {
        return Math.round([12, 13, 14, 16][densityForWidth(w)] * userUiScale)
    }
}
