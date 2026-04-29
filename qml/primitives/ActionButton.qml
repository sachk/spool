import QtQuick
import QtQuick.Controls.Basic
import "../theme"

Button {
    id: root
    property string kind: "secondary"
    property bool pointerHovered: hover.hovered

    implicitWidth: Math.max(132, label.implicitWidth + 34)
    implicitHeight: 44
    focusPolicy: Qt.StrongFocus

    background: Rectangle {
        radius: Theme.radiusMedium
        color: root.down ? Theme.bgRaised : root.kind === "flat" ? "transparent" : Theme.bgPanel
        border.width: root.activeFocus ? 2 : root.pointerHovered ? 1 : root.kind === "flat" ? 0 : 1
        border.color: root.activeFocus ? Theme.accent : root.pointerHovered ? Theme.borderStrong : Theme.border
        antialiasing: true

        gradient: Gradient {
            GradientStop { position: 0.0; color: root.kind === "primary" ? Theme.jellyfinBlue : root.down ? Theme.bgRaised : root.kind === "flat" ? "transparent" : Theme.bgPanel }
            GradientStop { position: 1.0; color: root.kind === "primary" ? Theme.jellyfinPurple : root.down ? Theme.bgRaised : root.kind === "flat" ? "transparent" : Theme.bgPanel }
        }
    }

    contentItem: AppText {
        id: label
        text: root.text
        color: root.enabled ? Theme.textPrimary : Theme.textDisabled
        font.pixelSize: Metrics.bodyPx(root.Window.window ? root.Window.window.width : 1920)
        font.weight: root.kind === "primary" ? Font.DemiBold : Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    HoverHandler { id: hover }
}
