import QtQuick
import QtQuick.Templates as T
import "../theme"

T.Button {
    id: root
    property string kind: "secondary"
    property string iconName: ""
    property bool pointerHovered: hover.hovered

    implicitWidth: Math.max(132, buttonContent.implicitWidth + 34)
    implicitHeight: Metrics.controlHeight(root.Window.window ? root.Window.window.width : 1920)
    focusPolicy: Qt.StrongFocus

    background: Rectangle {
        radius: Theme.radiusMedium
        color: root.kind === "primary" ? root.down ? Theme.accentDim : root.pointerHovered || root.activeFocus ? Theme.accent : Theme.accentDim : root.down ? Theme.bgRaised : root.kind === "flat" ? "transparent" : Theme.bgPanel
        border.width: root.activeFocus ? 2 : root.pointerHovered ? 1 : root.kind === "flat" ? 0 : 1
        border.color: root.activeFocus ? Theme.textPrimary : root.pointerHovered ? Theme.borderStrong : root.kind === "primary" ? Theme.accentDim : Theme.border
        antialiasing: true
    }

    contentItem: Item {
        Row {
            id: buttonContent
            anchors.centerIn: parent
            spacing: 8

            MaterialIcon {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.iconName.length > 0
                name: root.iconName
                iconSize: Metrics.bodyPx(root.Window.window ? root.Window.window.width : 1920) + 6
                iconColor: root.enabled ? Theme.textPrimary : Theme.textDisabled
            }

            AppText {
                anchors.verticalCenter: parent.verticalCenter
                text: root.text
                color: root.enabled ? Theme.textPrimary : Theme.textDisabled
                font.pixelSize: Metrics.bodyPx(root.Window.window ? root.Window.window.width : 1920)
                font.weight: root.kind === "primary" ? Font.DemiBold : Font.Medium
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }
    }

    HoverHandler {
        id: hover
    }
}
