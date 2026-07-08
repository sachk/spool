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
            font.pixelSize: Metrics.bodyPx(root.Window.window ? root.Window.window.width : 1920) + 2
            font.weight: Font.DemiBold
        }
        MonoText {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.detail
            color: Theme.textMuted
            font.pixelSize: Metrics.metaPx(root.Window.window ? root.Window.window.width : 1920)
        }
    }
}
