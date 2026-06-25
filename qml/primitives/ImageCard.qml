import QtQuick
import "../theme"

Item {
    id: root
    property string imageUrl: ""
    property string fallbackText: ""
    property real aspectRatio: 2 / 3
    property bool focused: false
    property bool hovered: hover.hovered
    property bool retainWhileLoading: false
    property bool loadImage: true
    property int loadDelay: 0
    property string activeImageUrl: ""

    function artworkSource(url) {
        if (url.indexOf("http://") === 0 || url.indexOf("https://") === 0)
            return "image://artwork/" + encodeURIComponent(url)
        return url
    }

    function updateActiveImage(clearCurrent) {
        loadTimer.stop()
        if (clearCurrent)
            activeImageUrl = ""
        if (!loadImage || imageUrl.length <= 0) {
            activeImageUrl = ""
            return
        }
        if (loadDelay <= 0) {
            activeImageUrl = imageUrl
            return
        }
        loadTimer.restart()
    }

    onImageUrlChanged: updateActiveImage(true)
    onLoadImageChanged: updateActiveImage(!loadImage)
    onLoadDelayChanged: updateActiveImage(false)
    Component.onCompleted: updateActiveImage(false)

    clip: false
    scale: focused && !Theme.reducedMotion ? 1.025 : 1.0
    Behavior on scale { enabled: !Theme.reducedMotion; NumberAnimation { duration: 100; easing.type: Easing.OutQuad } }

    Rectangle {
        id: frame
        anchors.fill: parent
        radius: Theme.radiusMedium
        color: Theme.bgRaised
        border.width: root.focused ? 2 : root.hovered ? 1 : 1
        border.color: root.focused ? Theme.accent : root.hovered ? Theme.borderStrong : Theme.border
        clip: true
        antialiasing: true

        Image {
            id: artwork
            anchors.fill: parent
            source: root.activeImageUrl.length > 0 ? root.artworkSource(root.activeImageUrl) : ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            smooth: true
            mipmap: false
            sourceSize.width: Math.max(1, Math.round(width * Screen.devicePixelRatio))
            sourceSize.height: Math.max(1, Math.round(height * Screen.devicePixelRatio))
            opacity: status === Image.Ready ? 1 : root.retainWhileLoading ? 0.35 : 0
        }

        Rectangle {
            anchors.fill: parent
            visible: artwork.status !== Image.Ready
            color: Theme.bgRaised
            border.width: 1
            border.color: Theme.border

            MonoText {
                anchors.centerIn: parent
                width: parent.width - 20
                text: root.fallbackText.length > 0 ? root.fallbackText : "..."
                color: Theme.textMuted
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                font.pixelSize: 13
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 3
            visible: root.focused
            color: Theme.accentPurple
        }
    }

    Timer {
        id: loadTimer
        interval: root.loadDelay
        repeat: false
        onTriggered: if (root.loadImage) root.activeImageUrl = root.imageUrl
    }

    HoverHandler { id: hover }
}
