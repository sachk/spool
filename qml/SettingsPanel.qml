import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

FocusScope {
    id: root
    focus: true

    Keys.onReleased: (event) => {
        if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape || event.key === Qt.Key_BrowserBack) {
            appController.closeSettings()
            event.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#b0040910"

        MouseArea {
            anchors.fill: parent
            onClicked: appController.closeSettings()
        }
    }

    Rectangle {
        width: 560
        height: 310
        anchors.centerIn: parent
        radius: 18
        color: "#111d27"
        border.width: 1
        border.color: "#3b7188"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 30
            spacing: 24

            RowLayout {
                Layout.fillWidth: true

                Label {
                    text: "Settings"
                    font.pixelSize: 38
                    font.weight: Font.DemiBold
                    color: "#effaff"
                    Layout.fillWidth: true
                }

                Button {
                    text: "Close"
                    onClicked: appController.closeSettings()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: "#2d5a70"
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 18

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Label {
                        text: "Night mode"
                        font.pixelSize: 28
                        color: "#f0fbff"
                    }

                    Label {
                        text: "Dialogue lift and late-night dynamic range"
                        font.pixelSize: 18
                        color: "#91c6d9"
                    }
                }

                Switch {
                    checked: appController.nightModeEnabled
                    focus: true
                    onToggled: appController.setNightModeEnabled(checked)
                    Keys.onReleased: (event) => {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            appController.toggleNightMode()
                            event.accepted = true
                        }
                    }
                }
            }
        }
    }
}
