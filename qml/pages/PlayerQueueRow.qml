import QtQuick
import QtQuick.Layouts
import "../primitives"
import "../theme"

// One queue entry. The row carries enough to tell two tracks off an album or
// two episodes of a series apart, which the old title-only menu row could not.
Item {
    id: row

    required property var overlay
    // The whole MovieItem. Art.url() reads every artwork tag off the gadget,
    // where a plain map would only carry the handful ArtworkService unpacks.
    required property var entry
    required property string entryType
    required property string title
    required property string subtitle
    required property string episodeCode
    required property bool genericEpisodeTitle
    required property bool playable
    required property real progress
    required property int position

    property bool current: false
    property bool highlighted: false
    property bool grabbed: false
    property bool reordering: false
    property bool pointerEnabled: false

    // Hover-revealed controls, so a resting queue is just the list.
    readonly property bool pointerAffordances: pointerEnabled && rowHover.hovered

    signal activated
    signal removeRequested
    signal dragStarted
    signal dragMovedTo(real sceneY)
    signal dragEnded

    readonly property bool audio: entryType === "Audio" || entryType === "AudioBook"
    readonly property real artHeight: overlay.dp(audio ? 46 : 40)
    readonly property real artWidth: audio ? artHeight : Math.round(artHeight * 16 / 9)

    implicitHeight: overlay.dp(Platform.isTV ? 78 : 62)

    function durationText() {
        const ticks = Number(entry && entry.runtimeTicks ? entry.runtimeTicks : 0)
        if (!(ticks > 0))
            return ""
        const total = Math.round(ticks / 10000000)
        const hours = Math.floor(total / 3600)
        const minutes = Math.floor((total % 3600) / 60)
        const seconds = total % 60
        const padded = seconds < 10 ? "0" + seconds : String(seconds)
        if (hours > 0)
            return hours + ":" + (minutes < 10 ? "0" + minutes : String(minutes)) + ":" + padded
        return minutes + ":" + padded
    }

    function primaryText() {
        if (entryType !== "Episode")
            return title
        // A bare "Episode 7" says nothing next to its own episode number, so
        // the code carries the row on its own in that case.
        return genericEpisodeTitle || episodeCode.length === 0 ? (episodeCode.length > 0 ? episodeCode : title) : episodeCode
                                                                 + " · " + title
    }

    function artworkKind() {
        return audio ? "square" : "landscape"
    }

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: -overlay.dp(8)
        anchors.rightMargin: -overlay.dp(8)
        radius: Theme.radiusSmall
        color: row.grabbed ? Theme.focusedFill : row.highlighted ? Theme.bgHover : "transparent"
        border.width: row.grabbed || row.highlighted ? Theme.focusBorderWidth : 0
        border.color: row.grabbed ? Theme.accent : Qt.alpha(Theme.accent, 0.55)
    }

    RowLayout {
        anchors.fill: parent
        spacing: overlay.dp(12)

        // Position, or the state that replaces it: playing, or picked up.
        Item {
            Layout.preferredWidth: overlay.dp(24)
            Layout.fillHeight: true

            SecondaryText {
                anchors.centerIn: parent
                visible: !row.current && !row.grabbed
                text: String(row.position)
                color: Theme.textMuted
                font.pixelSize: Metrics.metaSizePx
            }

            MaterialIcon {
                anchors.centerIn: parent
                visible: row.current && !row.grabbed
                name: "graphic_eq"
                iconSize: overlay.dp(18)
                iconColor: Theme.accent
            }

            MaterialIcon {
                anchors.centerIn: parent
                visible: row.grabbed
                name: "drag_indicator"
                iconSize: overlay.dp(18)
                iconColor: Theme.accent
            }
        }

        ImageCard {
            Layout.preferredWidth: row.artWidth
            Layout.preferredHeight: row.artHeight
            imageUrl: Art.url(row.entry, row.artworkKind(), Math.round(row.artWidth * 2))
            fallbackIcon: row.audio ? "music_note" : row.entryType === "Episode" ? "tv" : "movie"
            fallbackTint: Theme.bgHover

            // Resume position, on the artwork where the eye already is.
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: overlay.dp(3)
                visible: row.progress > 0
                color: Qt.alpha("#000000", 0.55)

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: Math.round(parent.width * Math.min(1, Math.max(0, row.progress)))
                    color: Theme.accent
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            AppText {
                Layout.fillWidth: true
                text: row.primaryText()
                color: row.playable ? (row.current ? Theme.accent : Theme.textPrimary) : Theme.textDisabled
                font.pixelSize: Metrics.bodySizePx
                font.weight: row.current ? Font.DemiBold : Font.Normal
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            SecondaryText {
                Layout.fillWidth: true
                visible: row.subtitle.length > 0
                text: row.subtitle
                color: Theme.textSecondary
                font.pixelSize: Metrics.metaSizePx
                maximumLineCount: 1
                elide: Text.ElideRight
            }
        }

        SecondaryText {
            visible: !removeButton.visible && text.length > 0
            text: row.durationText()
            color: Theme.textMuted
            font.pixelSize: Metrics.metaSizePx
        }

        // Pointer-only. A remote removes through the row's own menu, and an
        // extra focusable button would sit between every row and the next.
        IconButton {
            id: removeButton
            visible: row.pointerAffordances && !row.reordering
            focusPolicy: Qt.NoFocus
            chromeless: true
            iconName: "delete"
            accessibleName: "Remove from queue"
            onClicked: row.removeRequested()
        }

        // The drag grip, not the whole row: dragging anywhere would fight the
        // list's own flicking and swallow taps meant to play a track.
        Item {
            id: grip
            Layout.preferredWidth: row.pointerAffordances ? overlay.dp(26) : 0
            Layout.fillHeight: true
            visible: row.pointerAffordances

            MaterialIcon {
                anchors.centerIn: parent
                name: "drag_indicator"
                iconSize: overlay.dp(18)
                iconColor: gripHover.hovered ? Theme.textPrimary : Theme.textMuted
            }

            HoverHandler {
                id: gripHover
            }

            // Dragging is confined to the grip. Attached to the whole row it
            // would fight the list's own flicking and swallow taps meant to
            // play a track.
            DragHandler {
                target: null
                xAxis.enabled: false
                yAxis.enabled: true
                onActiveChanged: active ? row.dragStarted() : row.dragEnded()
                onCentroidChanged: if (active)
                                       row.dragMovedTo(centroid.scenePosition.y)
            }
        }
    }

    HoverHandler {
        id: rowHover
        enabled: row.playable
    }

    TapHandler {
        enabled: row.playable && !row.reordering
        onTapped: row.activated()
    }
}
