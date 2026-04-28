import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

FocusScope {
    id: librariesPage
    focus: true

    Keys.onReleased: (event) => {
        if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape || event.key === Qt.Key_BrowserBack) {
            appController.back()
            event.accepted = true
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 76
        spacing: 22

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: "Libraries"
                font.pixelSize: 62
                font.weight: Font.DemiBold
                color: "#f0fbff"
                Layout.fillWidth: true
            }

            GlowButton {
                text: "Settings"
                implicitWidth: 170
                onClicked: appController.openSettings()
            }
        }

        Label {
            text: "Choose a Jellyfin library"
            font.pixelSize: 25
            color: "#aac7d4"
        }

        ListView {
            id: libraryList
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16
            clip: true
            model: appController.libraries
            keyNavigationWraps: true
            focus: true

            onCountChanged: {
                if (count > 0 && currentIndex < 0)
                    currentIndex = 0
                else if (count === 0)
                    currentIndex = -1
            }

            delegate: GlassPanel {
                required property int index
                required property string name
                required property string collectionType

                width: libraryList.width
                height: 104
                radius: 30
                panelColor: ListView.isCurrentItem ? "#4e526d80" : "#50131d28"
                edgeColor: ListView.isCurrentItem ? "#d7f8ff" : "#344f62"

                Rectangle {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 18
                    width: 8
                    height: parent.height - 40
                    radius: 4
                    color: ListView.isCurrentItem ? "#74ffd7" : "#2d607d"
                    antialiasing: true
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    anchors.leftMargin: 44
                    spacing: 18

                    Label {
                        text: name
                        font.pixelSize: 35
                        color: "#eef9ff"
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        implicitWidth: typeLabel.implicitWidth + 28
                        implicitHeight: 38
                        radius: 19
                        color: "#263a49"
                        border.width: 1
                        border.color: "#405d70"
                        antialiasing: true

                        Label {
                            id: typeLabel
                            anchors.centerIn: parent
                            text: collectionType.length > 0 ? collectionType : "library"
                            font.pixelSize: 19
                            color: "#9edff0"
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        libraryList.currentIndex = index
                        appController.openLibrary(index)
                    }
                }
            }

            Keys.onReleased: (event) => {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    appController.openLibrary(currentIndex)
                    event.accepted = true
                }
            }
        }
    }
}
