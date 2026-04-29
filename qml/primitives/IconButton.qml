import QtQuick
import QtQuick.Controls.Basic
import "../theme"

Button {
    id: root
    property string iconText: ""
    property bool selected: false
    property bool pointerHovered: hover.hovered

    width: 44
    height: 44
    text: iconText
    focusPolicy: Qt.StrongFocus

    background: Rectangle {
        radius: Theme.radiusSmall
        color: root.down ? Theme.bgHover : root.selected ? Theme.bgPanel : "transparent"
        border.width: root.activeFocus || root.selected ? 2 : root.pointerHovered ? 1 : 0
        border.color: root.activeFocus || root.selected ? Theme.accent : Theme.borderStrong
        antialiasing: true
    }

    contentItem: AppText {
        text: root.iconText
        font.pixelSize: 20
        font.weight: Font.DemiBold
        color: root.selected || root.activeFocus ? Theme.textPrimary : Theme.textSecondary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    HoverHandler { id: hover }
}
