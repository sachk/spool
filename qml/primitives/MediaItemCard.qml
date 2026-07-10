import QtQuick
import "../theme"

Item {
    id: root

    property var shell
    property var item: ({})
    property string kind: "poster"
    property string titleOverride: ""
    property string subtitleOverride: ""
    property string imageOverride: ""
    property string fallbackOverride: ""
    property bool focused: false
    property bool useSeriesPoster: false
    property bool preferEpisodeTitle: false
    property real progress: -1

    readonly property bool posterKind: kind === "poster"
    readonly property real metadataHeight: metadataLabel.text.length > 0 ? metadataLabel.implicitHeight : 0
    readonly property real artHeight: posterKind ? width * 1.5 : width * 9 / 16
    readonly property real titleAvailableHeight: Math.max(0, height - art.height - Metrics.scaled(10) - metadataHeight)
    readonly property real effectiveProgress: playbackProgress()

    signal activated

    clip: true

    function text(field) {
        return item && item[field] !== undefined && item[field] !== null ? String(item[field]) : ""
    }

    function titleText() {
        if (titleOverride.length > 0)
            return titleOverride
        const title = text("title")
        const seriesName = text("seriesName")
        if (preferEpisodeTitle && text("itemType") === "Episode")
            return title
        if (text("itemType") === "Episode" && seriesName.length > 0)
            return seriesName
        return title || seriesName
    }

    function subtitleText() {
        if (subtitleOverride.length > 0)
            return subtitleOverride
        const subtitle = text("subtitle")
        const title = text("title")
        if (text("itemType") === "Episode") {
            if (preferEpisodeTitle)
                return subtitle
            if (subtitle.length > 0 && title.length > 0)
                return subtitle + " · " + title
            if (title.length > 0)
                return title
        }
        const year = Number(item && item.year || 0)
        return subtitle || (year > 0 ? String(year) : "")
    }

    function imageSource() {
        if (imageOverride.length > 0)
            return imageOverride
        const artKind = posterKind && useSeriesPoster && text("itemType") === "Episode" ? "seriesPoster" : kind
        return Art.url(item, artKind, Math.ceil(width))
    }

    function fallbackText() {
        if (fallbackOverride.length > 0)
            return fallbackOverride
        const type = text("itemType")
        if (!posterKind)
            return subtitleText() || type
        const year = Number(item && item.year || 0)
        return year > 0 ? String(year) : (type || "Poster")
    }

    function playbackProgress() {
        if (progress >= 0)
            return Math.max(0, Math.min(1, progress))
        const resumeTicks = Number(item && item.resumeTicks || 0)
        const runtimeTicks = Number(item && item.runtimeTicks || 0)
        return resumeTicks > 0 && runtimeTicks > 0 ? Math.max(0, Math.min(1, resumeTicks / runtimeTicks)) : 0
    }

    ImageCard {
        id: art
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: root.artHeight
        imageUrl: root.imageSource()
        fallbackText: root.fallbackText()
        focused: root.focused
        retainWhileLoading: !root.posterKind
    }

    Rectangle {
        anchors.left: art.left
        anchors.right: art.right
        anchors.bottom: art.bottom
        height: Metrics.scaled(4)
        visible: !root.posterKind && root.effectiveProgress > 0
        color: "#66000000"

        Rectangle {
            width: parent.width * root.effectiveProgress
            height: parent.height
            color: Theme.accent
        }
    }

    AppText {
        id: titleLabel
        anchors.top: art.bottom
        anchors.topMargin: Metrics.scaled(8)
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
        anchors.topMargin: Metrics.scaled(2)
        anchors.left: parent.left
        anchors.right: parent.right
        visible: text.length > 0 && root.height > y
        text: root.subtitleText()
        color: Theme.textMuted
        font.pixelSize: Metrics.metaPx(root.Window.window ? root.Window.window.width : 1920)
        elide: Text.ElideRight
    }

    MediaItemActions {
        anchors.fill: parent
        shell: root.shell
        item: root.item
        onActivated: root.activated()
    }
}
