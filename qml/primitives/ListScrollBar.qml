import QtQuick
import "../theme"

Item {
    id: root

    required property Flickable flickable
    property bool interactive: true
    property real minimumSize: 0.04
    readonly property int visualWidth: Math.max(Metrics.scaled(10), 10)
    readonly property bool hovered: trackHover.hovered || handleHover.hovered || drag.active
    readonly property bool pressed: drag.active
    readonly property real handleCenterY: handle.y + handle.height / 2

    readonly property real scrollRange: Math.max(0, flickable.contentHeight - flickable.height)
    readonly property real availableTrack: Math.max(0, height - handle.height)
    readonly property real visibleFraction: flickable.contentHeight > 0 ? Math.min(1, flickable.height
                                                                                   / flickable.contentHeight) : 1

    visible: scrollRange > 0
    implicitWidth: Math.ceil(visualWidth * 3)

    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: root.visualWidth
        radius: width / 2
        color: Theme.bgRaised
        opacity: root.hovered ? 0.8 : 0

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.reducedMotion ? 0 : 90
            }
        }
    }

    HoverHandler {
        id: trackHover
        enabled: root.interactive
    }

    TapHandler {
        enabled: root.interactive
        onTapped: point => {
            if (root.availableTrack <= 0)
                return
            const targetY = Math.max(0, Math.min(root.availableTrack, point.position.y - handle.height / 2))
            root.flickable.contentY = targetY * root.scrollRange / root.availableTrack
        }
    }

    Rectangle {
        id: handle

        width: Math.max(Metrics.scaled(6), 6)
        height: Math.max(root.height * root.minimumSize, root.height * root.visibleFraction)
        x: root.width - (root.visualWidth + width) / 2
        y: root.scrollRange > 0 ? root.availableTrack * root.flickable.contentY / root.scrollRange : 0
        radius: Math.max(1, width / 3)
        color: Theme.textMuted

        HoverHandler {
            id: handleHover
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
