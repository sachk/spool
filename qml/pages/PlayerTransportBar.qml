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

    component ActionTarget: Rectangle {
        required property string action
        readonly property int globalIndex: root.overlay.actions.indexOf(action)
        readonly property bool focused: root.overlay.isControlsActive() && root.overlay.focusZone === "actions"
                                        && root.overlay.actionIndex === globalIndex
        Layout.preferredWidth: root.overlay.actionTargetSize
        Layout.preferredHeight: root.overlay.actionTargetSize
        radius: width / 2
        color: focused ? Qt.alpha(Theme.accent, 0.2) : "transparent"
        border.width: focused ? Theme.focusBorderWidth : 0
        border.color: Theme.accent

        MaterialIcon {
            anchors.centerIn: parent
            name: parent.action.length > 0 ? root.overlay.actionIcon(parent.action) : ""
            iconColor: parent.focused ? Theme.textPrimary : Theme.textSecondary
            iconSize: root.overlay.dp(parent.action === "debug" ? 30 : 36)
        }

        TapHandler {
            onTapped: {
                root.overlay.controlsVisible = true
                root.overlay.focusZone = "actions"
                root.overlay.actionIndex = parent.globalIndex
                root.overlay.activateAction()
            }
        }
    }

    Repeater {
        model: root.overlay.transportActions
        delegate: ActionTarget {
            required property string modelData
            action: modelData
        }
    }

    Item {
        Layout.fillWidth: true
    }

    Repeater {
        model: root.overlay.utilityActions
        delegate: ActionTarget {
            required property string modelData
            action: modelData
        }
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
