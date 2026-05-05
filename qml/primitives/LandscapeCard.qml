import QtQuick
import "../theme"

Item {
    id: root
    property string title: ""
    property string subtitle: ""
    property string imageUrl: ""
    property real progress: 0
    property bool focused: false

    implicitWidth: 320
    implicitHeight: art.height + 8 + titleLabel.implicitHeight + 2 + subtitleLabel.implicitHeight

    ImageCard {
        id: art
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: width * 9 / 16
        imageUrl: root.imageUrl
        fallbackText: root.subtitle
        focused: root.focused
        retainWhileLoading: true
    }

    Rectangle {
        anchors.left: art.left
        anchors.right: art.right
        anchors.bottom: art.bottom
        height: 4
        visible: root.progress > 0
        color: "#66000000"
        Rectangle {
            width: parent.width * Math.max(0, Math.min(1, root.progress))
            height: parent.height
            color: Theme.accent
        }
    }

    AppText {
        id: titleLabel
        anchors.top: art.bottom
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.right: parent.right
        text: root.title
        font.pixelSize: Metrics.bodyPx(root.Window.window ? root.Window.window.width : 1920)
        font.weight: Font.Medium
        maximumLineCount: 2
        wrapMode: Text.Wrap
        elide: Text.ElideRight
    }

    MonoText {
        id: subtitleLabel
        anchors.top: titleLabel.bottom
        anchors.topMargin: 2
        anchors.left: parent.left
        anchors.right: parent.right
        text: root.subtitle
        color: Theme.textMuted
        font.pixelSize: Metrics.metaPx(root.Window.window ? root.Window.window.width : 1920)
        elide: Text.ElideRight
    }
}
