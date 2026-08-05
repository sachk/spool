import QtQuick
import QtQuick.Layouts
import "../theme"

Item {
    id: root

    Component.onCompleted: InputLatency.noteDelegate("menu_row", 1)
    Component.onDestruction: InputLatency.noteDelegate("menu_row", -1)

    property string label: ""
    property string detail: ""
    property string iconName: ""
    property bool checked: false
    property bool section: false
    property bool highlighted: false
    property bool actionable: true
    property bool pointerActivationEnabled: true
    property int metricsWidth: root.Window.window ? root.Window.window.width : 1920
    property int rowHeight: Metrics.scaled(detail.length > 0 ? 54 : 46)
    property string checkIconName: "done"
    property bool stepperVisible: false
    property bool stepperEnabled: true
    property string stepperText: ""

    signal activated
    signal hovered
    signal decreaseRequested
    signal increaseRequested

    width: parent ? parent.width : Metrics.scaled(320)
    height: section ? Metrics.scaled(34) : rowHeight

    AppText {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        visible: root.section
        text: root.label
        color: Theme.textMuted
        font.pixelSize: Metrics.metaSizePx
        font.weight: Font.DemiBold
        maximumLineCount: 1
        elide: Text.ElideRight
    }

    Rectangle {
        anchors.fill: parent
        visible: !root.section
        radius: Theme.radiusSmall
        color: root.highlighted ? Theme.focusedFill : hover.hovered && root.actionable ? Theme.bgHover : root.checked
                                                                                         ? Theme.accentPanel :
                                                                                           "transparent"
        border.width: root.highlighted ? Theme.hoverBorderWidth : 0
        border.color: Theme.accent
        antialiasing: true
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Metrics.scaled(10)
        anchors.rightMargin: Metrics.scaled(10)
        visible: !root.section
        spacing: Metrics.scaled(10)

        MaterialIcon {
            visible: root.iconName.length > 0
            Layout.preferredWidth: Metrics.scaled(28)
            Layout.preferredHeight: width
            name: root.iconName
            iconSize: Metrics.scaled(22)
            iconColor: root.checked || root.highlighted ? Theme.accent : root.actionable ? Theme.textSecondary :
                                                                                           Theme.textMuted

        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Metrics.scaled(1)

            AppText {
                Layout.fillWidth: true
                text: root.label
                color: root.actionable || root.highlighted ? Theme.textPrimary : Theme.textSecondary
                font.pixelSize: Metrics.bodySizePx
                font.weight: root.highlighted ? Font.DemiBold : Font.Medium
                maximumLineCount: 1
                elide: Text.ElideRight
            }
            SecondaryText {
                Layout.fillWidth: true
                visible: root.detail.length > 0
                text: root.detail
                color: Theme.textMuted
                font.pixelSize: Metrics.metaSizePx - 1
                maximumLineCount: 1
                elide: Text.ElideRight
            }
        }

        RowLayout {
            visible: root.stepperVisible
            spacing: Metrics.scaled(6)

            Rectangle {
                Layout.preferredWidth: Metrics.scaled(38)
                Layout.preferredHeight: Metrics.scaled(34)
                radius: Theme.radiusSmall
                color: minusHover.hovered && root.stepperEnabled ? Theme.bgHover : Theme.bgPanel
                border.width: Theme.hoverBorderWidth
                border.color: root.stepperEnabled ? Theme.borderStrong : Theme.border

                AppText {
                    anchors.centerIn: parent
                    text: "−"
                    color: root.stepperEnabled ? Theme.textPrimary : Theme.textDisabled
                    font.pixelSize: Metrics.titleSizePx
                    font.weight: Font.DemiBold
                }
                TapHandler {
                    enabled: root.stepperEnabled
                    onTapped: root.decreaseRequested()
                }
                HoverHandler {
                    id: minusHover
                    enabled: root.stepperEnabled
                }
            }

            SecondaryText {
                Layout.preferredWidth: Metrics.scaled(68)
                text: root.stepperText
                color: root.stepperEnabled ? Theme.textPrimary : Theme.textSecondary
                font.pixelSize: Metrics.bodySizePx
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }

            Rectangle {
                Layout.preferredWidth: Metrics.scaled(38)
                Layout.preferredHeight: Metrics.scaled(34)
                radius: Theme.radiusSmall
                color: plusHover.hovered && root.stepperEnabled ? Theme.bgHover : Theme.bgPanel
                border.width: Theme.hoverBorderWidth
                border.color: root.stepperEnabled ? Theme.borderStrong : Theme.border

                AppText {
                    anchors.centerIn: parent
                    text: "+"
                    color: root.stepperEnabled ? Theme.textPrimary : Theme.textDisabled
                    font.pixelSize: Metrics.titleSizePx
                    font.weight: Font.DemiBold
                }
                TapHandler {
                    enabled: root.stepperEnabled
                    onTapped: root.increaseRequested()
                }
                HoverHandler {
                    id: plusHover
                    enabled: root.stepperEnabled
                }
            }
        }

        MaterialIcon {
            visible: root.checked && root.checkIconName.length > 0
            Layout.preferredWidth: Metrics.scaled(24)
            Layout.preferredHeight: width
            name: root.checkIconName
            iconSize: Metrics.scaled(21)
            iconColor: Theme.accent
        }
    }

    HoverHandler {
        id: hover
        enabled: root.actionable && !root.section
        onHoveredChanged: if (hovered)
                              root.hovered()
    }

    TapHandler {
        enabled: root.pointerActivationEnabled && root.actionable && !root.section
        onTapped: root.activated()
    }
}
