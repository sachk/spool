import QtQuick
import "../theme"

Item {
    id: root

    property real from: 0
    property real to: 1
    property real value: from
    property real stepSize: 0
    property real barHeight: 6
    property real handleSize: 16

    signal moved

    readonly property real range: Math.max(0, to - from)
    readonly property real position: range > 0 ? Math.max(0, Math.min(1, (value - from) / range)) : 0

    implicitHeight: Math.max(handleSize, barHeight)

    function updateValue(pointerX) {
        const fraction = Math.max(0, Math.min(1, pointerX / Math.max(1, width)))
        let next = from + fraction * range
        if (stepSize > 0)
            next = Math.round((next - from) / stepSize) * stepSize + from
        value = Math.max(from, Math.min(to, next))
        moved()
    }

    Rectangle {
        id: track
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: root.barHeight
        radius: height / 2
        color: Theme.borderStrong

        Rectangle {
            width: parent.width * root.position
            height: parent.height
            radius: parent.radius
            color: Theme.textSecondary
        }
    }

    Rectangle {
        x: root.position * (root.width - width)
        anchors.verticalCenter: parent.verticalCenter
        width: root.handleSize
        height: width
        radius: width / 2
        color: Theme.textPrimary
        border.width: 1
        border.color: Theme.bg
    }

    TapHandler {
        onTapped: eventPoint => root.updateValue(eventPoint.position.x)
    }

    DragHandler {
        target: null
        xAxis.enabled: true
        yAxis.enabled: false
        onCentroidChanged: if (active)
                               root.updateValue(centroid.position.x)
    }
}
