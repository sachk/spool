import QtQuick
import "../theme"

// Small caps label with a rule running out to the right edge. Settings lists
// use this rather than SectionHeader, whose weight belongs to content rows.
Item {
    id: root
    property string title: ""

    implicitHeight: label.implicitHeight

    AppText {
        id: label
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        text: root.title
        color: Theme.textSecondary
        font.pixelSize: Metrics.metaSizePx
        font.weight: Font.DemiBold
        font.capitalization: Font.AllUppercase
        font.letterSpacing: Math.max(1, Metrics.scaled(1))
    }

    Rectangle {
        anchors.left: label.right
        anchors.leftMargin: Metrics.scaled(14)
        anchors.right: parent.right
        anchors.verticalCenter: label.verticalCenter
        height: Math.max(1, Metrics.scaled(1))
        color: Theme.border
    }
}
