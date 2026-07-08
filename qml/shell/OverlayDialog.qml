import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    default property alias content: body.data
    property real preferredWidth: 620
    signal dismissed

    anchors.fill: parent
    focus: true

    Rectangle {
        anchors.fill: parent
        color: "#99000000"
        MouseArea {
            anchors.fill: parent
            onClicked: root.dismissed()
        }
    }

    Surface {
        anchors.centerIn: parent
        width: Math.min(parent.width - 96, root.preferredWidth)
        height: Math.min(parent.height - 96, body.implicitHeight + 48)
        elevated: true
        baseColor: Theme.bgPanel

        ColumnLayout {
            id: body
            anchors.fill: parent
            anchors.margins: 24
            spacing: 14
        }
    }
}
