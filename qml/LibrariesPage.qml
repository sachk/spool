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
        anchors.margins: 80
        spacing: 24

        Label {
            text: "Libraries"
            font.pixelSize: 54
            font.weight: Font.DemiBold
            color: "#f0fbff"
        }

        Label {
            text: "Choose a Jellyfin library"
            font.pixelSize: 24
            color: "#8ec7de"
        }

        ListView {
            id: libraryList
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14
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

            delegate: Rectangle {
                required property int index
                required property string name
                required property string collectionType

                width: libraryList.width
                height: 96
                radius: 26
                color: ListView.isCurrentItem ? "#22668b" : "#44131d28"
                border.color: ListView.isCurrentItem ? "#d7f8ff" : "#274d62"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 18

                    Label {
                        text: name
                        font.pixelSize: 34
                        color: "#eef9ff"
                        Layout.fillWidth: true
                    }

                    Label {
                        text: collectionType.length > 0 ? collectionType : "library"
                        font.pixelSize: 20
                        color: "#8ec7de"
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
