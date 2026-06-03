import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

Rectangle {
    id: audioSyncPanel

    required property var overlay
    readonly property real uiScale: overlay ? overlay.uiScale : 1
    readonly property bool instantOpen: overlay && overlay.isAudioSyncOpen()

    function dp(n) {
        return Math.round(n * uiScale)
    }

    component DelayButton: Rectangle {
        required property var overlay
        property int dir: 1
        readonly property real uiScale: overlay ? overlay.uiScale : 1
        readonly property bool focused: overlay && overlay.audioSyncRow === "delay"

        function dp(n) {
            return Math.round(n * uiScale)
        }

        Layout.preferredWidth: dp(92)
        Layout.preferredHeight: dp(92)
        radius: width / 2
        color: focused ? Qt.alpha(overlay.accent, 0.2) : overlay.colFillSubtle
        border.width: focused ? 2 : 1
        border.color: focused ? overlay.accentBright : overlay.colHairline

        MaterialIcon {
            anchors.centerIn: parent
            name: dir < 0 ? "remove" : "add"
            iconColor: overlay.colTextStrong
            iconSize: dp(42)
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                overlay.audioSyncRow = "delay"
                overlay.adjustAudioDelay(dir)
            }
        }
    }

    anchors.horizontalCenter: parent.horizontalCenter
    anchors.verticalCenter: parent.verticalCenter
    width: Math.min(parent.width - dp(120), dp(760))
    height: dp(330)
    visible: opacity > 0.01
    opacity: 0
    scale: instantOpen || opacity > 0.5 ? 1 : 0.97
    radius: dp(16)
    color: overlay.colPanelBg
    border.width: 1
    border.color: overlay.colHairline

    Behavior on scale {
        enabled: !Theme.reducedMotion && !audioSyncPanel.instantOpen
        NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
    }

    transform: Translate {
        y: audioSyncPanel.instantOpen || audioSyncPanel.opacity > 0.5 ? 0 : audioSyncPanel.dp(14)
        Behavior on y {
            enabled: !Theme.reducedMotion && !audioSyncPanel.instantOpen
            NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: dp(26)
        spacing: dp(18)

        Text {
            Layout.fillWidth: true
            text: "Audio Sync"
            color: overlay.colTextMuted
            font.pixelSize: dp(15)
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 0
            font.hintingPreference: Font.PreferNoHinting
            renderType: Text.QtRendering
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: dp(128)
            spacing: dp(22)

            DelayButton {
                overlay: audioSyncPanel.overlay
                dir: -1
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: dp(10)

                Text {
                    Layout.fillWidth: true
                    text: overlay.formatAudioDelay(overlay.currentAudioDelayMs)
                    color: overlay.colTextStrong
                    font.pixelSize: dp(58)
                    font.weight: Font.Bold
                    font.hintingPreference: Font.PreferNoHinting
                    renderType: Text.QtRendering
                    horizontalAlignment: Text.AlignHCenter
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: dp(26)

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        height: dp(8)
                        radius: height / 2
                        color: overlay.colDelayTrack
                    }

                    Rectangle {
                        anchors.left: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.abs(overlay.currentAudioDelayMs) / 2000 * parent.width / 2
                        height: dp(12)
                        radius: height / 2
                        color: overlay.accent
                        visible: overlay.currentAudioDelayMs >= 0
                    }

                    Rectangle {
                        anchors.right: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.abs(overlay.currentAudioDelayMs) / 2000 * parent.width / 2
                        height: dp(12)
                        radius: height / 2
                        color: overlay.accentPurple
                        visible: overlay.currentAudioDelayMs < 0
                    }

                    Rectangle {
                        x: Math.max(0, Math.min(parent.width - width, ((overlay.currentAudioDelayMs + 2000) / 4000) * parent.width - width / 2))
                        anchors.verticalCenter: parent.verticalCenter
                        width: dp(24)
                        height: width
                        radius: width / 2
                        color: overlay.colTextStrong
                        border.width: 2
                        border.color: overlay.accent
                    }
                }
            }

            DelayButton {
                overlay: audioSyncPanel.overlay
                dir: 1
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: dp(62)
            spacing: dp(10)

            Text {
                text: "Step"
                color: overlay.audioSyncRow === "step" ? overlay.colTextStrong : overlay.colTextDim
                font.pixelSize: dp(22)
                font.weight: Font.DemiBold
                font.hintingPreference: Font.PreferNoHinting
                renderType: Text.QtRendering
                Layout.preferredWidth: dp(88)
                verticalAlignment: Text.AlignVCenter
            }

            Repeater {
                model: overlay.audioSyncSteps.length
                delegate: Rectangle {
                    required property int index
                    readonly property bool selected: overlay.audioSyncStepIndex === index
                    readonly property bool focused: overlay.audioSyncRow === "step" && selected
                    Layout.fillWidth: true
                    Layout.preferredHeight: dp(52)
                    radius: dp(10)
                    color: focused ? Qt.alpha(overlay.accent, 0.2) : selected ? Qt.alpha(overlay.accent, 0.15) : overlay.colFillSubtle
                    border.width: focused ? 2 : selected ? 1 : 1
                    border.color: focused ? overlay.accentBright : selected ? Qt.alpha(overlay.accent, 0.3) : overlay.colHairlineSoft

                    Text {
                        anchors.centerIn: parent
                        text: overlay.audioSyncSteps[index] + " ms"
                        color: selected ? overlay.colTextStrong : overlay.colTextSubtle
                        font.pixelSize: dp(21)
                        font.weight: Font.DemiBold
                        font.hintingPreference: Font.PreferNoHinting
                        renderType: Text.QtRendering
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            overlay.audioSyncRow = "step"
                            overlay.audioSyncStepIndex = index
                        }
                    }
                }
            }
        }
    }
}
