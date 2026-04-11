import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: card
    property string title
    property string posterUrl
    property int year
    property bool selected: false

    width: 240
    height: 430

    Rectangle {
        anchors.fill: parent
        radius: 28
        color: card.selected ? "#1d6285" : "#2b111a24"
        border.color: card.selected ? "#d6f7ff" : "#23455a"
        border.width: card.selected ? 2 : 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 340
                radius: 20
                color: "#163445"
                clip: true

                Image {
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectCrop
                    source: card.posterUrl
                    asynchronous: true
                    cache: true
                }
            }

            Label {
                Layout.fillWidth: true
                text: card.title
                wrapMode: Text.Wrap
                maximumLineCount: 2
                font.pixelSize: 24
                font.weight: Font.DemiBold
                color: "#eef8ff"
            }

            Label {
                text: year > 0 ? String(year) : ""
                font.pixelSize: 18
                color: "#84bfd8"
            }
        }
    }
}
