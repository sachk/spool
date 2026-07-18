import QtQuick
import "../theme"

SettingRow {
    id: root

    property var options: []
    property int currentIndex: 0
    property real metricsWidth: 1920
    readonly property string selectedText: options.length > 0 ? String(options[Math.max(0, Math.min(options.length - 1,
                                                                                                    currentIndex))]) :
                                                                ""

    signal selected(int index, string value)
    signal opened

    valueText: selectedText
    valueTextVisible: false

    function move(direction) {
        if (options.length <= 0)
            return false
        opened()
        return true
    }

    onClicked: opened()

    Rectangle {
        anchors.right: parent.right
        anchors.rightMargin: Metrics.scaled(14)
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(Math.max(valueLabel.implicitWidth + Metrics.scaled(64), Metrics.scaled(220)), Math.max(Metrics.scaled(
                                                                                                                   220), root.width
                                                                                                               * 0.46))
        height: Math.max(Metrics.scaled(40), Metrics.controlHeightPx)
        radius: Theme.radiusMedium
        color: root.rowFocus ? Theme.accentPanel : Theme.bgRaised
        border.width: root.rowFocus ? Theme.focusBorderWidth : Theme.hoverBorderWidth
        border.color: root.rowFocus ? Theme.accent : Theme.borderStrong
        antialiasing: true

        AppText {
            id: valueLabel
            anchors.left: parent.left
            anchors.right: dropdownIcon.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.leftMargin: Metrics.scaled(16)
            anchors.rightMargin: Metrics.scaled(10)
            text: root.selectedText
            color: Theme.textPrimary
            font.pixelSize: Metrics.metaSizePx
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        MaterialIcon {
            id: dropdownIcon
            anchors.right: parent.right
            anchors.rightMargin: Metrics.scaled(14)
            anchors.verticalCenter: parent.verticalCenter
            name: "expand_more"
            iconSize: Math.max(20, Metrics.iconSizePx)
            iconColor: root.options.length > 0 ? Theme.textPrimary : Theme.textDisabled
        }
    }
}
