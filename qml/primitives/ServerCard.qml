import QtQuick
import QtQuick.Layouts
import "../theme"

// One server row. Discovered, saved and just-typed servers all use this, so
// the status column has to carry a live connection attempt as well as a
// settled result; `tone` is what colours it.
FocusScope {
    id: root

    property string title: ""
    property string serverAddress: ""
    property string detail: ""
    property string status: ""
    // neutral | pending | positive | negative
    property string tone: "neutral"
    property bool selectable: true
    property bool focused: activeFocus

    readonly property color toneColor: tone === "positive" ? Theme.success : tone === "pending" ? Theme.pending : tone
                                                                                                  === "negative"
                                                                                                  ? Theme.errorText :
                                                                                                    Theme.textSecondary

    signal accepted

    implicitHeight: Metrics.scaled(detail.length > 0 ? 112 : 94)
    focusPolicy: Qt.StrongFocus

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusMedium
        color: root.focused ? Theme.accentPanel : hover.hovered ? Theme.bgHover : Theme.bgRaised
        border.width: root.focused ? Theme.focusBorderWidth : 1
        border.color: root.focused ? Theme.accent : hover.hovered ? Theme.borderStrong : Theme.border
        opacity: root.selectable ? 1.0 : 0.68
        antialiasing: true
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Metrics.scaled(18)
        spacing: Metrics.scaled(16)

        MaterialIcon {
            name: "dns"
            iconSize: Metrics.scaled(30)
            iconColor: root.focused ? Theme.accent : Theme.textSecondary
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Metrics.scaled(5)

            AppText {
                Layout.fillWidth: true
                text: root.title
                font.pixelSize: Metrics.bodySizePx + Metrics.scaled(3)
                font.weight: Font.Medium
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            SecondaryText {
                Layout.fillWidth: true
                text: root.serverAddress
                color: Theme.textMuted
                font.pixelSize: Metrics.metaSizePx + Metrics.scaled(3)
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            SecondaryText {
                Layout.fillWidth: true
                visible: root.detail.length > 0
                text: root.detail
                color: Theme.textMuted
                font.pixelSize: Metrics.metaSizePx
                maximumLineCount: 1
                elide: Text.ElideRight
            }
        }

        AppText {
            visible: root.status.length > 0
            text: root.status
            color: root.toneColor
            font.pixelSize: Metrics.metaSizePx + Metrics.scaled(3)
            font.weight: Font.Medium
            maximumLineCount: 1
        }
    }

    TapHandler {
        enabled: root.selectable
        onTapped: root.accepted()
    }

    HoverHandler {
        id: hover
    }
}
