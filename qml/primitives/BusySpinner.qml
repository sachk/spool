import QtQuick

Item {
    id: root

    property bool running: visible
    property color color: "#eeeeee"

    implicitWidth: 28
    implicitHeight: 28
    rotation: 0

    Repeater {
        model: 8

        delegate: Rectangle {
            required property int index

            width: Math.max(2, root.width * 0.14)
            height: width
            radius: width / 2
            color: root.color
            opacity: 0.25 + index * 0.09
            x: root.width / 2 + Math.cos(index * Math.PI / 4) * root.width * 0.34 - width / 2
            y: root.height / 2 + Math.sin(index * Math.PI / 4) * root.height * 0.34 - height / 2
        }
    }

    RotationAnimator on rotation {
        running: root.running
        from: 0
        to: 360
        duration: 850
        loops: Animation.Infinite
    }
}
