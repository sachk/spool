import QtQuick
import "../theme"

Item {
    id: root
    property string title: ""
    property string posterUrl: ""
    property int year: 0
    property bool focused: false
    property bool loadImage: true
    property int imageLoadDelay: 0
    property string metadata: ""
    readonly property real metadataHeight: yearLabel.text.length > 0 ? yearLabel.implicitHeight : 0
    readonly property real titleAvailableHeight: Math.max(0, height - poster.height - 10 - metadataHeight)

    implicitWidth: 210
    implicitHeight: poster.height + titleLabel.implicitHeight + yearLabel.implicitHeight + 10
    clip: true

    ImageCard {
        id: poster
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: width * 1.5
        imageUrl: root.posterUrl
        loadImage: root.loadImage
        loadDelay: root.imageLoadDelay
        fallbackText: root.year > 0 ? String(root.year) : "Poster"
        focused: root.focused
    }

    AppText {
        id: titleLabel
        anchors.top: poster.bottom
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.right: parent.right
        height: Math.min(implicitHeight, root.titleAvailableHeight)
        visible: height > 0 && text.length > 0
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
        height: Math.min(implicitHeight, Math.max(0, root.height - y))
        visible: height > 0 && text.length > 0
        text: root.metadata.length > 0 ? root.metadata : root.year > 0 ? String(root.year) : ""
        font.pixelSize: Metrics.metaPx(root.Window.window ? root.Window.window.width : 1920)
        color: Theme.textMuted
        elide: Text.ElideRight
    }
}
