import QtQuick
import QtQuick.Layouts
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
            MonoText {
                text: "Diagnostics"
                color: Theme.textPrimary
                font.weight: Font.DemiBold
            }
            MonoText {
                text: "CPU  system " + root.cpu(SystemPerformance.systemCpuPercent) + "  app " + root.cpu(
                          SystemPerformance.processCpuPercent)
            }
            MonoText {
                text: "mpv  total " + root.cpu(SystemPerformance.mpvCpuPercent) + "  video decode " + root.cpu(
                          SystemPerformance.videoDecodeCpuPercent)
            }
            MonoText {
                text: "audio  decode " + root.cpu(SystemPerformance.audioDecodeCpuPercent) + "  output " + root.cpu(
                          SystemPerformance.audioOutputCpuPercent)
            }
            MonoText {
                text: "Load  " + SystemPerformance.loadOne.toFixed(2) + "  " + SystemPerformance.loadFive.toFixed(2)
                      + "  " + SystemPerformance.loadFifteen.toFixed(2)
            }
            MonoText {
                text: "App memory  " + root.formatBytes(SystemPerformance.processRssBytes) + " RSS  " + root.formatBytes(
                          SystemPerformance.processAnonymousBytes) + " anon"
            }
            MonoText {
                text: "System memory  " + root.formatBytes(SystemPerformance.systemUsedBytes) + " / " + root.formatBytes(
                          SystemPerformance.systemTotalBytes) + "  free " + root.formatBytes(
                          SystemPerformance.systemAvailableBytes)
            }
            MonoText {
                text: "Input  " + InputLatency.lastLatencyMs.toFixed(2) + " ms  worst " + InputLatency.worstLatencyMs.toFixed(
                          2) + " ms  budget " + InputLatency.frameBudgetMs.toFixed(2) + " ms"
            }
            MonoText {
                text: "Frames  late " + InputLatency.lateCount + "  missed " + InputLatency.missedFrameCount
                      + "  samples " + InputLatency.sampleCount
            }
            MonoText {
                Layout.maximumWidth: Metrics.scaled(396)
                text: "Stage  " + InputLatency.lastStage + (InputLatency.lastRouteSample.length > 0 ? "  ·  "
                                                                                                      + InputLatency.lastRouteSample :
                                                                                                      "")
                elide: Text.ElideRight
            }
            MonoText {
                Layout.maximumWidth: Metrics.scaled(396)
                text: "UI  " + root.width + "x" + root.height + "  " + Metrics.densityForWidth(root.width) + "  "
                      + root.route + (root.focusedItemId.length > 0 ? "  ·  " + root.focusedItemId : "")
                elide: Text.ElideRight
            }
        }
    }
}
