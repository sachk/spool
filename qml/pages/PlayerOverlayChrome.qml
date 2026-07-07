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

            Item {
                id: timeline
                Layout.fillWidth: true
                Layout.preferredHeight: dp(82)
                readonly property double ratio: overlay.hasPlayer && overlay.player.durationSeconds > 0 ? Math.max(0, Math.min(1, overlay.positionSeconds() / overlay.player.durationSeconds)) : 0
                readonly property bool hasActiveSegment: overlay.hasPlayer && overlay.player.activeSegmentType.length > 0 && overlay.player.durationSeconds > 0
                readonly property double activeSegmentRatio: hasActiveSegment ? Math.max(0, Math.min(1, overlay.player.activeSegmentEndSeconds / overlay.player.durationSeconds)) : 0
                readonly property bool focused: overlay.isControlsActive() && overlay.row === "timeline" && !overlay.isAudioSyncOpen()

                Text {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    text: overlay.formatClock(overlay.positionSeconds())
                    color: timeline.focused ? overlay.colTextStrong : overlay.colTextDim
                    font.pixelSize: dp(23)
                    font.weight: Font.Medium
                    font.hintingPreference: Font.PreferNoHinting
                    renderType: Text.QtRendering
                }
                Text {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    text: overlay.formatClock(overlay.hasPlayer ? overlay.player.durationSeconds : 0)
                    color: overlay.colTextDim
                    font.pixelSize: dp(23)
                    font.weight: Font.Medium
                    font.hintingPreference: Font.PreferNoHinting
                    renderType: Text.QtRendering
                }
                Rectangle {
                    id: track
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: timeline.focused ? dp(18) : dp(11)
                    radius: height / 2
                    color: overlay.colTrackOuter
                    antialiasing: true
                    Behavior on height { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: Math.max(1, dp(2))
                        radius: height / 2
                        color: overlay.colTrackInner
                        antialiasing: true
                    }
                }
                Rectangle {
                    anchors.left: track.left
                    anchors.verticalCenter: track.verticalCenter
                    width: Math.max(track.height, track.width * timeline.ratio)
                    height: track.height
                    radius: height / 2
                    color: overlay.hasPlayer && overlay.player.buffering ? overlay.accentBright : overlay.accent
                    antialiasing: true
                    Behavior on width { enabled: !overlay.scrubbing && (!overlay.hasPlayer || !overlay.player.seeking); NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                }
                Repeater {
                    model: overlay.hasPlayer ? overlay.player.chapters : []
                    delegate: Rectangle {
                        required property var modelData
                        readonly property double startRatio: overlay.hasPlayer && overlay.player.durationSeconds > 0
                            ? Math.max(0, Math.min(1, (modelData.start || 0) / overlay.player.durationSeconds)) : 0
                        visible: startRatio > 0.002 && startRatio < 0.998
                        x: Math.max(0, Math.min(track.width - width, track.width * startRatio - width / 2))
                        anchors.verticalCenter: track.verticalCenter
                        width: Math.max(1, dp(2))
                        height: track.height
                        radius: width / 2
                        color: overlay.colTextStrong
                        opacity: 0.5
                        antialiasing: true
                    }
                }
                Rectangle {
                    x: Math.max(0, Math.min(track.width - width, track.width * timeline.activeSegmentRatio - width / 2))
                    anchors.verticalCenter: track.verticalCenter
                    width: Math.max(1, dp(3))
                    height: track.height + dp(12)
                    radius: width / 2
                    color: overlay.accentBright
                    antialiasing: true
                    opacity: 0.95
                    visible: timeline.hasActiveSegment
                }
                Rectangle {
                    id: thumbGlow
                    x: thumb.x - dp(5)
                    anchors.verticalCenter: track.verticalCenter
                    width: thumb.width + dp(10)
                    height: width
                    radius: width / 2
                    color: overlay.scrubbing || timeline.focused ? Qt.alpha(overlay.accent, 0.33) : overlay.colBackdropSoft
                    antialiasing: true
                    opacity: timeline.focused || overlay.scrubbing ? 1 : 0.55
                    Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                    Behavior on width { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                }
                Rectangle {
                    id: thumb
                    x: Math.max(0, Math.min(track.width - width, track.width * timeline.ratio - width / 2))
                    anchors.verticalCenter: track.verticalCenter
                    width: timeline.focused ? dp(34) : dp(20)
                    height: width
                    radius: width / 2
                    color: overlay.colTextStrong
                    antialiasing: true
                    Behavior on width { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }

                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * (timeline.focused || overlay.scrubbing ? 0.48 : 0.34)
                        height: width
                        radius: width / 2
                        color: overlay.accent
                        opacity: timeline.focused || overlay.scrubbing ? 1 : 0.8
                        antialiasing: true
                        Behavior on width { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    function scrub(mouse) {
                        overlay.row = "timeline"
                        overlay.mode = "controls"
                        overlay.scrubbing = true
                        overlay.scrubSeconds = overlay.clampSeconds((mouse.x / Math.max(1, width)) * (overlay.hasPlayer ? overlay.player.durationSeconds : 0))
                    }
                    onPressed: (mouse) => scrub(mouse)
                    onPositionChanged: (mouse) => { if (overlay.scrubbing) scrub(mouse) }
                    onReleased: (mouse) => { scrub(mouse); overlay.commitScrub() }
                    onCanceled: overlay.scrubbing = false
                }
            }

            RowLayout {
                id: actionRow
                Layout.fillWidth: true
                spacing: dp(8)

                Repeater {
                    model: overlay.actions.length
                    delegate: Rectangle {
                        required property int index
                        readonly property bool focused: overlay.isControlsActive() && overlay.row === "actions" && overlay.actionIndex === index && !overlay.isAudioSyncOpen()
                        readonly property string actionValue: overlay.actions[index].value
                        Layout.preferredWidth: overlay.actionTargetSize
                        Layout.preferredHeight: overlay.actionTargetSize
                        radius: Math.round(overlay.actionTargetSize / 2)
                        color: focused ? Qt.alpha(overlay.accent, 0.2) : "transparent"
                        border.width: focused ? 2 : 0
                        border.color: overlay.accentBright

                        MaterialIcon {
                            anchors.centerIn: parent
                            name: overlay.actionIcon(actionValue)
                            iconColor: focused ? overlay.colTextStrong : overlay.colIconDim
                            iconSize: actionValue === "debug" ? dp(30) : dp(36)
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                overlay.mode = "controls"
                                overlay.row = "actions"
                                overlay.actionIndex = index
                                overlay.activateAction()
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                RowLayout {
                    visible: overlay.desktopControlsAvailable
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: visible ? dp(300) : 0
                    spacing: dp(10)

                    MaterialIcon {
                        name: overlay.hasPlayer && overlay.player.volume === 0 ? "volume_off" : "volume_up"
                        iconColor: overlay.colIconDim
                        iconSize: dp(30)
                    }

                    Slider {
                        id: volumeSlider
                        Layout.preferredWidth: dp(210)
                        from: 0
                        to: 100
                        stepSize: 1
                        value: overlay.hasPlayer ? overlay.player.volume : 100
                        focusPolicy: Qt.NoFocus

                        background: Rectangle {
                            x: volumeSlider.leftPadding
                            y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                            width: volumeSlider.availableWidth
                            height: dp(7)
                            radius: height / 2
                            color: overlay.colTrackOuter

                            Rectangle {
                                width: volumeSlider.visualPosition * parent.width
                                height: parent.height
                                radius: parent.radius
                                color: overlay.accent
                            }
                        }

                        handle: Rectangle {
                            x: volumeSlider.leftPadding + volumeSlider.visualPosition * (volumeSlider.availableWidth - width)
                            y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                            width: dp(22)
                            height: width
                            radius: width / 2
                            color: overlay.colTextStrong
                            border.width: 2
                            border.color: overlay.accent
                        }

                        onMoved: if (overlay.hasPlayer) overlay.player.setVolume(Math.round(value))
                    }

                    Text {
                        text: overlay.hasPlayer ? Math.round(overlay.player.volume) + "%" : "100%"
                        color: overlay.colTextDim
                        font.pixelSize: dp(20)
                        font.weight: Font.DemiBold
                        font.hintingPreference: Font.PreferNoHinting
                        renderType: Text.QtRendering
                    }
                }
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
