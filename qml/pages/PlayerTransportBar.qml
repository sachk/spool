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
        readonly property string tooltip: root.overlay.actionTooltip(action)
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
            iconSize: root.overlay.dp(parent.action === "debug" ? 32 : 38)
        }

        HoverHandler {
            id: hover
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.top
            anchors.bottomMargin: root.overlay.dp(8)
            width: tooltipText.implicitWidth + root.overlay.dp(20)
            height: root.overlay.dp(34)
            radius: root.overlay.dp(6)
            color: "#E6222222"
            border.width: 1
            border.color: Theme.borderStrong
            visible: parent.tooltip.length > 0 && (parent.focused || hover.hovered)
            z: 10

            AppText {
                id: tooltipText
                anchors.centerIn: parent
                text: parent.parent.tooltip
                color: Theme.textPrimary
                font.pixelSize: root.overlay.dp(15)
                font.weight: Font.Medium
            }
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
        id: volumeControls

        readonly property bool showSlider: Settings.values["playback/showVolumeSlider"] !== false
        property real lastAudibleVolume: 100

        visible: root.overlay.desktopControlsAvailable
        Layout.minimumWidth: visible ? root.overlay.dp(showSlider ? 254 : 28) : 0
        Layout.preferredWidth: visible ? root.overlay.dp(showSlider ? 254 : 28) : 0
        Layout.maximumWidth: visible ? root.overlay.dp(showSlider ? 254 : 28) : 0
        spacing: root.overlay.dp(10)

        MaterialIcon {
            name: root.overlay.hasPlayer && root.overlay.player.volume === 0 ? "volume_off" : "volume_up"
            iconColor: Theme.textSecondary
            iconSize: root.overlay.dp(28)

            TapHandler {
                onTapped: {
                    if (!root.overlay.hasPlayer)
                    return
                    const volume = Number(root.overlay.player.volume)
                    if (volume > 0) {
                        volumeControls.lastAudibleVolume = volume
                        root.overlay.player.setVolume(0)
                    } else {
                        root.overlay.player.setVolume(Math.max(1, volumeControls.lastAudibleVolume))
                    }
                }
            }
        }

        Slider {
            visible: parent.showSlider
            Layout.minimumWidth: visible ? root.overlay.dp(144) : 0
            Layout.preferredWidth: visible ? root.overlay.dp(144) : 0
            Layout.maximumWidth: visible ? root.overlay.dp(144) : 0
            from: 0
            to: 100
            stepSize: 1
            value: root.overlay.hasPlayer ? root.overlay.player.volume : 100
            focusPolicy: Qt.NoFocus
            onMoved: if (root.overlay.hasPlayer)
            root.overlay.player.setVolume(Math.round(value))

            background: Rectangle {
                x: parent.leftPadding
                y: parent.topPadding + parent.availableHeight / 2 - height / 2
                width: parent.availableWidth
                height: root.overlay.dp(6)
                radius: height / 2
                color: Theme.borderStrong

                Rectangle {
                    width: parent.parent.visualPosition * parent.width
                    height: parent.height
                    radius: parent.radius
                    color: Theme.textSecondary
                }
            }

            handle: Rectangle {
                x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                y: parent.topPadding + parent.availableHeight / 2 - height / 2
                width: root.overlay.dp(16)
                height: width
                radius: width / 2
                color: Theme.textPrimary
                border.width: root.overlay.dp(1)
                border.color: Theme.bg
            }
        }

        AppText {
            visible: parent.showSlider
            text: root.overlay.hasPlayer ? Math.round(root.overlay.player.volume) + "%" : "100%"
            color: Theme.textSecondary
            font.pixelSize: root.overlay.dp(18)
            font.weight: Font.DemiBold
        }
    }
}
