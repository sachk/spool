import QtQuick
import "../theme"

Item {
    id: root
    property string title: ""
    implicitHeight: titleText.implicitHeight

    AppText {
        id: titleText
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        text: root.title
        font.pixelSize: Metrics.bodySizePx + 4
        font.weight: Font.DemiBold
        elide: Text.ElideRight
        maximumLineCount: 1
    }
}
