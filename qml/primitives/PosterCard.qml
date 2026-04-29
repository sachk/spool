import QtQuick
import "../theme"

Item {
    id: root
    property string title: ""
    property string posterUrl: ""
    property int year: 0
    property bool focused: false
    property string metadata: ""

    implicitWidth: 210
    implicitHeight: poster.height + titleLabel.implicitHeight + yearLabel.implicitHeight + 10

    ImageCard {
        id: poster
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: width * 1.5
        imageUrl: root.posterUrl
        fallbackText: root.year > 0 ? String(root.year) : "Poster"
        focused: root.focused
    }

    AppText {
        id: titleLabel
        anchors.top: poster.bottom
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.right: parent.right
        text: root.title
        font.pixelSize: Metrics.bodyPx(root.Window.window ? root.Window.window.width : 1920)
        font.weight: Font.Medium
        color: root.focused ? Theme.textPrimary : Theme.textSecondary
        maximumLineCount: 2
        wrapMode: Text.Wrap
        elide: Text.ElideRight
    }

    MonoText {
        id: yearLabel
        anchors.top: titleLabel.bottom
        anchors.topMargin: 2
        anchors.left: parent.left
        anchors.right: parent.right
        text: root.metadata.length > 0 ? root.metadata : root.year > 0 ? String(root.year) : ""
        font.pixelSize: Metrics.metaPx(root.Window.window ? root.Window.window.width : 1920)
        color: Theme.textMuted
        elide: Text.ElideRight
    }
}
