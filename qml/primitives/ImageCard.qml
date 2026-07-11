import QtQuick
import "../theme"

Item {
    id: root
    property string imageUrl: ""
    property string fallbackText: ""
    property bool focused: false
    property bool artworkVisible: true
    readonly property bool artworkReady: imageUrl.length === 0 || artwork.status === Image.Ready || artwork.status
                                         === Image.Error

    function artworkSource(url) {
        if (url.indexOf("http://") === 0 || url.indexOf("https://") === 0)
            return "image://artwork/" + encodeURIComponent(url)
        return url
    }

    Rectangle {
        id: frame
        anchors.fill: parent
        radius: Theme.radiusMedium
        color: Theme.bgRaised
        border.width: root.focused ? Theme.focusBorderWidth : Theme.hoverBorderWidth
        border.color: root.focused ? Theme.accent : Theme.border
        clip: true
        antialiasing: true

        MonoText {
            anchors.centerIn: parent
            width: parent.width - Metrics.scaled(20)
            text: root.fallbackText.length > 0 ? root.fallbackText : "..."
            color: Theme.textMuted
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            font.pixelSize: Metrics.scaled(13)
        }

        Image {
            id: artwork
            anchors.fill: parent
            anchors.margins: frame.border.width
            source: root.imageUrl.length > 0 ? root.artworkSource(root.imageUrl) : ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            retainWhileLoading: true
            cache: true
            smooth: true
            mipmap: false
            sourceSize.width: Math.max(1, Math.round(root.width * Screen.devicePixelRatio))
            sourceSize.height: Math.max(1, Math.round(root.height * Screen.devicePixelRatio))
            opacity: root.artworkVisible ? 1 : 0
        }
    }
}
