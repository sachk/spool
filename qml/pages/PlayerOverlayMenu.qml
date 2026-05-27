import QtQuick
import QtQuick.Layouts
import "../primitives"

Rectangle {
    id: menuPanel

    required property var overlay
    readonly property real uiScale: overlay ? overlay.uiScale : 1
    readonly property real edgeMargin: dp(20)

    function dp(n) {
        return Math.round(n * uiScale)
    }

    function positionAtTop() {
        menuList.positionViewAtBeginning()
    }

    x: Math.max(edgeMargin, Math.min(parent.width - width - edgeMargin, overlay.menuAnchorX - width / 2))
    y: Math.max(edgeMargin, Math.min(parent.height - height - edgeMargin, overlay.menuAnchorY - height - dp(12)))
    width: Math.min(parent.width - edgeMargin * 2, dp(380))
    height: Math.min(Math.round(parent.height * 0.5), Math.round(menuHeaderBlock.implicitHeight + menuList.contentHeight + dp(30)))
    visible: overlay.isMenuOpen()
    opacity: 0
    radius: dp(16)
    color: overlay.colPanelBg
    border.width: 1
    border.color: overlay.colHairline

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: dp(14)
        spacing: dp(10)

        ColumnLayout {
            id: menuHeaderBlock

            Layout.fillWidth: true
            spacing: dp(10)

            Text {
                Layout.fillWidth: true
                Layout.leftMargin: dp(6)
                text: overlay.mode === "subtitles" ? "Subtitles"
                    : overlay.mode === "audio" ? "Audio"
                    : "Settings"
                color: overlay.colTextMuted
                font.pixelSize: dp(15)
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 0
                font.hintingPreference: Font.PreferNoHinting
                renderType: Text.QtRendering
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: overlay.colHairlineSoft
            }
        }

        ListView {
            id: menuList

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: dp(2)
            model: overlay.mode === "subtitles" && overlay.hasPlayer ? overlay.player.subtitleTracks
                 : overlay.mode === "audio" && overlay.hasPlayer ? overlay.player.audioTracks
                 : overlay.debugOptions
            currentIndex: overlay.menuIndex
            boundsBehavior: Flickable.StopAtBounds
            highlightMoveDuration: 90
            onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)

            delegate: Rectangle {
                required property int index
                required property var modelData

                readonly property bool current: overlay.menuIndex === index
                readonly property bool isSelected: (overlay.mode === "subtitles" && overlay.hasPlayer && overlay.player.selectedSubtitleIndex === index)
                                                || (overlay.mode === "audio" && overlay.hasPlayer && overlay.player.selectedAudioIndex === index)

                width: menuList.width
                height: dp(46)
                radius: dp(10)
                color: current ? Qt.alpha(overlay.accent, 0.18) : "transparent"

                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: dp(4)
                    anchors.verticalCenter: parent.verticalCenter
                    width: dp(3)
                    height: parent.height - dp(16)
                    radius: width / 2
                    color: overlay.accent
                    visible: current
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: dp(16)
                    anchors.rightMargin: dp(14)
                    spacing: dp(10)

                    Text {
                        Layout.fillWidth: true
                        text: String(modelData)
                        color: current ? overlay.colTextStrong : isSelected ? overlay.colSelectedText : overlay.colTextSubtle
                        font.pixelSize: dp(17)
                        font.weight: current || isSelected ? Font.DemiBold : Font.Medium
                        font.hintingPreference: Font.PreferNoHinting
                        renderType: Text.QtRendering
                        elide: Text.ElideRight
                    }

                    MaterialIcon {
                        visible: isSelected
                        name: "check"
                        iconColor: current ? overlay.colTextStrong : overlay.accentBright
                        iconSize: dp(20)
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        overlay.menuIndex = index
                        overlay.activateMenuItem()
                    }
                }
            }
        }
    }
}
