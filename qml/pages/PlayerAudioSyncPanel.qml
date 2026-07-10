pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import JellyfinWebOS
import "../theme"
import "../primitives"

OverlayDialog {
    id: root

    required property var overlay
    visible: overlay.audioSyncVisible
    preferredWidth: 760
    z: 45
    onDismissed: overlay.closeAudioSync()

    AppText {
        Layout.fillWidth: true
        text: "Audio sync"
        color: Theme.textPrimary
        font.pixelSize: Metrics.titlePx(root.width)
        font.weight: Font.DemiBold
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 18

        ActionButton {
            text: "−"
            kind: root.overlay.audioSyncRow === "delay" ? "primary" : "secondary"
            onClicked: {
                root.overlay.audioSyncRow = "delay"
                root.overlay.adjustAudioDelay(-1)
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            AppText {
                Layout.fillWidth: true
                text: root.overlay.formatAudioDelay(root.overlay.currentAudioDelayMs)
                color: Theme.textPrimary
                font.pixelSize: Math.round(Metrics.titlePx(root.width) * 1.8)
                font.weight: Font.Bold
                horizontalAlignment: Text.AlignHCenter
            }
            AppText {
                Layout.fillWidth: true
                text: "Negative values advance audio; positive values delay it"
                color: Theme.textMuted
                font.pixelSize: Metrics.metaPx(root.width)
                horizontalAlignment: Text.AlignHCenter
            }
        }

        ActionButton {
            text: "+"
            kind: root.overlay.audioSyncRow === "delay" ? "primary" : "secondary"
            onClicked: {
                root.overlay.audioSyncRow = "delay"
                root.overlay.adjustAudioDelay(1)
            }
        }
    }

    AppText {
        Layout.fillWidth: true
        text: "Step size"
        color: root.overlay.audioSyncRow === "step" ? Theme.textPrimary : Theme.textSecondary
        font.pixelSize: Metrics.bodyPx(root.width)
        font.weight: Font.DemiBold
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10

        Repeater {
            model: root.overlay.audioSyncSteps.length
            delegate: ActionButton {
                required property int index
                Layout.fillWidth: true
                text: root.overlay.audioSyncSteps[index] + " ms"
                kind: root.overlay.audioSyncStepIndex === index ? "primary" : "secondary"
                onClicked: {
                    root.overlay.audioSyncRow = "step"
                    root.overlay.audioSyncStepIndex = index
                }
            }
        }
    }
}
