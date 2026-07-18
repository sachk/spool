import QtQuick
import "../theme"

Item {
    id: root
    property string imageUrl: ""
    property string fallbackText: ""
    property bool artworkVisible: true
    property bool artworkEnabled: true
    readonly property bool artworkReady: !artworkEnabled || imageUrl.length === 0 || artwork.status === Image.Ready || artwork.status
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
        border.width: Theme.hoverBorderWidth
        border.color: Theme.border
        clip: true

        Loader {
            anchors.fill: parent
            active: !root.artworkEnabled || root.imageUrl.length === 0 || artwork.status === Image.Error
            sourceComponent: MonoText {
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
            anchors.margins: frame.border.width
            source: root.artworkEnabled && root.imageUrl.length > 0 ? root.artworkSource(root.imageUrl) : ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            // Never display the previous delegate's artwork after its source
            // changes. The fallback remains visible while the new image loads.
            retainWhileLoading: false
            cache: !NativeWindow.smartTvPlatform
            smooth: true
            mipmap: false
            sourceSize.width: Math.max(1, Math.round(root.width * Screen.devicePixelRatio))
            sourceSize.height: Math.max(1, Math.round(root.height * Screen.devicePixelRatio))
            opacity: root.artworkVisible ? 1 : 0
            Behavior on opacity {
                NumberAnimation {
                    duration: Theme.reducedMotion ? 0 : 90
                }
            }
        }
    }
}
