import QtQuick

WheelHandler {
    id: root

    required property var flickable
    property bool horizontal: false
    property real multiplier: 2.6
    property int stepPixels: 260

    target: null
    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

    function clamp(value, minValue, maxValue) {
        return Math.max(minValue, Math.min(maxValue, value))
    }

    onWheel: (event) => {
        if (!flickable)
            return

        const pixelDelta = root.horizontal
                ? (event.pixelDelta.x !== 0 ? event.pixelDelta.x : event.pixelDelta.y)
                : (event.pixelDelta.y !== 0 ? event.pixelDelta.y : event.pixelDelta.x)
        const angleDelta = root.horizontal
                ? (event.angleDelta.x !== 0 ? event.angleDelta.x : event.angleDelta.y)
                : (event.angleDelta.y !== 0 ? event.angleDelta.y : event.angleDelta.x)
        const delta = pixelDelta !== 0 ? pixelDelta : angleDelta / 120 * root.stepPixels
        if (delta === 0)
            return

        if (root.horizontal) {
            const maxX = Math.max(0, flickable.contentWidth - flickable.width)
            flickable.contentX = clamp(flickable.contentX - delta * root.multiplier, 0, maxX)
        } else {
            const maxY = Math.max(0, flickable.contentHeight - flickable.height)
            flickable.contentY = clamp(flickable.contentY - delta * root.multiplier, 0, maxY)
        }
        event.accepted = true
    }
}
