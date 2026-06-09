import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    property var item: ({})
    readonly property var sources: item && item.mediaSources ? item.mediaSources : []
    signal closed()

    focus: visible
    onActiveFocusChanged: if (visible && !activeFocus) forceActiveFocus()
    onVisibleChanged: if (visible) forceActiveFocus()

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

    Keys.onReleased: (event) => {
        if (InputKeys.isBack(event.key) || event.key === Qt.Key_I) {
            root.closed()
            event.accepted = true
        }
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

    Rectangle { anchors.fill: parent; color: "#CC000000" }
    MouseArea { anchors.fill: parent; onClicked: root.closed() }

    Surface {
        anchors.centerIn: parent
        width: Math.min(parent.width - 120, 1120)
        height: Math.min(parent.height - 120, 760)
        baseColor: Theme.bgRaised
        elevated: true

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
                        text: (root.item && (root.item.displayTitle || root.item.title)) ? (root.item.displayTitle || root.item.title) : "Media info"
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
                            return parts.join(" · ")
                        }
                    }
                }

                ActionButton {
                    id: closeBtn
                    text: "Close"
                    focus: true
                    onClicked: root.closed()
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
