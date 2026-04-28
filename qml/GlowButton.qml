import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic

Button {
    id: control
    property color accent: "#74ffd7"
    property color baseColor: "#30172330"
    property color focusColor: "#38506478"
    property bool primary: false

    implicitHeight: 58
    padding: 20
    focusPolicy: Qt.StrongFocus
    font.pixelSize: 22
    font.weight: Font.DemiBold

    contentItem: Label {
        text: control.text
        color: control.primary ? "#061018" : (control.activeFocus || control.hovered ? "#ffffff" : "#d9e8f2")
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: height / 2
        antialiasing: true
        color: control.primary
               ? (control.down ? Qt.darker(control.accent, 1.08) : control.accent)
               : (control.down || control.activeFocus || control.hovered ? control.focusColor : control.baseColor)
        border.width: 1
        border.color: control.primary
                      ? "#d8fff6"
                      : (control.activeFocus || control.hovered ? control.accent : "#405a6d")

        Rectangle {
            anchors.fill: parent
            anchors.margins: -5
            radius: parent.radius + 5
            color: "transparent"
            border.width: control.activeFocus ? 2 : 0
            border.color: "#6674ffd7"
            antialiasing: true
        }

        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on border.color { ColorAnimation { duration: 120 } }
    }
}
