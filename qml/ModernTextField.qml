import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic

TextField {
    id: field
    property color accent: "#74ffd7"

    implicitHeight: 64
    padding: 18
    leftPadding: 22
    rightPadding: 22
    color: "#f3fbff"
    placeholderTextColor: "#7894a5"
    selectionColor: "#5574ffd7"
    selectedTextColor: "#ffffff"
    font.pixelSize: 22

    background: Rectangle {
        radius: 22
        antialiasing: true
        color: field.activeFocus ? "#2c3d4bdd" : "#24131f2add"
        border.width: 1
        border.color: field.activeFocus ? field.accent : "#355164"

        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            radius: parent.radius + 4
            color: "transparent"
            border.width: field.activeFocus ? 2 : 0
            border.color: "#4474ffd7"
            antialiasing: true
        }
    }
}
