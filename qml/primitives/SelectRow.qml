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

    function move(dir) {
        if (options.length <= 0)
            return
        currentIndex = (currentIndex + dir + options.length) % options.length
        selected(currentIndex, String(options[currentIndex]))
    }

    onClicked: opened()

    Row {
        anchors.right: parent.right
        anchors.rightMargin: Metrics.scaled(14)
        anchors.verticalCenter: parent.verticalCenter
        spacing: Metrics.scaled(8)

        MaterialIcon {
            anchors.verticalCenter: parent.verticalCenter
            name: "unfold_more"
            iconSize: Math.max(18, Metrics.metaPx(root.metricsWidth) + 6)
            iconColor: root.enabled && root.options.length > 0 ? Theme.textSecondary : Theme.textDisabled
        }

        Rectangle {
            width: Math.min(Math.max(valueLabel.implicitWidth + Metrics.scaled(28), Metrics.scaled(112)), Math.max(
                                Metrics.scaled(112), root.width * 0.42))
            height: Math.max(Metrics.scaled(32), Metrics.metaPx(root.metricsWidth) + Metrics.scaled(18))
            radius: Theme.radiusMedium
            color: root.activeFocus ? Theme.accentPanel : Theme.bgRaised
            border.width: root.activeFocus ? Theme.focusBorderWidth : Theme.hoverBorderWidth
            border.color: root.activeFocus ? Theme.accent : Theme.border
            antialiasing: true

            AppText {
                id: valueLabel
                anchors.fill: parent
                anchors.leftMargin: Metrics.scaled(14)
                anchors.rightMargin: Metrics.scaled(14)
                text: root.selectedText
                color: Theme.textPrimary
                font.pixelSize: Metrics.metaPx(root.metricsWidth)
                font.weight: Font.Medium
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }
    }
}
