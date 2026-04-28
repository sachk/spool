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
        color: "#c0040910"

        MouseArea {
            anchors.fill: parent
            onClicked: appController.closeSettings()
        }
    }

    GlassPanel {
        width: 660
        height: 360
        anchors.centerIn: parent
        radius: 34
        panelColor: "#e00e1a24"
        edgeColor: "#496b7e"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 34
            spacing: 24

            RowLayout {
                Layout.fillWidth: true

                Label {
                    text: "Settings"
                    font.pixelSize: 44
                    font.weight: Font.DemiBold
                    color: "#effaff"
                    Layout.fillWidth: true
                }

                GlowButton {
                    text: "Close"
                    implicitWidth: 135
                    onClicked: appController.closeSettings()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: "#38586a"
            }

            GlassPanel {
                Layout.fillWidth: true
                Layout.preferredHeight: 120
                radius: 26
                panelColor: "#52131d28"
                edgeColor: "#36576b"

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 18

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Label {
                            text: "Night mode"
                            font.pixelSize: 30
                            font.weight: Font.DemiBold
                            color: "#f0fbff"
                        }

                        Label {
                            text: "Dialogue lift and late-night dynamic range"
                            font.pixelSize: 19
                            color: "#aac7d4"
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
}
