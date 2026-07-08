import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

Item {
    id: root
    property string route: ""
    property string focusedItemId: ""
    property int frames: 0
    property int fps: 0

    Timer {
        interval: 1000
        running: root.visible
        repeat: true
        onTriggered: {
            root.fps = root.frames
            root.frames = 0
        }
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
                text: "FPS: " + root.fps
            }
            MonoText {
                text: "Render loop: threaded/basic"
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
                text: "Image cache: Qt network cache"
            }
            MonoText {
                text: "Last API: unavailable"
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
            MonoText {
                text: "QSG backend: Qt Quick"
            }
        }
    }
}
