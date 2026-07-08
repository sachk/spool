import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../primitives"

RowLayout {
    id: actionRow
    required property var overlay

    function dp(n) {
        return overlay ? overlay.dp(n) : Math.round(n)
    }
    Layout.fillWidth: true
    spacing: dp(8)

    Repeater {
        model: overlay.actions.length
        delegate: Rectangle {
            required property int index
            readonly property bool focused: overlay.isControlsActive() && overlay.row === "actions"
                                            && overlay.actionIndex === index && !overlay.isAudioSyncOpen()
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

    Item {
        Layout.fillWidth: true
    }

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

            onMoved: if (overlay.hasPlayer)
                         overlay.player.setVolume(Math.round(value))
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
