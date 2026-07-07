import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import JellyfinWebOS
import "../theme"
import "../primitives"

Item {
    id: root

    property var overlay
    readonly property real hudX: hud.x
    readonly property real hudY: hud.y
    readonly property real actionRowX: actionRow.x
    readonly property real actionRowSpacing: actionRow.spacing
    readonly property real actionRowY: actionRow.y
    readonly property real actionRowWidth: actionRow.width

    function dp(n) {
        return overlay ? overlay.dp(n) : Math.round(n)
    }

    function positionMenuAtTop() {
        menuPanel.positionAtTop()
    }

    Item {
        id: trickplayPreloadPool
        visible: false
        Repeater {
            model: overlay.visible && overlay.hasPlayer ? overlay.player.trickplaySheetUrls : []
            delegate: Image {
                required property string modelData
                source: modelData
                asynchronous: true
                cache: true
            }
        }
    }

    // Embedded video surface (desktop / non-Starfish builds). On Starfish the
    // video lives on a separate exported surface so this item is harmless —
    // MpvVideoItem just sits unused. z=-1 keeps it behind the HUD.
    MpvVideoItem {
        anchors.fill: parent
        z: -1
    }

    TapHandler { onTapped: overlay.showControls("timeline") }
    HoverHandler { onHoveredChanged: if (hovered && overlay.mode !== "hidden") overlay.showControls(overlay.row) }
    WheelHandler {
        target: null
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: (event) => overlay.adjustVolumeFromWheel(event)
    }

    states: [
        State { name: "hidden"; when: overlay.mode === "hidden"; PropertyChanges { target: hud; opacity: 0 } },
        State { name: "controls"; when: overlay.mode === "controls"; PropertyChanges { target: hud; opacity: 1 } PropertyChanges { target: backButton; opacity: 1 } PropertyChanges { target: topScrim; opacity: 1 } PropertyChanges { target: bottomScrim; opacity: 1 } },
        State {
            name: "subtitles"
            when: overlay.mode === "subtitles"
            PropertyChanges { target: hud; opacity: 1 }
            PropertyChanges { target: backButton; opacity: 1 }
            PropertyChanges { target: topScrim; opacity: 1 }
            PropertyChanges { target: bottomScrim; opacity: 1 }
            PropertyChanges { target: menuPanel; opacity: 1 }
        },
        State {
            name: "audio"
            when: overlay.mode === "audio"
            PropertyChanges { target: hud; opacity: 1 }
            PropertyChanges { target: backButton; opacity: 1 }
            PropertyChanges { target: topScrim; opacity: 1 }
            PropertyChanges { target: bottomScrim; opacity: 1 }
            PropertyChanges { target: menuPanel; opacity: 1 }
        },
        State {
            name: "debug"
            when: overlay.mode === "debug"
            PropertyChanges { target: hud; opacity: 1 }
            PropertyChanges { target: backButton; opacity: 1 }
            PropertyChanges { target: topScrim; opacity: 1 }
            PropertyChanges { target: bottomScrim; opacity: 1 }
            PropertyChanges { target: menuPanel; opacity: 1 }
        },
        State {
            name: "audiosync"
            when: overlay.isAudioSyncOpen()
            PropertyChanges { target: hud; opacity: 1 }
            PropertyChanges { target: backButton; opacity: 1 }
            PropertyChanges { target: topScrim; opacity: 0 }
            PropertyChanges { target: bottomScrim; opacity: 0.35 }
            PropertyChanges { target: audioSyncPanel; opacity: 1 }
        }
    ]

    Rectangle {
        id: topScrim
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: dp(150)
        opacity: 0
        visible: opacity > 0.01
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#99000000" }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    Rectangle {
        id: bottomScrim
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: dp(360)
        opacity: 0
        visible: opacity > 0.01
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 1.0; color: "#CC000000" }
        }
    }

    Rectangle {
        id: backButton
        readonly property bool focused: overlay.isControlsActive() && overlay.row === "back" && !overlay.isAudioSyncOpen()
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: dp(40)
        width: dp(64)
        height: width
        radius: width / 2
        color: focused ? Qt.alpha(overlay.accent, 0.2) : "transparent"
        border.width: focused ? 2 : 0
        border.color: overlay.accentBright
        opacity: 0
        visible: opacity > 0.01

        MaterialIcon {
            anchors.centerIn: parent
            name: "arrow_back"
            iconColor: backButton.focused ? overlay.colTextStrong : overlay.colIconDim
            iconSize: dp(34)
        }

        MouseArea {
            anchors.fill: parent
            // Explicit exit gesture — skip handleBack's progressive layering.
            onClicked: overlay.stopPlayback("overlay-back")
        }
    }

    transitions: [
        Transition {
            to: "debug"
            NumberAnimation { properties: "opacity"; duration: 0 }
        },
        Transition {
            to: "audiosync"
            NumberAnimation { properties: "opacity"; duration: 0 }
        },
        Transition {
            NumberAnimation { properties: "opacity"; duration: 140; easing.type: Easing.OutCubic }
        }
    ]

    PlayerTrickplayPreview {
        id: trickplayPreview
        overlay: root.overlay
    }

    PlayerSkipSegmentCard {
        id: skipSegmentCard
        overlay: root.overlay
    }

    Item {
        id: hud
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: dp(52)
        height: dp(236)
        visible: opacity > 0.01
        opacity: 0

        ColumnLayout {
            anchors.fill: parent
            spacing: dp(16)

            RowLayout {
                Layout.fillWidth: true
                    spacing: dp(20)

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: dp(6)
                    Text {
                        Layout.fillWidth: true
                        text: overlay.hasPlayer ? overlay.player.title : ""
                        color: overlay.colText
                        font.pixelSize: dp(40)
                        font.weight: Font.Bold
                        font.hintingPreference: Font.PreferNoHinting
                        renderType: Text.QtRendering
                        maximumLineCount: 1
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: overlay.hasPlayer ? overlay.player.statusText : ""
                        color: overlay.hasPlayer && (overlay.player.buffering || overlay.player.seeking) ? overlay.accentBright : overlay.colStatus
                        font.pixelSize: dp(24)
                        font.weight: Font.Medium
                        font.hintingPreference: Font.PreferNoHinting
                        renderType: Text.QtRendering
                        maximumLineCount: 1
                        elide: Text.ElideRight
                    }
                }

                Text {
                    text: overlay.hasPlayer && overlay.player.paused ? "Paused" : "Playing"
                    color: overlay.hasPlayer && overlay.player.paused ? overlay.colTextStrong : overlay.colTextDim
                    font.pixelSize: dp(23)
                    font.weight: Font.DemiBold
                    font.hintingPreference: Font.PreferNoHinting
                    renderType: Text.QtRendering
                }
            }

            PlayerSeekBar {
                id: timeline
                overlay: root.overlay
            }

            PlayerTransportBar {
                id: actionRow
                overlay: root.overlay
            }

            Text {
                Layout.fillWidth: true
                visible: overlay.hasPlayer && overlay.player.errorText.length > 0
                text: overlay.hasPlayer ? overlay.player.errorText : ""
                color: overlay.colError
                font.pixelSize: dp(18)
                font.hintingPreference: Font.PreferNoHinting
                renderType: Text.QtRendering
                wrapMode: Text.Wrap
            }
        }
    }

    PlayerAudioSyncPanel {
        id: audioSyncPanel
        overlay: root.overlay
    }

    PlayerOverlayMenu {
        id: menuPanel
        overlay: root.overlay
    }
}
