import QtQuick
import "../theme"

Item {
    id: root

    property var item: ({})
    property string kind: "poster"
    property bool focused: false
    property bool useSeriesPoster: false
    property string fallbackTitle: item.itemType || "Media"

    signal activated()
    signal favoriteToggled(bool favorite)
    signal playedToggled(bool played)
    signal mediaInfoRequested()

    function titleText() {
        return item.displayTitle || item.title || item.seriesName || ""
    }

    function subtitleText() {
        return item.displaySubtitle || item.subtitle || (item.year > 0 ? String(item.year) : "")
    }

    function posterImage() {
        if (useSeriesPoster && item.itemType === "Episode" && item.seriesPosterUrl)
            return item.seriesPosterUrl
        return item.posterUrl || item.seriesPosterUrl || item.thumbUrl || ""
    }

    function landscapeImage() {
        return item.thumbUrl || item.backdropUrl || item.posterUrl || item.seriesPosterUrl || ""
    }

    function handleAcceptPressed(key) {
        return actions.handleAcceptPressed(key)
    }

    function handleAcceptReleased(key) {
        return actions.handleAcceptReleased(key)
    }

    function handleNavigationKey(key) {
        return actions.handleNavigationKey(key)
    }

    PosterCard {
        anchors.fill: parent
        visible: root.kind === "poster"
        title: root.titleText()
        posterUrl: root.posterImage()
        year: root.item.year || 0
        metadata: root.subtitleText()
        focused: root.focused
    }

    LandscapeCard {
        anchors.fill: parent
        visible: root.kind !== "poster"
        title: root.titleText()
        subtitle: root.subtitleText()
        imageUrl: root.landscapeImage()
        progress: root.item.progress || 0
        focused: root.focused
    }

    MediaItemActions {
        id: actions
        item: root.item
        focused: root.focused
        onActivated: root.activated()
        onFavoriteToggled: (favorite) => root.favoriteToggled(favorite)
        onPlayedToggled: (played) => root.playedToggled(played)
        onMediaInfoRequested: root.mediaInfoRequested()
    }
}
