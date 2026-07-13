import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

Item {
    id: root

    property var stats: []

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: Metrics.scaled(24)
        width: Math.min(parent.width - Metrics.scaled(48), Math.max(Metrics.scaled(680), content.implicitWidth
                                                                    + Metrics.scaled(32)))
        height: content.implicitHeight + Metrics.scaled(28)
        color: "#E6080B10"
        border.width: 1
        border.color: Theme.borderStrong
        radius: Theme.radiusMedium

        ColumnLayout {
            id: content

            anchors.fill: parent
            anchors.margins: Metrics.scaled(14)
            spacing: Metrics.scaled(4)

            MonoText {
                text: "Playback performance · 1 second samples"
                color: Theme.textPrimary
                font.weight: Font.DemiBold
            }

            Repeater {
                model: root.stats

                MonoText {
                    required property string modelData

                    text: modelData
                    color: Theme.textSecondary
                }
            }
        }
    }
}
