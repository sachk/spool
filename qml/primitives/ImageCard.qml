import QtQuick
import "../theme"

Item {
    id: root
    property string imageUrl: ""
    property string fallbackText: ""
    property real aspectRatio: 2 / 3
    property bool focused: false
    property bool hovered: hover.hovered
    property bool artworkVisible: true
    readonly property int sourcePixelWidth: Math.max(1, Math.round(width * Screen.devicePixelRatio))
    readonly property int sourcePixelHeight: Math.max(1, Math.round(height * Screen.devicePixelRatio))
    readonly property bool artworkReady: imageUrl.length === 0 || artwork.status === Image.Ready || artwork.status
                                         === Image.Error

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
        border.width: root.focused ? Theme.focusBorderWidth : Theme.hoverBorderWidth
        border.color: root.focused ? Theme.accent : root.hovered ? Theme.borderStrong : Theme.border
        clip: true
        antialiasing: true

        Rectangle {
            anchors.fill: parent
            color: Theme.bgRaised
            border.width: Theme.hoverBorderWidth
            border.color: Theme.border

            MonoText {
                anchors.centerIn: parent
                width: parent.width - Metrics.scaled(20)
                text: root.fallbackText.length > 0 ? root.fallbackText : "..."
                color: Theme.textMuted
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                font.pixelSize: Metrics.scaled(13)
            }
        }

        Image {
            id: artwork
            anchors.fill: parent
            source: root.imageUrl.length > 0 ? root.artworkSource(root.imageUrl) : ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            retainWhileLoading: true
            cache: true
            smooth: true
            mipmap: false
            sourceSize.width: root.sourcePixelWidth
            sourceSize.height: root.sourcePixelHeight
            opacity: root.artworkVisible ? 1 : 0
        }

        // Focus ring painted above the artwork: frame's own border renders
        // beneath its children, so a loaded poster covers it completely.
        Rectangle {
            anchors.fill: parent
            radius: frame.radius
            color: "transparent"
            border.width: Theme.focusBorderWidth + Theme.hoverBorderWidth
            border.color: Theme.accent
            visible: root.focused
            antialiasing: true
        }
    }

    HoverHandler {
        id: hover
    }
}
