pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

OverlayDialog {
    id: root

    preferredWidth: 1120
    preferredHeight: 800
    padding: 28
    panelColor: Theme.bgRaised
    property var item: ({})
    property var shell
    property int currentActionIndex: 0
    readonly property string itemId: item && item.movieId ? String(item.movieId) : ""
    readonly property var detail: Content.detailItem && String(Content.detailItem.movieId || "") === itemId
                                  ? Content.detailItem : ({})
    readonly property var sources: detail.mediaSources || []
    readonly property bool showLinkVisible: Boolean(item && item.seriesId && item.seriesName && (item.itemType
                                                                                                 === "Episode"
                                                                                                 || item.itemType
                                                                                                 === "Season"))
    readonly property bool seasonLinkVisible: Boolean(item && item.itemType === "Episode" && item.seriesId
                                                      && item.seasonId)
    readonly property int showActionIndex: showLinkVisible ? 0 : -1
    readonly property int seasonActionIndex: seasonLinkVisible ? (showLinkVisible ? 1 : 0) : -1
    readonly property int closeActionIndex: (showLinkVisible ? 1 : 0) + (seasonLinkVisible ? 1 : 0)
    readonly property int actionCount: closeActionIndex + 1

    signal closed
    onDismissed: closeOverlay()

    onVisibleChanged: if (visible) {
        currentActionIndex = closeActionIndex
        Qt.callLater(ensureFocus)
        Qt.callLater(refreshItemDetail)
    }
    onItemChanged: if (visible)
    Qt.callLater(refreshItemDetail)
    onActiveFocusChanged: if (visible && !activeFocus)
    InputKeys.focus(root)

    component Pair: ColumnLayout {
        property string label: ""
        property string value: ""
        visible: value.length > 0
        spacing: 3
        MonoText {
            Layout.fillWidth: true
            text: label
            color: Theme.textMuted
            font.pixelSize: Metrics.metaPx(root.width) - 1
            elide: Text.ElideRight
        }
        AppText {
            Layout.fillWidth: true
            text: value
            color: Theme.textPrimary
            font.pixelSize: Metrics.metaPx(root.width) + 1
            font.weight: Font.Medium
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
    }

    function text(value) {
        return value === undefined || value === null ? "" : String(value)
    }
    function upper(value) {
        const v = text(value)
        return v.length > 0 ? v.toUpperCase() : ""
    }
    function titleText() {
        return item && (item.displayTitle || item.title) ? (item.displayTitle || item.title) : "Media info"
    }
    function seasonTitle() {
        if (item && item.seasonNumber > 0)
            return "Season " + item.seasonNumber
        const match = item && item.displaySubtitle ? String(item.displaySubtitle).match(/Season\s+\d+/i) : null
        return match ? match[0] : "Season"
    }
    function bitrate(bits) {
        const v = Number(bits || 0)
        if (v <= 0)
            return ""
        if (v >= 1000000)
            return (v / 1000000).toFixed(v >= 10000000 ? 0 : 1) + " Mbps"
        if (v >= 1000)
            return Math.round(v / 1000) + " kbps"
        return v + " bps"
    }
    function fileSize(bytes) {
        let value = Number(bytes || 0)
        if (value <= 0)
            return ""
        const units = ["B", "KB", "MB", "GB", "TB"]
        let unit = 0
        while (value >= 1024 && unit < units.length - 1) {
            value /= 1024
            ++unit
        }
        return value.toFixed(unit >= 3 ? 2 : unit === 0 ? 0 : 1) + " " + units[unit]
    }
    function runtime(ticks) {
        const minutes = Math.round(Number(ticks || 0) / 600000000)
        if (minutes <= 0)
            return ""
        const hours = Math.floor(minutes / 60)
        const mins = minutes % 60
        return hours > 0 ? hours + "h " + mins + "m" : mins + "m"
    }
    function ratingText() {
        const parts = []
        if (item && item.officialRating)
            parts.push(item.officialRating)
        const community = Number(item && item.communityRating ? item.communityRating : 0)
        const critic = Number(item && item.criticRating ? item.criticRating : 0)
        if (community > 0)
            parts.push(community.toFixed(1) + "/10")
        if (critic > 0)
            parts.push(Math.round(critic) + "%")
        return parts.join(" · ")
    }
    function codec(stream) {
        const parts = []
        if (stream.codec)
            parts.push(upper(stream.codec))
        if (stream.profile)
            parts.push(stream.profile)
        return parts.join(" / ")
    }
    function streamSummary(stream) {
        const parts = []
        const c = codec(stream)
        if (c.length > 0)
            parts.push(c)
        if (stream.width > 0 && stream.height > 0)
            parts.push(stream.width + "x" + stream.height)
        if (stream.frameRate > 0)
            parts.push(Number(stream.frameRate).toFixed(3).replace(/0+$/, "").replace(/\.$/, "") + " fps")
        if (stream.channels > 0)
            parts.push(stream.channels === 1 ? "Mono" : stream.channels === 2 ? "Stereo" : stream.channels
                                                                                + " channels")
        if (stream.sampleRate > 0)
            parts.push((Number(stream.sampleRate) / 1000).toFixed(stream.sampleRate % 1000 === 0 ? 0 : 1) + " kHz")
        if (stream.bitDepth > 0)
            parts.push(stream.bitDepth + "-bit")
        if (stream.videoRange)
            parts.push(stream.videoRange)
        if (stream.pixelFormat)
            parts.push(stream.pixelFormat)
        if (stream.language)
            parts.push(stream.language)
        if (stream.title)
            parts.push(stream.title)
        const br = bitrate(stream.bitRate)
        if (br.length > 0)
            parts.push(br)
        return parts.join(" · ")
    }
    function streamFlags(stream) {
        const flags = []
        if (stream.index >= 0)
            flags.push("Stream " + stream.index)
        if (stream.isDefault)
            flags.push("Default")
        if (stream.isForced)
            flags.push("Forced")
        if (stream.isExternal)
            flags.push("External")
        if (stream.isInterlaced)
            flags.push("Interlaced")
        return flags.join(" · ")
    }
    function sourceSummary(source) {
        const parts = []
        if (source.container)
            parts.push(upper(source.container))
        if (source.protocol)
            parts.push(source.protocol)
        if (source.videoType)
            parts.push(source.videoType)
        const run = runtime(source.runtimeTicks || (item ? item.runtimeTicks : 0))
        const size = fileSize(source.size)
        const br = bitrate(source.bitRate)
        if (run.length > 0)
            parts.push(run)
        if (size.length > 0)
            parts.push(size)
        if (br.length > 0)
            parts.push(br)
        return parts.join(" · ")
    }
    function sourcePairs(source) {
        return [
                    {
                        label: "Container",
                        value: upper(source.container)
                    },
                    {
                        label: "Protocol",
                        value: text(source.protocol)
                    },
                    {
                        label: "Size",
                        value: fileSize(source.size)
                    },
                    {
                        label: "Bitrate",
                        value: bitrate(source.bitRate)
                    },
                    {
                        label: "Runtime",
                        value: runtime(source.runtimeTicks || (item ? item.runtimeTicks : 0))
                    },
                    {
                        label: "Video type",
                        value: text(source.videoType)
                    },
                    {
                        label: "Path",
                        value: source.path || (item && item.path ? item.path : "")
                    }
                ].filter(function (pair) {
                    return pair.value.length > 0
                })
    }
    function actionAt(index) {
        if (index === showActionIndex)
            return showBtn
        if (index === seasonActionIndex)
            return seasonBtn
        return closeBtn
    }
    function focusAction(index) {
        currentActionIndex = Math.max(0, Math.min(actionCount - 1, index))
        const target = actionAt(currentActionIndex)
        if (target)
            InputKeys.focus(target)
    }
    function ensureFocus() {
        InputKeys.focus(root)
        focusAction(currentActionIndex)
    }
    function refreshItemDetail() {
        if (itemId.length > 0)
            Content.loadItemDetail(itemId)
    }
    function closeOverlay() {
        root.closed()
    }
    function openSeries() {
        if (!item || !item.seriesId)
            return
        if (shell && shell.replaceRoute)
            shell.replaceRoute("libraryGrid")
        root.closed()
        App.openSeriesById(String(item.seriesId), String(item.seriesName || ""))
    }
    function openSeason() {
        if (!item || !item.seriesId || !item.seasonId)
            return
        if (shell && shell.replaceRoute)
            shell.replaceRoute("libraryGrid")
        root.closed()
        App.openSeasonById(String(item.seriesId), String(item.seasonId), seasonTitle())
    }
    function activateCurrent() {
        if (currentActionIndex === showActionIndex)
            openSeries()
        else if (currentActionIndex === seasonActionIndex)
            openSeason()
        else
            closeOverlay()
    }
    function routeKey(key, phase, repeat) {
        if (phase === "release" && key === Qt.Key_I) {
            closeOverlay()
            return true
        }
        if (key === Qt.Key_Up || key === Qt.Key_Left) {
            focusAction(currentActionIndex - 1)
            return true
        }
        if (key === Qt.Key_Down || key === Qt.Key_Right) {
            focusAction(currentActionIndex + 1)
            return true
        }
        return false
    }

    function activate() {
        activateCurrent()
    }

    function back() {
        closeOverlay()
        return true
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 12
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 5
            AppText {
                Layout.fillWidth: true
                text: root.titleText()
                font.pixelSize: Metrics.titlePx(root.width)
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
            TechMetadataLine {
                Layout.fillWidth: true
                metadata: [item.itemType || "", item.year > 0 ? String(item.year) : "", item.displaySubtitle || "", root.ratingText(
                        )].filter(function (v) {
                            return v.length > 0
                        }).join(" · ")
            }
        }
        ActionButton {
            id: showBtn
            visible: root.showLinkVisible
            text: "Show"
            iconName: "live_tv"
            focus: root.currentActionIndex === root.showActionIndex
            onClicked: root.openSeries()
            onActiveFocusChanged: if (activeFocus)
            root.currentActionIndex = root.showActionIndex
        }
        ActionButton {
            id: seasonBtn
            visible: root.seasonLinkVisible
            text: root.seasonTitle()
            iconName: "video_library"
            focus: root.currentActionIndex === root.seasonActionIndex
            onClicked: root.openSeason()
            onActiveFocusChanged: if (activeFocus)
            root.currentActionIndex = root.seasonActionIndex
        }
        ActionButton {
            id: closeBtn
            text: "Close"
            iconName: "close"
            focus: root.currentActionIndex === root.closeActionIndex
            onClicked: root.closeOverlay()
            onActiveFocusChanged: if (activeFocus)
            root.currentActionIndex = root.closeActionIndex
        }
    }

    Flickable {
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: width
        contentHeight: infoColumn.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        FastWheelHandler {
            flickable: parent
        }

        ColumnLayout {
            id: infoColumn
            width: parent.width
            spacing: 18

            EmptyPlaceholder {
                Layout.fillWidth: true
                visible: root.sources.length === 0
                title: "No media source data"
                detail: "This item did not include codec or stream metadata in the loaded Jellyfin response."
            }

            Repeater {
                model: root.sources
                Surface {
                    required property var modelData
                    readonly property var source: modelData || ({})
                    Layout.fillWidth: true
                    Layout.preferredHeight: sourceColumn.implicitHeight + 24
                    baseColor: Theme.bgPanel

                    ColumnLayout {
                        id: sourceColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 12
                        spacing: 12
                        SectionHeader {
                            Layout.fillWidth: true
                            title: source.name && source.name.length > 0 ? source.name : "Media source"
                            detail: root.sourceSummary(source)
                        }
                        GridLayout {
                            Layout.fillWidth: true
                            columns: root.width >= 1400 ? 4 : 2
                            columnSpacing: 18
                            rowSpacing: 12
                            Repeater {
                                model: root.sourcePairs(source)
                                Pair {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    label: modelData.label
                                    value: modelData.value
                                }
                            }
                        }
                        Repeater {
                            model: source.streams || []
                            Surface {
                                required property var modelData
                                readonly property var stream: modelData || ({})
                                Layout.fillWidth: true
                                Layout.preferredHeight: streamColumn.implicitHeight + 20
                                baseColor: Theme.bg
                                ColumnLayout {
                                    id: streamColumn
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 6
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8
                                        MetadataChip {
                                            text: root.text(stream.type || "Stream")
                                            selected: true
                                        }
                                        AppText {
                                            Layout.fillWidth: true
                                            text: stream.displayTitle || root.streamSummary(stream) || root.text(
                                                      stream.type || "Stream")
                                            font.pixelSize: Metrics.bodyPx(root.width)
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                        }
                                    }
                                    TechMetadataLine {
                                        Layout.fillWidth: true
                                        metadata: [root.streamSummary(stream), root.streamFlags(stream)].filter(
                                            function (v) {
                                                return v.length > 0
                                            }).join(" · ")
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
