import QtQuick
import QtQuick.Layouts
import JellyfinWebOS
import "../theme"
import "../primitives"

Item {
    id: root
    property string route: ""
    property string focusedItemId: ""

    function formatBytes(bytes) {
        const value = Math.max(0, Number(bytes || 0))
        if (value >= 1024 * 1024 * 1024)
            return (value / (1024 * 1024 * 1024)).toFixed(1) + " GiB"
        return Math.round(value / (1024 * 1024)) + " MiB"
    }

    function cpu(value) {
        return Number(value || 0).toFixed(1) + "%"
    }

    function signedMs(value) {
        const number = Number(value || 0)
        return (number > 0 ? "+" : "") + number.toFixed(2) + " ms"
    }

    NumberAnimation on opacity {
        running: root.visible
        from: 0
        to: 1
        duration: 80
    }

    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: Metrics.scaled(18)
        width: Metrics.scaled(420)
        height: diagColumn.implicitHeight + Metrics.scaled(24)
        color: "#E6161616"
        border.width: 1
        border.color: Theme.borderStrong
        radius: Theme.radiusMedium
        ColumnLayout {
            id: diagColumn
            anchors.fill: parent
            anchors.margins: Metrics.scaled(12)
            spacing: Metrics.scaled(4)
            SecondaryText {
                text: "Diagnostics"
                color: Theme.textPrimary
                font.weight: Font.DemiBold
            }
            SecondaryText {
                text: "CPU  system " + root.cpu(SystemPerformance.systemCpuPercent) + "  app " + root.cpu(
                          SystemPerformance.processCpuPercent)
            }
            SecondaryText {
                text: "mpv  total " + root.cpu(SystemPerformance.mpvCpuPercent) + "  video decode " + root.cpu(
                          SystemPerformance.videoDecodeCpuPercent)
            }
            SecondaryText {
                text: "audio  decode " + root.cpu(SystemPerformance.audioDecodeCpuPercent) + "  output " + root.cpu(
                          SystemPerformance.audioOutputCpuPercent)
            }
            SecondaryText {
                text: "Dropped frames  decoder " + Player.decoderDroppedFrames + "  output "
                      + Player.outputDroppedFrames
            }
            SecondaryText {
                text: "Load  " + SystemPerformance.loadOne.toFixed(2) + "  " + SystemPerformance.loadFive.toFixed(2)
                      + "  " + SystemPerformance.loadFifteen.toFixed(2)
            }
            SecondaryText {
                text: "App memory  " + root.formatBytes(SystemPerformance.processRssBytes) + " RSS  " + root.formatBytes(
                          SystemPerformance.processAnonymousBytes) + " anon"
            }
            SecondaryText {
                text: "System memory  " + root.formatBytes(SystemPerformance.systemUsedBytes) + " / " + root.formatBytes(
                          SystemPerformance.systemTotalBytes) + "  free " + root.formatBytes(
                          SystemPerformance.systemAvailableBytes)
            }
            SecondaryText {
                text: "Input  " + InputLatency.lastLatencyMs.toFixed(2) + " ms  worst " + InputLatency.worstLatencyMs.toFixed(
                          2) + " ms  budget " + InputLatency.frameBudgetMs.toFixed(2) + " ms"
            }
            SecondaryText {
                text: "Frames  late " + InputLatency.lateCount + "  missed " + InputLatency.missedFrameCount
                      + "  samples " + InputLatency.sampleCount
            }
            SecondaryText {
                visible: SyncPlay.enabled
                Layout.maximumWidth: Metrics.scaled(396)
                text: "SyncPlay  " + (SyncPlay.groupState || "Unknown") + (SyncPlay.groupStateReason ? " / "
                                                                                                       + SyncPlay.groupStateReason :
                                                                                                       "") + (SyncPlay.waitingForPlayback
                                                                                                              ? "  ·  waiting to play" :
                                                                                                                "")
                elide: Text.ElideRight
            }
            SecondaryText {
                visible: SyncPlay.enabled
                Layout.maximumWidth: Metrics.scaled(396)
                text: "Time sync  " + SyncPlay.timeSyncDevice + "  offset " + root.signedMs(SyncPlay.clockOffsetMs)
                      + "  ping " + Number(SyncPlay.pingMs || 0).toFixed(2) + " ms"
                elide: Text.ElideRight
            }
            SecondaryText {
                visible: SyncPlay.enabled
                Layout.maximumWidth: Metrics.scaled(396)
                text: "Playback drift  " + (SyncPlay.playbackDiffValid ? root.signedMs(SyncPlay.playbackDiffMs) : "—") + "  method "
                      + SyncPlay.syncMethod + "  (speed ≥60 ms; skip ≥3.0 s)"
                elide: Text.ElideRight
            }
            SecondaryText {
                Layout.maximumWidth: Metrics.scaled(396)
                text: "Stage  " + InputLatency.lastStage + (InputLatency.lastRouteSample.length > 0 ? "  ·  "
                                                                                                      + InputLatency.lastRouteSample :
                                                                                                      "")
                elide: Text.ElideRight
            }
            SecondaryText {
                Layout.maximumWidth: Metrics.scaled(396)
                text: "UI  " + root.width + "x" + root.height + "  " + Metrics.densityForWidth(root.width) + "  "
                      + root.route + (root.focusedItemId.length > 0 ? "  ·  " + root.focusedItemId : "")
                elide: Text.ElideRight
            }
        }
    }
}
