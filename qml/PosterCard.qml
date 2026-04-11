import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: card
    property string title
    property string posterUrl
    property int year
    property bool selected: false

    width: 214
    height: 356

    Rectangle {
        anchors.fill: parent
        radius: 24
        color: card.selected ? "#1d6285" : "#2b111a24"
        border.color: card.selected ? "#d6f7ff" : "#23455a"
        border.width: card.selected ? 2 : 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 9

            Rectangle {
                id: posterFrame
                Layout.fillWidth: true
                Layout.preferredHeight: 292
                radius: 18
                color: "#163445"
                clip: true

                Rectangle {
                    anchors.fill: parent
                    visible: posterImage.status !== Image.Ready
                    color: "#214355"

                    Column {
                        anchors.centerIn: parent
                        spacing: 8

                        BusyIndicator {
                            anchors.horizontalCenter: parent.horizontalCenter
                            running: visible
                            visible: posterImage.status === Image.Loading
                            width: 28
                            height: 28
                        }

                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: year > 0 ? String(year) : "Poster"
                            color: "#a8cada"
                            font.pixelSize: 16
                        }
                    }
                }

                Image {
                    id: posterImage
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectCrop
                    source: card.posterUrl
                    asynchronous: true
                    cache: true
                    smooth: true
                    mipmap: true
                    sourceSize.width: Math.max(1, Math.round(posterFrame.width))
                    sourceSize.height: Math.max(1, Math.round(posterFrame.height))
                    opacity: status === Image.Ready ? 1 : 0

                    Behavior on opacity {
                        NumberAnimation {
                            duration: 140
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: card.title
                wrapMode: Text.Wrap
                maximumLineCount: 2
                font.pixelSize: 21
                font.weight: Font.DemiBold
                color: "#eef8ff"
            }

            Label {
                text: year > 0 ? String(year) : ""
                font.pixelSize: 16
                color: "#84bfd8"
            }
        }
    }
}
