pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../theme"
import "../primitives"

RowLayout {
    id: root

    required property var overlay
    Layout.fillWidth: true
    spacing: overlay.dp(8)

    Repeater {
        model: root.overlay.actions
        delegate: Rectangle {
            required property int index
            required property string modelData
            readonly property bool focused: root.overlay.isControlsActive() && root.overlay.focusZone === "actions"
                                            && root.overlay.actionIndex === index
            Layout.preferredWidth: root.overlay.actionTargetSize
            Layout.preferredHeight: root.overlay.actionTargetSize
            radius: width / 2
            color: focused ? Qt.alpha(Theme.accent, 0.2) : "transparent"
            border.width: focused ? Theme.focusBorderWidth : 0
            border.color: Theme.accent

            MaterialIcon {
                anchors.centerIn: parent
                name: root.overlay.actionIcon(modelData)
                iconColor: focused ? Theme.textPrimary : Theme.textSecondary
                iconSize: root.overlay.dp(modelData === "debug" ? 30 : 36)
            }

            TapHandler {
                onTapped: {
                    root.overlay.controlsVisible = true
                    root.overlay.focusZone = "actions"
                    root.overlay.actionIndex = index
                    root.overlay.activateAction()
                }
            }
        }
    }

    Item {
        Layout.fillWidth: true
    }

    RowLayout {
        visible: root.overlay.desktopControlsAvailable
        Layout.preferredWidth: visible ? Math.min(root.width * 0.15, root.overlay.dp(300)) : 0
        spacing: root.overlay.dp(10)

        MaterialIcon {
            name: root.overlay.hasPlayer && root.overlay.player.volume === 0 ? "volume_off" : "volume_up"
            iconColor: Theme.textSecondary
            iconSize: root.overlay.dp(28)
        }

        Slider {
            Layout.fillWidth: true
            from: 0
            to: 100
            stepSize: 1
            value: root.overlay.hasPlayer ? root.overlay.player.volume : 100
            focusPolicy: Qt.NoFocus
            onMoved: if (root.overlay.hasPlayer)
            root.overlay.player.setVolume(Math.round(value))
        }

        AppText {
            text: root.overlay.hasPlayer ? Math.round(root.overlay.player.volume) + "%" : "100%"
            color: Theme.textSecondary
            font.pixelSize: root.overlay.dp(18)
            font.weight: Font.DemiBold
        }
    }
}
