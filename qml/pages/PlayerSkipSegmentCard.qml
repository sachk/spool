import QtQuick
import QtQuick.Layouts
import "../primitives"

Rectangle {
    id: skipSegmentCard

    required property var overlay
    readonly property real uiScale: overlay ? overlay.uiScale : 1
    readonly property string segmentType: overlay && overlay.hasPlayer ? overlay.player.activeSegmentType : ""
    readonly property string label: segmentType === "Intro" ? "Skip Intro"
                                  : segmentType === "Outro" ? "Skip Outro"
                                  : segmentType === "Recap" ? "Skip Recap"
                                  : segmentType === "Preview" ? "Skip Preview"
                                  : segmentType.length > 0 ? "Skip " + segmentType
                                  : ""

    function dp(n) {
        return Math.round(n * uiScale)
    }

    anchors.right: parent.right
    anchors.bottom: parent.bottom
    anchors.rightMargin: dp(48)
    anchors.bottomMargin: dp(220)
    width: dp(220)
    height: dp(60)
    radius: dp(12)
    color: Qt.alpha(overlay.accent, 0.9)
    border.width: 1
    border.color: overlay.accentBright
    visible: segmentType.length > 0 && overlay.mode !== "hidden"
    opacity: visible ? 1 : 0
    z: 30

    Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: dp(16)
        anchors.rightMargin: dp(16)
        spacing: dp(10)

        MaterialIcon {
            name: "skip_next"
            iconColor: overlay.colTextStrong
            iconSize: dp(24)
        }

        Text {
            Layout.fillWidth: true
            text: skipSegmentCard.label
            color: overlay.colTextStrong
            font.pixelSize: dp(18)
            font.weight: Font.DemiBold
            font.hintingPreference: Font.PreferNoHinting
            renderType: Text.QtRendering
        }

        Text {
            text: "T"
            color: overlay.colTextStrong
            font.pixelSize: dp(13)
            font.weight: Font.Medium
            opacity: 0.7
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: if (overlay.hasPlayer) overlay.player.skipActiveSegment()
    }
}
