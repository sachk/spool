import QtQuick
import "../theme"

Item {
    id: root
    property string title: ""
    property string detail: ""
    implicitHeight: Math.max(titleText.implicitHeight, detailText.implicitHeight)

    AppText {
        id: titleText
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        text: root.title
        font.pixelSize: Metrics.bodySizePx + 4
        font.weight: Font.DemiBold
    }

    MonoText {
        id: detailText
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        text: root.detail
        color: Theme.textMuted
        font.pixelSize: Metrics.metaSizePx
    }
}
