import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

Item {
    id: root
    property string route: ""
    property string focusedItemId: ""
    NumberAnimation on opacity {
        running: root.visible
        from: 0
        to: 1
        duration: 80
    }

    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 18
        width: 390
        height: diagColumn.implicitHeight + 24
        color: "#E6161616"
        border.width: 1
        border.color: Theme.borderStrong
        radius: Theme.radiusMedium
        ColumnLayout {
            id: diagColumn
            anchors.fill: parent
            anchors.margins: 12
            spacing: 5
            MonoText {
                text: "Diagnostics"
                color: Theme.textPrimary
                font.weight: Font.DemiBold
            }
            MonoText {
                text: "Input samples: " + InputLatency.sampleCount
            }
            MonoText {
                text: "Late samples: " + InputLatency.lateCount
            }
            MonoText {
                text: "Missed frames: " + InputLatency.missedFrameCount
            }
            MonoText {
                text: "Last latency: " + InputLatency.lastLatencyMs.toFixed(2) + " ms"
            }
            MonoText {
                text: "Worst latency: " + InputLatency.worstLatencyMs.toFixed(2) + " ms"
            }
            MonoText {
                text: "Frame budget: " + InputLatency.frameBudgetMs.toFixed(2) + " ms"
            }
            MonoText {
                text: "Last stage: " + InputLatency.lastStage
            }
            MonoText {
                text: "Screen: " + root.width + "x" + root.height
            }
            MonoText {
                text: "Density: " + Metrics.densityForWidth(root.width)
            }
            MonoText {
                text: "Grid columns: " + Metrics.columns(root.width)
            }
            MonoText {
                text: "Route: " + root.route
            }
            MonoText {
                text: "Focused item: " + root.focusedItemId
            }
            MonoText {
                text: "Text render: " + (Theme.normalTextRenderType === Text.CurveRendering ? "CurveRendering" :
                                                                                              "QtRendering")
            }
        }
    }
}
