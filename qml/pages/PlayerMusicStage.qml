pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import JellyfinWebOS
import "../theme"
import "../primitives"

// Audio has no picture of its own, so the screen it would have filled carries
// the record instead: the cover large and left of centre, with what is playing
// set beside it. Sized off the overlay so it clears the transport chrome.
Item {
    id: root

    required property var overlay

    readonly property var item: overlay.currentQueueItem || ({})
    readonly property string trackTitle: {
        const queued = String(root.item.title || "").trim()
        if (queued.length > 0)
        return queued
        return overlay.hasPlayer ? overlay.player.title : ""
    }
    readonly property string artistText: String(root.item.albumArtist || "").trim()
    readonly property string albumText: String(root.item.album || "").trim()
    readonly property string yearText: Number(root.item.year || 0) > 0 ? String(root.item.year) : ""
    readonly property string queuePositionText: {
        const queue = overlay.playQueue
        if (!queue || queue.count <= 1)
        return ""
        return "Track " + (queue.currentIndex + 1) + " of " + queue.count
    }
    // The chrome owns the lower band of the screen; centre the record in what
    // is left rather than in the window, so it never sits behind the controls.
    readonly property real stageHeight: Math.max(root.dp(240), root.height - root.dp(400))
    readonly property real coverSize: Math.max(root.dp(180), Math.min(stageHeight * 0.86, root.dp(460)))

    function dp(value) {
        return overlay ? overlay.dp(value) : Math.round(value)
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
    }

    // The cover again, filling the screen behind everything, dimmed far enough
    // to read as a tint rather than as a second copy of the artwork.
    Image {
        anchors.fill: parent
        source: cover.imageUrl.length > 0 ? cover.artworkSource(cover.imageUrl) : ""
        visible: status === Image.Ready
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: false
        opacity: 0.18
        sourceSize.width: Math.max(1, Math.round(width / 3))
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop {
                position: 0
                color: Qt.rgba(0, 0, 0, 0.45)
            }
            GradientStop {
                position: 1
                color: Qt.rgba(0, 0, 0, 0.85)
            }
        }
    }

    RowLayout {
        anchors.left: parent.left
        anchors.leftMargin: root.dp(96)
        anchors.right: parent.right
        anchors.rightMargin: root.dp(96)
        anchors.top: parent.top
        anchors.topMargin: Math.round((root.stageHeight - root.coverSize) / 2)
        height: root.coverSize
        spacing: root.dp(48)

        ImageCard {
            id: cover
            Layout.preferredWidth: root.coverSize
            Layout.preferredHeight: root.coverSize
            imageUrl: Art.url(root.item, "poster", Math.ceil(root.coverSize))
            fallbackIcon: "music_note"
            fallbackTint: Theme.libraryTint(root.albumText.length > 0 ? root.albumText : root.trackTitle)
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: root.dp(10)

            AppText {
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? implicitHeight : 0
                visible: root.queuePositionText.length > 0
                text: root.queuePositionText
                color: Theme.accent
                font.pixelSize: root.dp(20)
                font.weight: Font.DemiBold
            }

            AppText {
                Layout.fillWidth: true
                text: root.trackTitle
                color: Theme.textPrimary
                font.pixelSize: root.dp(52)
                font.weight: Font.Bold
                maximumLineCount: 2
                wrapMode: Text.Wrap
                elide: Text.ElideRight
            }

            AppText {
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? implicitHeight : 0
                visible: root.artistText.length > 0
                text: root.artistText
                color: Theme.textSecondary
                font.pixelSize: root.dp(30)
                font.weight: Font.DemiBold
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            AppText {
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? implicitHeight : 0
                visible: root.albumText.length > 0
                text: root.yearText.length > 0 ? root.albumText + "  ·  " + root.yearText : root.albumText
                color: Theme.textMuted
                font.pixelSize: root.dp(24)
                maximumLineCount: 1
                elide: Text.ElideRight
            }
        }
    }
}
