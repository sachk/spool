import QtQuick
import "../theme"

Item {
    id: root

    property var info: null
    property int rightInset: Metrics.scaled(12)
    readonly property bool hasContent: infoLineRow.implicitWidth > 0

    implicitWidth: infoLineRow.implicitWidth + rightInset
    implicitHeight: Metrics.scaled(40)

    component InfoSegment: Row {
        property string iconName
        property string text
        visible: text.length > 0
        spacing: Metrics.scaled(6)

        MaterialIcon {
            anchors.verticalCenter: parent.verticalCenter
            name: parent.iconName
            iconSize: Metrics.metaSizePx + 4
            iconColor: Theme.textSecondary
        }

        MonoText {
            anchors.verticalCenter: parent.verticalCenter
            text: parent.text
            color: Theme.textPrimary
            font.pixelSize: Metrics.metaSizePx
            maximumLineCount: 1
        }
    }

    Row {
        id: infoLineRow
        readonly property var values: root.info || ({})
        readonly property string resolutionText: String(values.resolution || "")
        anchors.right: parent.right
        anchors.rightMargin: root.rightInset
        anchors.verticalCenter: parent.verticalCenter
        spacing: Metrics.scaled(16)

        InfoSegment {
            iconName: "calendar_month"
            text: String(infoLineRow.values.date || "")
        }

        InfoSegment {
            iconName: "schedule"
            text: String(infoLineRow.values.runtime || "")
        }

        Rectangle {
            visible: infoLineRow.resolutionText.length > 0
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -Metrics.scaled(1)
            width: resolutionLabel.implicitWidth + Metrics.scaled(12)
            height: resolutionLabel.implicitHeight + Metrics.scaled(5)
            radius: Theme.radiusTiny
            color: "transparent"
            border.width: Theme.hoverBorderWidth
            border.color: Theme.textSecondary

            MonoText {
                id: resolutionLabel
                anchors.centerIn: parent
                text: infoLineRow.resolutionText
                color: Theme.textPrimary
                font.pixelSize: Metrics.metaSizePx - 1
                font.weight: Font.DemiBold
                maximumLineCount: 1
            }
        }

        InfoSegment {
            iconName: "graphic_eq"
            text: String(infoLineRow.values.audio || "")
        }

        InfoSegment {
            iconName: "subtitles"
            text: String(infoLineRow.values.subs || "")
        }
    }
}
