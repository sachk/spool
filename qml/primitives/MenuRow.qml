import QtQuick
import QtQuick.Layouts
import "../theme"

Item {
    id: root

    property string label: ""
    property string detail: ""
    property string iconName: ""
    property bool checked: false
    property bool section: false
    property bool highlighted: false
    property bool actionable: true
    property int metricsWidth: root.Window.window ? root.Window.window.width : 1920
    property int rowHeight: Metrics.scaled(detail.length > 0 ? 54 : 46)
    property string checkIconName: "done"

    signal activated
    signal hovered

    width: parent ? parent.width : Metrics.scaled(320)
    height: section ? Metrics.scaled(34) : rowHeight

    AppText {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        visible: root.section
        text: root.label
        color: Theme.textMuted
        font.pixelSize: Metrics.metaPx(root.metricsWidth)
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
                font.pixelSize: Metrics.bodyPx(root.metricsWidth)
                font.weight: root.highlighted ? Font.DemiBold : Font.Medium
                maximumLineCount: 1
                elide: Text.ElideRight
            }
            MonoText {
                Layout.fillWidth: true
                visible: root.detail.length > 0
                text: root.detail
                color: Theme.textMuted
                font.pixelSize: Metrics.metaPx(root.metricsWidth) - 1
                maximumLineCount: 1
                elide: Text.ElideRight
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
        enabled: root.actionable && !root.section
        onTapped: root.activated()
    }
}
