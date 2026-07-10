import QtQuick
import QtQuick.Templates as T
import "../theme"

T.Control {
    id: root
    property string iconText: ""
    property string iconName: ""
    property bool selected: false
    property bool checked: false
    property bool railStyle: false
    property bool pointerHovered: hover.hovered
    property string accessibleName: ""
    signal clicked

    width: Metrics.scaled(44)
    height: width
    focusPolicy: Qt.StrongFocus

    background: Rectangle {
        radius: Theme.radiusSmall
        color: tap.pressed ? Theme.bgHover : root.railStyle && root.selected ? Theme.accentPanel : root.selected
                                                                               ? Theme.bgPanel : "transparent"
        border.width: root.activeFocus ? Theme.focusBorderWidth : root.selected || root.pointerHovered
                                         ? Theme.hoverBorderWidth : 0
        border.color: root.activeFocus ? Theme.textPrimary : root.selected ? Theme.accent : Theme.borderStrong
        antialiasing: true

        Rectangle {
            anchors.fill: parent
            anchors.margins: -Metrics.scaled(4)
            radius: parent.radius + Metrics.scaled(4)
            color: "transparent"
            border.width: root.railStyle && root.activeFocus ? Theme.focusBorderWidth : 0
            border.color: Theme.accent
            opacity: 0.55
        }
    }

    contentItem: Item {
        MaterialIcon {
            anchors.centerIn: parent
            visible: root.iconName.length > 0
            name: root.iconName
            iconSize: Metrics.scaled(root.railStyle ? 24 : 22)
            iconColor: root.activeFocus ? Theme.textPrimary : root.selected ? Theme.accent : Theme.textSecondary
        }

        AppText {
            anchors.fill: parent
            visible: root.iconName.length === 0
            text: root.iconText
            font.pixelSize: Metrics.scaled(20)
            font.weight: Font.DemiBold
            color: root.activeFocus ? Theme.textPrimary : root.selected ? Theme.accent : Theme.textSecondary
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    TapHandler {
        id: tap
        onTapped: {
            InputKeys.focus(root)
            root.clicked()
        }
    }

    HoverHandler {
        id: hover
    }
}
