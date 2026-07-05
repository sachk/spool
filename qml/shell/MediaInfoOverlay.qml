import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root

    property var item: ({})
    property var shell
    property int currentActionIndex: closeActionIndex
    readonly property var sources: item && item.mediaSources ? item.mediaSources : []
    readonly property bool showLinkVisible: Boolean(item && item.seriesId && item.seriesName
                                                    && (item.itemType === "Episode" || item.itemType === "Season"))
    readonly property bool seasonLinkVisible: Boolean(item && item.itemType === "Episode"
                                                      && item.seriesId && item.seasonId)
    readonly property int showActionIndex: showLinkVisible ? 0 : -1
    readonly property int seasonActionIndex: seasonLinkVisible ? (showLinkVisible ? 1 : 0) : -1
    readonly property int closeActionIndex: (showLinkVisible ? 1 : 0) + (seasonLinkVisible ? 1 : 0)
    readonly property int actionCount: closeActionIndex + 1

    signal closed()

    focus: visible
    onVisibleChanged: if (visible) Qt.callLater(ensureFocus)
    onActiveFocusChanged: if (visible && !activeFocus) forceActiveFocus()

    component InfoPair: ColumnLayout {
        property string label: ""
        property string value: ""

        visible: value.length > 0
        spacing: 3

        MonoText {
            Layout.fillWidth: true
            text: label
            color: Theme.textMuted
            font.pixelSize: Metrics.metaPx(root.width) - 1
            maximumLineCount: 1
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

    component LinkRow: FocusScope {
        id: linkRoot
        property string label: ""
        property string value: ""
        property string iconName: "link"
        property int actionIndex: -1
        signal activated()

        Layout.fillWidth: true
        Layout.preferredHeight: 54
        visible: value.length > 0
        focus: root.currentActionIndex === actionIndex

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusMedium
            color: linkRoot.activeFocus || root.currentActionIndex === linkRoot.actionIndex ? Theme.focusedFill : Theme.bgPanel
            border.width: linkRoot.activeFocus || root.currentActionIndex === linkRoot.actionIndex ? Theme.focusBorderWidth : 1
            border.color: linkRoot.activeFocus || root.currentActionIndex === linkRoot.actionIndex ? Theme.accent : Theme.border
            antialiasing: true
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 12

            MaterialIcon {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                name: linkRoot.iconName
                iconSize: 23
                iconColor: linkRoot.activeFocus || root.currentActionIndex === linkRoot.actionIndex ? Theme.accent : Theme.textSecondary
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                MonoText {
                    Layout.fillWidth: true
                    text: linkRoot.label
                    color: Theme.textMuted
                    font.pixelSize: Metrics.metaPx(root.width) - 1
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }

                AppText {
                    Layout.fillWidth: true
                    text: linkRoot.value
                    color: Theme.textPrimary
                    font.pixelSize: Metrics.bodyPx(root.width)
                    font.weight: Font.DemiBold
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }
            }

            MaterialIcon {
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                name: "chevron_right"
                iconSize: 24
                iconColor: Theme.textSecondary
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onEntered: root.focusAction(linkRoot.actionIndex)
            onClicked: linkRoot.activated()
        }
    }

    component StreamCard: Surface {
        property var stream: ({})

        Layout.fillWidth: true
        Layout.preferredHeight: Math.max(94, streamColumn.implicitHeight + 24)
        baseColor: Theme.bgPanel

        ColumnLayout {
            id: streamColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 12
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                MetadataChip {
                    text: root.streamType(stream)
                    selected: true
                }

                AppText {
                    Layout.fillWidth: true
                    text: root.streamTitle(stream)
                    font.pixelSize: Metrics.bodyPx(root.width)
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }
            }

            AppText {
                Layout.fillWidth: true
                text: root.streamSummary(stream)
                color: Theme.textSecondary
                font.pixelSize: Metrics.metaPx(root.width) + 1
                wrapMode: Text.Wrap
                maximumLineCount: 3
                elide: Text.ElideRight
            }

            TechMetadataLine {
                Layout.fillWidth: true
                metadata: root.streamFlags(stream)
            }
        }
    }

    Keys.onPressed: (event) => {
        if (handlePressed(event))
            event.accepted = true
    }

    Keys.onReleased: (event) => {
        if (handleReleased(event))
            event.accepted = true
    }

    function textValue(value) {
        if (value === undefined || value === null)
            return ""
        return String(value)
    }

    function upper(value) {
        const text = textValue(value)
        return text.length > 0 ? text.toUpperCase() : ""
    }

    function titleText() {
        return item && (item.displayTitle || item.title) ? (item.displayTitle || item.title) : "Media info"
    }

    function seasonTitle() {
        if (!item)
            return "Season"
        if (item.seasonNumber > 0)
            return "Season " + item.seasonNumber
        if (item.displaySubtitle) {
            const text = String(item.displaySubtitle)
            const match = text.match(/Season\s+\d+/i)
            if (match)
                return match[0]
        }
        return "Season"
    }

    function ratingText() {
        const parts = []
        if (item && item.officialRating)
            parts.push(item.officialRating)
        const community = Number(item && item.communityRating ? item.communityRating : 0)
        if (community > 0)
            parts.push(community.toFixed(1) + "/10")
        const critic = Number(item && item.criticRating ? item.criticRating : 0)
        if (critic > 0)
            parts.push(Math.round(critic) + "%")
        return parts.join(" · ")
    }

    function communityRatingText() {
        const value = Number(item && item.communityRating ? item.communityRating : 0)
        return value > 0 ? value.toFixed(1) + "/10" : ""
    }

    function criticRatingText() {
        const value = Number(item && item.criticRating ? item.criticRating : 0)
        return value > 0 ? Math.round(value) + "%" : ""
    }

    function hasRatings() {
        return textValue(item && item.officialRating).length > 0
                || communityRatingText().length > 0
                || criticRatingText().length > 0
    }

    function bitrateText(bits) {
        const value = Number(bits || 0)
        if (value <= 0)
            return ""
        if (value >= 1000000)
            return (value / 1000000).toFixed(value >= 10000000 ? 0 : 1) + " Mbps"
        if (value >= 1000)
            return Math.round(value / 1000) + " kbps"
        return value + " bps"
    }

    function sizeText(bytes) {
        let value = Number(bytes || 0)
        if (value <= 0)
            return ""
        const units = ["B", "KB", "MB", "GB", "TB"]
        let unit = 0
        while (value >= 1024 && unit < units.length - 1) {
            value /= 1024
            unit += 1
        }
        return value.toFixed(unit >= 3 ? 2 : unit === 0 ? 0 : 1) + " " + units[unit]
    }

    function runtimeText(ticks) {
        const minutes = Math.round(Number(ticks || 0) / 600000000)
        if (minutes <= 0)
            return ""
        const hours = Math.floor(minutes / 60)
        const mins = minutes % 60
        return hours > 0 ? hours + "h " + mins + "m" : mins + "m"
    }

    function resolutionText(stream) {
        const width = Number(stream.width || 0)
        const height = Number(stream.height || 0)
        if (width <= 0 || height <= 0)
            return ""
        return width + "x" + height
    }

    function frameRateText(stream) {
        const rate = Number(stream.frameRate || 0)
        return rate > 0 ? rate.toFixed(rate >= 100 ? 0 : 3).replace(/0+$/, "").replace(/\.$/, "") + " fps" : ""
    }

    function channelsText(stream) {
        const channels = Number(stream.channels || 0)
        if (channels <= 0)
            return ""
        if (channels === 1)
            return "Mono"
        if (channels === 2)
            return "Stereo"
        return channels + " channels"
    }

    function sampleRateText(stream) {
        const hz = Number(stream.sampleRate || 0)
        return hz > 0 ? (hz / 1000).toFixed(hz % 1000 === 0 ? 0 : 1) + " kHz" : ""
    }

    function codecText(stream) {
        const parts = []
        if (stream.codec) parts.push(upper(stream.codec))
        if (stream.profile) parts.push(stream.profile)
        return parts.join(" / ")
    }

    function streamType(stream) {
        const type = textValue(stream.type)
        return type.length > 0 ? type : "Stream"
    }

    function streamTitle(stream) {
        if (stream.displayTitle) return stream.displayTitle
        const parts = []
        const codec = codecText(stream)
        if (codec.length > 0) parts.push(codec)
        if (stream.language) parts.push(stream.language)
        if (stream.title) parts.push(stream.title)
        return parts.length > 0 ? parts.join(" / ") : streamType(stream)
    }

    function streamSummary(stream) {
        const type = textValue(stream.type)
        const parts = []
        const codec = codecText(stream)
        if (codec.length > 0) parts.push(codec)
        if (type === "Video") {
            const resolution = resolutionText(stream)
            const frameRate = frameRateText(stream)
            if (resolution.length > 0) parts.push(resolution)
            if (frameRate.length > 0) parts.push(frameRate)
            if (stream.videoRange) parts.push(stream.videoRange)
            if (stream.pixelFormat) parts.push(stream.pixelFormat)
            if (stream.bitDepth > 0) parts.push(stream.bitDepth + "-bit")
            const color = [stream.colorPrimaries || "", stream.colorTransfer || "", stream.colorSpace || ""].filter(function(v) { return v.length > 0 }).join(" / ")
            if (color.length > 0) parts.push(color)
            if (stream.aspectRatio) parts.push(stream.aspectRatio)
        } else if (type === "Audio") {
            const channels = channelsText(stream)
            const sampleRate = sampleRateText(stream)
            if (channels.length > 0) parts.push(channels)
            if (sampleRate.length > 0) parts.push(sampleRate)
            if (stream.bitDepth > 0) parts.push(stream.bitDepth + "-bit")
        } else if (type === "Subtitle") {
            if (stream.language) parts.push(stream.language)
            if (stream.title) parts.push(stream.title)
            if (stream.isExternal) parts.push("External")
        }
        const bitrate = bitrateText(stream.bitRate)
        if (bitrate.length > 0) parts.push(bitrate)
        return parts.join(" · ")
    }

    function streamFlags(stream) {
        const flags = []
        if (stream.index >= 0) flags.push("Stream " + stream.index)
        if (stream.language) flags.push(stream.language)
        if (stream.isDefault) flags.push("Default")
        if (stream.isForced) flags.push("Forced")
        if (stream.isExternal) flags.push("External")
        if (stream.isInterlaced) flags.push("Interlaced")
        return flags.join(" · ")
    }

    function sourceSummary(source) {
        const parts = []
        if (source.container) parts.push(upper(source.container))
        if (source.protocol) parts.push(source.protocol)
        if (source.videoType) parts.push(source.videoType)
        const runtime = runtimeText(source.runtimeTicks || (root.item ? root.item.runtimeTicks : 0))
        if (runtime.length > 0) parts.push(runtime)
        const size = sizeText(source.size)
        if (size.length > 0) parts.push(size)
        const bitrate = bitrateText(source.bitRate)
        if (bitrate.length > 0) parts.push(bitrate)
        return parts.join(" · ")
    }

    function actionAt(index) {
        if (index === showActionIndex)
            return showLink
        if (index === seasonActionIndex)
            return seasonLink
        if (index === closeActionIndex)
            return closeBtn
        return null
    }

    function focusAction(index) {
        currentActionIndex = Math.max(0, Math.min(actionCount - 1, index))
        const target = actionAt(currentActionIndex)
        if (target && target.forceActiveFocus)
            target.forceActiveFocus()
        else
            forceActiveFocus()
    }

    function ensureFocus() {
        forceActiveFocus()
        focusAction(Math.max(0, Math.min(currentActionIndex, actionCount - 1)))
    }

    function closeOverlay() {
        root.closed()
    }

    function openSeries() {
        if (!item || !item.seriesId || !appController)
            return
        const seriesId = String(item.seriesId)
        const seriesName = String(item.seriesName || "")
        if (shell && shell.replaceRoute)
            shell.replaceRoute("libraryGrid")
        root.closed()
        appController.openSeriesById(seriesId, seriesName)
    }

    function openSeason() {
        if (!item || !item.seriesId || !item.seasonId || !appController)
            return
        const seriesId = String(item.seriesId)
        const seasonId = String(item.seasonId)
        const title = seasonTitle()
        if (shell && shell.replaceRoute)
            shell.replaceRoute("libraryGrid")
        root.closed()
        appController.openSeasonById(seriesId, seasonId, title)
    }

    function activateCurrent() {
        if (currentActionIndex === showActionIndex) {
            openSeries()
            return
        }
        if (currentActionIndex === seasonActionIndex) {
            openSeason()
            return
        }
        closeOverlay()
    }

    function handlePressed(event) {
        return visible
    }

    function handleReleased(event) {
        if (!visible)
            return false
        if (InputKeys.isBackEvent(event, true) || event.key === Qt.Key_I) {
            closeOverlay()
            return true
        }
        if (event.key === Qt.Key_Up || event.key === Qt.Key_Left) {
            focusAction(currentActionIndex - 1)
            return true
        }
        if (event.key === Qt.Key_Down || event.key === Qt.Key_Right) {
            focusAction(currentActionIndex + 1)
            return true
        }
        if (InputKeys.isAccept(event.key, false)) {
            activateCurrent()
            return true
        }
        return true
    }

    Rectangle { anchors.fill: parent; color: Theme.overlayScrimStrong }
    MouseArea { anchors.fill: parent; onClicked: root.closed() }

    Surface {
        anchors.centerIn: parent
        width: Math.min(parent.width - 120, 1120)
        height: Math.min(parent.height - 120, 800)
        baseColor: Theme.bgRaised
        elevated: true

        MouseArea { anchors.fill: parent }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 28
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

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
                        metadata: {
                            const parts = []
                            if (root.item && root.item.itemType) parts.push(root.item.itemType)
                            if (root.item && root.item.year > 0) parts.push(String(root.item.year))
                            if (root.item && root.item.displaySubtitle) parts.push(root.item.displaySubtitle)
                            const ratings = root.ratingText()
                            if (ratings.length > 0) parts.push(ratings)
                            return parts.join(" · ")
                        }
                    }
                }

                ActionButton {
                    id: closeBtn
                    text: "Close"
                    iconName: "close"
                    focus: root.currentActionIndex === root.closeActionIndex
                    onClicked: root.closeOverlay()
                    onActiveFocusChanged: if (activeFocus) root.currentActionIndex = root.closeActionIndex
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: root.showLinkVisible || root.seasonLinkVisible || root.hasRatings()
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    visible: root.showLinkVisible || root.seasonLinkVisible
                    spacing: 10

                    LinkRow {
                        id: showLink
                        Layout.fillWidth: true
                        label: "Show"
                        value: root.showLinkVisible ? root.item.seriesName : ""
                        iconName: "live_tv"
                        actionIndex: root.showActionIndex
                        onActivated: root.openSeries()
                    }

                    LinkRow {
                        id: seasonLink
                        Layout.fillWidth: true
                        label: "Season"
                        value: root.seasonLinkVisible ? root.seasonTitle() : ""
                        iconName: "video_library"
                        actionIndex: root.seasonActionIndex
                        onActivated: root.openSeason()
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    visible: root.hasRatings()
                    columns: root.width >= 1200 ? 3 : 1
                    columnSpacing: 18
                    rowSpacing: 10

                    InfoPair { Layout.fillWidth: true; label: "Parental rating"; value: root.textValue(root.item && root.item.officialRating) }
                    InfoPair { Layout.fillWidth: true; label: "Community rating"; value: root.communityRatingText() }
                    InfoPair { Layout.fillWidth: true; label: "Critic rating"; value: root.criticRatingText() }
                }
            }

            Flickable {
                id: infoFlick
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: width
                contentHeight: infoColumn.implicitHeight
                FastWheelHandler { flickable: infoFlick }

                ColumnLayout {
                    id: infoColumn
                    width: infoFlick.width
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

                                    InfoPair { Layout.fillWidth: true; label: "Container"; value: root.upper(source.container) }
                                    InfoPair { Layout.fillWidth: true; label: "Protocol"; value: source.protocol || "" }
                                    InfoPair { Layout.fillWidth: true; label: "Size"; value: root.sizeText(source.size) }
                                    InfoPair { Layout.fillWidth: true; label: "Bitrate"; value: root.bitrateText(source.bitRate) }
                                    InfoPair { Layout.fillWidth: true; label: "Runtime"; value: root.runtimeText(source.runtimeTicks || (root.item ? root.item.runtimeTicks : 0)) }
                                    InfoPair { Layout.fillWidth: true; label: "Video type"; value: source.videoType || "" }
                                }

                                InfoPair {
                                    Layout.fillWidth: true
                                    label: "Path"
                                    value: source.path || (root.item && root.item.path ? root.item.path : "")
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 10

                                    Repeater {
                                        model: source.streams || []
                                        StreamCard { stream: modelData || ({}) }
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
