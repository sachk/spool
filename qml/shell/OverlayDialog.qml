import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    default property alias content: body.data
    property real preferredWidth: 620
    property real preferredHeight: -1
    property real padding: 24
    property color panelColor: Theme.bgPanel
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
        height: Math.min(parent.height - 96, root.preferredHeight > 0 ? root.preferredHeight : body.implicitHeight
                                                                        + root.padding * 2)
        elevated: true
        baseColor: root.panelColor

        MouseArea {
            anchors.fill: parent
        }

        ColumnLayout {
            id: body
            anchors.fill: parent
            anchors.margins: root.padding
            spacing: 14
        }
    }
}
