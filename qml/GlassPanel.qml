import QtQuick

Rectangle {
    id: panel
    property color panelColor: "#a80c1722"
    property color edgeColor: "#344e6074"
    property bool elevated: true

    radius: 30
    color: panelColor
    border.width: 1
    border.color: edgeColor
    antialiasing: true
    layer.enabled: elevated
    layer.smooth: true

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: parent.radius - 1
        color: "transparent"
        border.width: 1
        border.color: "#18ffffff"
        antialiasing: true
    }
}
