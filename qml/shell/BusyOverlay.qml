import QtQuick
import QtQuick.Controls.Basic
import "../theme"
import "../primitives"

Item {
    id: root

    property string text: ""

    Rectangle {
        anchors.fill: parent
        color: Theme.busyScrim

        Surface {
            anchors.centerIn: parent
            width: Math.min(620, parent.width - 96)
            height: 104
            elevated: true

            Row {
                anchors.centerIn: parent
                spacing: 18

                BusyIndicator {
                    running: true
                    width: 30
                    height: 30
                }

                AppText {
                    text: root.text
                    font.pixelSize: Metrics.bodyPx(root.width) + 2
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }
}
