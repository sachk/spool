import QtQuick
import "../theme"

Item {
    id: root

    property var shell
    property string kind: "poster"
    property bool focused: false
    property bool useSeriesPoster: false
    property bool preferEpisodeTitle: false
    property string longPressAction: "menu"
    property var item: ({})
    property var itemProvider: null
    property string displayTitle: ""
    property string displaySubtitle: ""
    property real progress: 0

    readonly property string movieId: String(item.movieId || "")
    readonly property string title: String(item.title || "")
    readonly property string subtitle: String(item.subtitle || "")
    readonly property string posterUrl: String(item.posterUrl || "")
    readonly property string seriesPosterUrl: String(item.seriesPosterUrl || "")
    readonly property string thumbUrl: String(item.thumbUrl || "")
    readonly property string landscapeCardUrl: String(item.landscapeCardUrl || "")
    readonly property string backdropUrl: String(item.backdropUrl || "")
    readonly property string itemType: String(item.itemType || "")
    readonly property string seriesName: String(item.seriesName || "")
    readonly property string seriesId: String(item.seriesId || "")
    readonly property string seasonId: String(item.seasonId || "")
    readonly property int year: Number(item.year || 0)
    readonly property int seasonNumber: Number(item.seasonNumber || 0)
    readonly property real resumeTicks: Number(item.resumeTicks || 0)
    readonly property bool favorite: Boolean(item.favorite)
    readonly property bool played: Boolean(item.played)
    readonly property bool playable: item.playable === undefined || item.playable

    readonly property bool posterKind: kind === "poster"
    readonly property real metadataHeight: metadataLabel.text.length > 0 ? metadataLabel.implicitHeight : 0
    readonly property real artHeight: posterKind ? width * 1.5 : width * 9 / 16
    readonly property real titleAvailableHeight: Math.max(0, height - art.height - 10 - metadataHeight)

    signal activated
    signal detailsRequested
    signal favoriteToggled(bool favorite)
    signal playedToggled(bool played)
    signal mediaInfoRequested

    clip: true

    function titleText() {
        if (preferEpisodeTitle && itemType === "Episode" && title.length > 0)
            return title
        return displayTitle || title || seriesName || ""
    }

    function subtitleText() {
        if (preferEpisodeTitle && itemType === "Episode")
            return subtitle || ""
        return displaySubtitle || subtitle || (year > 0 ? String(year) : "")
    }

    function posterImage() {
        if (useSeriesPoster && itemType === "Episode" && seriesPosterUrl.length > 0)
            return seriesPosterUrl
        return posterUrl || seriesPosterUrl || thumbUrl || ""
    }

    function landscapeImage() {
        return landscapeCardUrl || thumbUrl || backdropUrl || posterUrl || seriesPosterUrl || ""
    }

    function providedItem() {
        return itemProvider ? (itemProvider() || ({})) : (item || ({}))
    }

    function handleAcceptPressed(key) {
        return actions.handleAcceptPressed(key)
    }

    function handleAcceptReleased(key) {
        return actions.handleAcceptReleased(key)
    }

    ImageCard {
        id: art
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: root.artHeight
        imageUrl: root.posterKind ? root.posterImage() : root.landscapeImage()
        fallbackText: root.posterKind ? (root.year > 0 ? String(root.year) : (root.itemType.length > 0 ? root.itemType :
                                                                                                         "Poster")) : (
                                            root.subtitleText().length > 0 ? root.subtitleText() : root.itemType)
        focused: root.focused
        retainWhileLoading: !root.posterKind
    }

    Rectangle {
        anchors.left: art.left
        anchors.right: art.right
        anchors.bottom: art.bottom
        height: 4
        visible: !root.posterKind && root.progress > 0
        color: "#66000000"
        Rectangle {
            width: parent.width * Math.max(0, Math.min(1, root.progress))
            height: parent.height
            color: Theme.accent
        }
    }

    AppText {
        id: titleLabel
        anchors.top: art.bottom
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.right: parent.right
        visible: text.length > 0 && root.titleAvailableHeight > 0
        text: root.titleText()
        font.pixelSize: Metrics.bodyPx(root.Window.window ? root.Window.window.width : 1920)
        font.weight: Font.Medium
        color: root.posterKind && !root.focused ? Theme.textSecondary : Theme.textPrimary
        maximumLineCount: root.titleAvailableHeight >= font.pixelSize * 2.25 ? 2 : 1
        clip: true
        elide: Text.ElideRight
    }

    MonoText {
        id: metadataLabel
        anchors.top: titleLabel.bottom
        anchors.topMargin: 2
        anchors.left: parent.left
        anchors.right: parent.right
        visible: text.length > 0 && root.height > y
        text: root.subtitleText()
        color: Theme.textMuted
        font.pixelSize: Metrics.metaPx(root.Window.window ? root.Window.window.width : 1920)
        elide: Text.ElideRight
    }

    MediaItemActions {
        id: actions
        anchors.fill: parent
        shell: root.shell
        focused: root.focused
        item: root.item
        longPressAction: root.longPressAction
        itemProvider: root.providedItem
        onActivated: root.activated()
        onDetailsRequested: root.detailsRequested()
        onFavoriteToggled: favorite => root.favoriteToggled(favorite)
        onPlayedToggled: played => root.playedToggled(played)
        onMediaInfoRequested: root.mediaInfoRequested()
    }
}
