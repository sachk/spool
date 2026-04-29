import QtQuick
import "../theme"

Surface {
    id: root
    property string text: ""
    property bool selected: false
    focused: activeFocus || selected
    radius: Theme.radiusSmall
    implicitWidth: label.implicitWidth + 18
    implicitHeight: label.implicitHeight + 9

    AppText {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.selected ? Theme.textPrimary : Theme.textSecondary
        font.pixelSize: Metrics.metaPx(root.Window.window ? root.Window.window.width : 1920)
    }
}
