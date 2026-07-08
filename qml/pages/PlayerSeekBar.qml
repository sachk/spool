import QtQuick
import QtQuick.Layouts
import "../theme"

Item {
    id: timeline
    required property var overlay

    function dp(n) {
        return overlay ? overlay.dp(n) : Math.round(n)
    }
    Layout.fillWidth: true
    Layout.preferredHeight: dp(82)
    readonly property double ratio: overlay.hasPlayer && overlay.player.durationSeconds > 0 ? Math.max(0, Math.min(1,
                                                                                                                   overlay.positionSeconds(
                                                                                                                       ) / overlay.player.durationSeconds)) :
                                                                                              0
    readonly property bool hasActiveSegment: overlay.hasPlayer && overlay.player.activeSegmentType.length > 0
                                             && overlay.player.durationSeconds > 0
    readonly property double activeSegmentRatio: hasActiveSegment ? Math.max(0, Math.min(1,
                                                                                         overlay.player.activeSegmentEndSeconds
                                                                                         / overlay.player.durationSeconds)) :
                                                                    0
    readonly property bool focused: overlay.isControlsActive() && overlay.row === "timeline" && !overlay.isAudioSyncOpen(
                                        )

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
        Behavior on height {
            NumberAnimation {
                duration: 140
                easing.type: Easing.OutCubic
            }
        }

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
        Behavior on width {
            enabled: !overlay.scrubbing && (!overlay.hasPlayer || !overlay.player.seeking)
            NumberAnimation {
                duration: 180
                easing.type: Easing.OutCubic
            }
        }
    }
    Repeater {
        model: overlay.hasPlayer ? overlay.player.chapters : []
        delegate: Rectangle {
            required property var modelData
            readonly property double startRatio: overlay.hasPlayer && overlay.player.durationSeconds > 0 ? Math.max(0, Math.min(
                                                                                                                        1, (modelData.start
                                                                                                                            || 0) / overlay.player.durationSeconds)) :
                                                                                                           0
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
        Behavior on opacity {
            NumberAnimation {
                duration: 140
                easing.type: Easing.OutCubic
            }
        }
        Behavior on width {
            NumberAnimation {
                duration: 140
                easing.type: Easing.OutCubic
            }
        }
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
        Behavior on width {
            NumberAnimation {
                duration: 140
                easing.type: Easing.OutCubic
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * (timeline.focused || overlay.scrubbing ? 0.48 : 0.34)
            height: width
            radius: width / 2
            color: overlay.accent
            opacity: timeline.focused || overlay.scrubbing ? 1 : 0.8
            antialiasing: true
            Behavior on width {
                NumberAnimation {
                    duration: 140
                    easing.type: Easing.OutCubic
                }
            }
        }
    }
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        function scrub(mouse) {
            overlay.row = "timeline"
            overlay.mode = "controls"
            overlay.scrubbing = true
            overlay.scrubSeconds = overlay.clampSeconds((mouse.x / Math.max(1, width)) * (overlay.hasPlayer
                                                                                          ? overlay.player.durationSeconds :
                                                                                            0))
        }
        onPressed: mouse => scrub(mouse)
        onPositionChanged: mouse => {
                               if (overlay.scrubbing)
                               scrub(mouse)
                           }
        onReleased: mouse => {
                        scrub(mouse)
                        overlay.commitScrub()
                    }
        onCanceled: overlay.scrubbing = false
    }
}
