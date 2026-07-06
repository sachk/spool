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
    property var snapshotProvider: null

    property string movieId: ""
    property string title: ""
    property string displayTitle: ""
    property string displaySubtitle: ""
    property string subtitle: ""
    property string posterUrl: ""
    property string seriesPosterUrl: ""
    property string thumbUrl: ""
    property string landscapeCardUrl: ""
    property string backdropUrl: ""
    property string itemType: ""
    property string seriesName: ""
    property string seriesId: ""
    property string seasonId: ""
    property int year: 0
    property int seasonNumber: 0
    property real progress: 0
    property real resumeTicks: 0
    property bool favorite: false
    property bool played: false
    property bool playable: true

    readonly property bool posterKind: kind === "poster"
    readonly property real metadataHeight: metadataLabel.text.length > 0 ? metadataLabel.implicitHeight : 0
    readonly property real artHeight: posterKind ? width * 1.5 : width * 9 / 16
    readonly property real titleAvailableHeight: Math.max(0, height - art.height - 10 - metadataHeight)

    signal activated()
    signal detailsRequested()
    signal favoriteToggled(bool favorite)
    signal playedToggled(bool played)
    signal mediaInfoRequested()

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

    function snapshot() {
        return snapshotProvider ? (snapshotProvider() || ({})) : ({
            movieId: movieId,
            title: title,
            displayTitle: displayTitle,
            displaySubtitle: displaySubtitle,
            subtitle: subtitle,
            posterUrl: posterUrl,
            seriesPosterUrl: seriesPosterUrl,
            thumbUrl: thumbUrl,
            landscapeCardUrl: landscapeCardUrl,
            backdropUrl: backdropUrl,
            itemType: itemType,
            seriesName: seriesName,
            seriesId: seriesId,
            seasonId: seasonId,
            year: year,
            seasonNumber: seasonNumber,
            progress: progress,
            resumeTicks: resumeTicks,
            favorite: favorite,
            played: played,
            playable: playable
        })
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

    ImageCard {
        id: art
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: root.artHeight
        imageUrl: root.posterKind ? root.posterImage() : root.landscapeImage()
        fallbackText: root.posterKind
                      ? (root.year > 0 ? String(root.year) : (root.itemType.length > 0 ? root.itemType : "Poster"))
                      : (root.subtitleText().length > 0 ? root.subtitleText() : root.itemType)
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
        movieId: root.movieId
        itemType: root.itemType
        favorite: root.favorite
        played: root.played
        resumeTicks: root.resumeTicks
        longPressAction: root.longPressAction
        snapshotProvider: root.snapshot
        onActivated: root.activated()
        onDetailsRequested: root.detailsRequested()
        onFavoriteToggled: (favorite) => root.favoriteToggled(favorite)
        onPlayedToggled: (played) => root.playedToggled(played)
        onMediaInfoRequested: root.mediaInfoRequested()
    }
}
