import QtQuick
import "../theme"

Rectangle {
    id: root
    property bool elevated: false
    property bool focused: false
    property bool hovered: false
    property color baseColor: Theme.bgPanel

    color: hovered ? Theme.bgHover : baseColor
    radius: Theme.radiusPanel
    border.width: focused ? Theme.focusBorderWidth : 1
    border.color: focused ? Theme.accent : hovered ? Theme.borderStrong : Theme.border
    antialiasing: true
    layer.enabled: elevated
    layer.smooth: elevated
}
