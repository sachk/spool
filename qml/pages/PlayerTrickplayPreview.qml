import QtQuick

Item {
    id: trickplayPreview

    required property var overlay
    readonly property real uiScale: overlay ? overlay.uiScale : 1
    readonly property bool active: overlay && overlay.hasPlayer && overlay.player.trickplayAvailable && (overlay.scrubbing
                                                                                                         || overlay.seekHoldActive
                                                                                                         || overlay.previewBurstActive)
                                   && overlay.mode !== "hidden"
    readonly property var trickplayData: active ? overlay.player.trickplayForSeconds(overlay.scrubbing
                                                                                     ? overlay.scrubSeconds :
                                                                                       overlay.positionSeconds()) : ({})
    readonly property bool dataReady: trickplayData && trickplayData.available === true

    function dp(n) {
        return Math.round(n * uiScale)
    }

    function artworkSource(url) {
        if (!url)
            return ""
        if (url.indexOf("http://") === 0 || url.indexOf("https://") === 0)
            return "image://artwork/" + encodeURIComponent(url)
        return url
    }

    anchors.left: parent.left
    anchors.right: parent.right
    anchors.bottom: parent.bottom
    anchors.bottomMargin: dp(310)
    height: dataReady ? Math.round((trickplayData.height || 0) * uiScale * 1.4) : 0
    visible: dataReady
    opacity: visible ? 1 : 0
    z: 22

    Behavior on opacity {
        NumberAnimation {
            duration: 100
        }
    }

    Item {
        id: thumbContainer

        readonly property real thumbWidth: trickplayPreview.dataReady ? trickplayPreview.trickplayData.width
                                                                        * trickplayPreview.uiScale * 1.4 : 0
        readonly property real thumbHeight: trickplayPreview.dataReady ? trickplayPreview.trickplayData.height
                                                                         * trickplayPreview.uiScale * 1.4 : 0

        x: Math.max(dp(52), Math.min(parent.width - thumbWidth - dp(52), overlay.positionRatio() * parent.width
                                     - thumbWidth / 2))
        y: 0
        width: thumbWidth
        height: thumbHeight + dp(24)

        Rectangle {
            id: thumbFrame

            width: thumbContainer.thumbWidth
            height: thumbContainer.thumbHeight
            color: "black"
            border.color: overlay.colHairline
            border.width: 1
            radius: dp(8)
            clip: true

            Image {
                source: trickplayPreview.dataReady ? trickplayPreview.artworkSource(trickplayPreview.trickplayData.url) :
                                                     ""
                visible: status === Image.Ready
                x: trickplayPreview.dataReady ? trickplayPreview.trickplayData.offsetX * trickplayPreview.uiScale * 1.4 :
                                                0
                y: trickplayPreview.dataReady ? trickplayPreview.trickplayData.offsetY * trickplayPreview.uiScale * 1.4 :
                                                0
                width: trickplayPreview.dataReady ? trickplayPreview.trickplayData.sheetWidth
                                                    * trickplayPreview.uiScale * 1.4 : 0
                height: trickplayPreview.dataReady ? trickplayPreview.trickplayData.sheetHeight
                                                     * trickplayPreview.uiScale * 1.4 : 0
                fillMode: Image.Stretch
                cache: true
                asynchronous: true
            }
        }

        Text {
            anchors.horizontalCenter: thumbFrame.horizontalCenter
            anchors.top: thumbFrame.bottom
            anchors.topMargin: dp(4)
            text: overlay.formatClock(overlay.scrubbing ? overlay.scrubSeconds : overlay.positionSeconds())
            color: overlay.colTextStrong
            font.pixelSize: dp(16)
            font.weight: Font.Medium
            font.hintingPreference: Font.PreferNoHinting
            renderType: Text.QtRendering
        }
    }
}
