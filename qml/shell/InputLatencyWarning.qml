import QtQuick
import "../theme"
import "../primitives"

Item {
    id: root

    required property var monitor

    visible: monitor.warningVisible

    Surface {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: Metrics.scaled(32)
        width: Math.min(parent.width - Metrics.scaled(64), Metrics.scaled(680))
        height: warningColumn.implicitHeight + Metrics.scaled(28)
        baseColor: Theme.errorPanel

        Column {
            id: warningColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: Metrics.scaled(14)
            spacing: Metrics.scaled(6)

            AppText {
                width: parent.width
                text: root.monitor.warningText
                color: Theme.errorText
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }

            AppText {
                width: parent.width
                text: root.monitor.warningStage
                color: Theme.errorText
                wrapMode: Text.Wrap
            }
        }
    }
}
