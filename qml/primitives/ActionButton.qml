import QtQuick
import QtQuick.Templates as T
import "../theme"

T.Control {
    id: root
    property string kind: "secondary"
    property string iconName: ""
    property bool pointerHovered: hover.hovered
    property string text: ""
    signal clicked

    implicitWidth: Math.max(Metrics.scaled(132), buttonContent.implicitWidth + Metrics.scaled(34))
    implicitHeight: Metrics.controlHeightPx
    focusPolicy: Qt.StrongFocus

    background: Rectangle {
        radius: Theme.radiusMedium
        color: root.kind === "primary" ? (tap.pressed ? Theme.accentDim : (root.pointerHovered || root.activeFocus
                                                                           ? Theme.accent : Theme.accentDim)) :
                                         root.kind === "danger" ? (tap.pressed ? Theme.bgRaised : Theme.errorPanel) :
                                                                  tap.pressed ? Theme.bgRaised : root.kind === "flat"
                                                                                ? "transparent" : Theme.bgPanel
        border.width: root.activeFocus ? Theme.focusBorderWidth : root.pointerHovered ? Theme.hoverBorderWidth :
                                                                                        root.kind === "flat" ? 0 :
                                                                                                               Theme.hoverBorderWidth
        border.color: root.activeFocus ? Theme.textPrimary : root.pointerHovered ? Theme.borderStrong : root.kind
                                                                                   === "primary" ? Theme.accentDim :
                                                                                                   root.kind
                                                                                                   === "danger"
                                                                                                   ? Theme.errorText :
                                                                                                     Theme.border
        antialiasing: true
    }

    contentItem: Item {
        clip: true
        Row {
            id: buttonContent
            anchors.centerIn: parent
            spacing: Metrics.scaled(8)

            MaterialIcon {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.iconName.length > 0
                name: root.iconName
                iconSize: Metrics.bodySizePx + 6
                iconColor: root.enabled ? Theme.textPrimary : Theme.textDisabled
            }

            AppText {
                anchors.verticalCenter: parent.verticalCenter
                text: root.text
                color: root.enabled ? (root.kind === "danger" ? Theme.errorText : Theme.textPrimary) :
                                      Theme.textDisabled

                font.pixelSize: Metrics.bodySizePx
                font.weight: root.kind === "primary" || root.kind === "danger" ? Font.DemiBold : Font.Medium
                elide: Text.ElideRight
                maximumLineCount: 1
            }
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
