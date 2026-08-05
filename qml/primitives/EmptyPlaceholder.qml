import QtQuick
import "../theme"

Surface {
    id: root
    property string title: "Nothing here yet"
    property string detail: "..."
    implicitHeight: 128

    Column {
        anchors.centerIn: parent
        spacing: 8
        AppText {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.title
            font.pixelSize: Metrics.bodySizePx + 2
            font.weight: Font.DemiBold
        }
        SecondaryText {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.detail
            color: Theme.textMuted
            font.pixelSize: Metrics.metaSizePx
        }
    }
}
