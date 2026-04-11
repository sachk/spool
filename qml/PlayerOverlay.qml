import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

FocusScope {
    id: overlay
    focus: visible

    Timer {
        id: autohideTimer
        interval: 3000
        running: overlay.visible && !autohideTimer.paused
        onTriggered: controlsItem.opacity = 0
        property bool paused: false
    }

    function resetAutohide() {
        controlsItem.opacity = 1
        autohideTimer.restart()
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onPositionChanged: overlay.resetAutohide()
        onClicked: overlay.resetAutohide()
    }

    Keys.onPressed: (event) => {
        overlay.resetAutohide()
    }

    Keys.onReleased: (event) => {
        overlay.resetAutohide()
        if (event.key === Qt.Key_Left) {
            appController.player.seekBack()
            event.accepted = true
        } else if (event.key === Qt.Key_Right) {
            appController.player.seekForward()
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            appController.player.togglePause()
            event.accepted = true
        } else if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape) {
            appController.player.stop()
            event.accepted = true
        }
    }

    Item {
        id: controlsItem
        anchors.fill: parent
        Behavior on opacity { NumberAnimation { duration: 250 } }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 36
            height: 220
            radius: 34
            color: "#fa08131c"
            border.color: "#366987"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: appController.player.title
                            font.pixelSize: 32
                            font.weight: Font.DemiBold
                            color: "#f1fbff"
                        }

                        Label {
                            text: appController.player.statusText
                            font.pixelSize: 20
                            color: "#8fe0f5"
                        }
                    }

                    Button {
                        text: "≡"
                        width: 60
                        height: 60
                        onClicked: appController.player.toggleDebugOsd()
                        ToolTip.visible: hovered
                        ToolTip.text: "Toggle Debug OSD"
                    }
                }

                Slider {
                    from: 0
                    to: Math.max(1, appController.player.durationSeconds)
                    value: appController.player.positionSeconds
                    enabled: false
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 20

                    Label {
                        text: Math.floor(appController.player.positionSeconds) + "s / " + Math.floor(appController.player.durationSeconds) + "s"
                        font.pixelSize: 22
                        color: "#d7f8ff"
                    }

                    Label {
                        text: appController.player.paused ? "Paused" : "Playing"
                        font.pixelSize: 22
                        color: "#d7f8ff"
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        text: "Left/Right seek  Enter pause  Back stop  ≡ debug"
                        font.pixelSize: 20
                        color: "#84bfd8"
                    }
                }

                Label {
                    visible: appController.player.errorText.length > 0
                    text: appController.player.errorText
                    font.pixelSize: 20
                    color: "#ffb8bd"
                }
            }
        }
    }
}

