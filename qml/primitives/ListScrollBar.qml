import QtQuick
import "../theme"

Item {
    id: root

    required property Flickable flickable
    property bool interactive: true
    property real minimumSize: 0.04

    readonly property real scrollRange: Math.max(0, flickable.contentHeight - flickable.height)
    readonly property real availableTrack: Math.max(0, height - handle.height)
    readonly property real visibleFraction: flickable.contentHeight > 0 ? Math.min(1, flickable.height
                                                                                   / flickable.contentHeight) : 1

    visible: scrollRange > 0
    implicitWidth: Math.max(Metrics.scaled(12), 12)

    Rectangle {
        anchors.fill: parent
        radius: Math.min(Theme.radiusSmall, width / 4)
        color: Theme.bgPanel
        border.width: Theme.hoverBorderWidth
        border.color: Theme.borderStrong
    }

    Rectangle {
        id: handle

        width: Math.max(Metrics.scaled(8), 8)
        height: Math.max(root.height * root.minimumSize, root.height * root.visibleFraction)
        x: (root.width - width) / 2
        y: root.scrollRange > 0 ? root.availableTrack * root.flickable.contentY / root.scrollRange : 0
        radius: Math.min(Theme.radiusSmall, width / 4)
        color: drag.active || hover.hovered || root.flickable.moving ? Theme.accent : Theme.accentDim
        border.width: Theme.hoverBorderWidth
        border.color: drag.active || hover.hovered ? Theme.textPrimary : Theme.accent

        HoverHandler {
            id: hover
            enabled: root.interactive
        }

        DragHandler {
            id: drag
            enabled: root.interactive
            target: null
            xAxis.enabled: false

            property real startContentY: 0

            onActiveChanged: {
                if (active)
                    startContentY = root.flickable.contentY
            }
            onTranslationChanged: {
                if (!active || root.availableTrack <= 0)
                    return
                root.flickable.contentY = Math.max(0, Math.min(root.scrollRange, startContentY + translation.y
                                                               * root.scrollRange / root.availableTrack))
            }
        }
    }
}
