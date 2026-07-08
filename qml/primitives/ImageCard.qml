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
    readonly property int sourcePixelWidth: Math.max(1, Math.round(width * Screen.devicePixelRatio))
    readonly property int sourcePixelHeight: Math.max(1, Math.round(height * Screen.devicePixelRatio))

    function artworkSource(url) {
        if (url.indexOf("http://") === 0 || url.indexOf("https://") === 0)
            return "image://artwork/" + encodeURIComponent(url)
        return url
    }

    clip: false
    scale: focused && !Theme.reducedMotion ? 1.025 : 1.0
    Behavior on scale {
        enabled: !Theme.reducedMotion
        NumberAnimation {
            duration: 100
            easing.type: Easing.OutQuad
        }
    }

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
            source: root.imageUrl.length > 0 ? root.artworkSource(root.imageUrl) : ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            smooth: true
            mipmap: false
            sourceSize.width: root.sourcePixelWidth
            sourceSize.height: root.sourcePixelHeight
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
    }

    HoverHandler {
        id: hover
    }
}
