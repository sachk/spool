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

    width: 44
    height: 44
    focusPolicy: Qt.StrongFocus

    background: Rectangle {
        radius: Theme.radiusSmall
        color: tap.pressed ? Theme.bgHover : root.railStyle && root.selected ? Theme.accentPanel : root.selected
                                                                               ? Theme.bgPanel : "transparent"
        border.width: root.activeFocus ? 3 : root.selected ? 1 : root.pointerHovered ? 1 : 0
        border.color: root.activeFocus ? Theme.textPrimary : root.selected ? Theme.accent : Theme.borderStrong
        antialiasing: true

        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            radius: parent.radius + 4
            color: "transparent"
            border.width: root.railStyle && root.activeFocus ? 2 : 0
            border.color: Theme.accent
            opacity: 0.55
        }
    }

    contentItem: Item {
        MaterialIcon {
            anchors.centerIn: parent
            visible: root.iconName.length > 0
            name: root.iconName
            iconSize: root.railStyle ? 24 : 22
            iconColor: root.activeFocus ? Theme.textPrimary : root.selected ? Theme.accent : Theme.textSecondary
        }

        AppText {
            anchors.fill: parent
            visible: root.iconName.length === 0
            text: root.iconText
            font.pixelSize: 20
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
