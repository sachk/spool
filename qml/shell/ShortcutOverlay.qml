import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    signal closed()
    focus: visible
    Keys.onReleased: (event) => { if (InputKeys.isBack(event.key, false, false) || event.key === Qt.Key_Question) { closed(); event.accepted = true } }

    Rectangle { anchors.fill: parent; color: Theme.overlayScrim; MouseArea { anchors.fill: parent; onClicked: root.closed() } }
    Surface {
        anchors.centerIn: parent
        width: Math.min(860, parent.width - 96)
        height: Math.min(700, parent.height - 96)
        elevated: true
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 12
            AppText { text: "Keyboard Shortcuts"; font.pixelSize: Metrics.titlePx(root.width); font.weight: Font.DemiBold }
            Repeater {
                model: [
                    "/  Search",
                    "?  Shortcut overlay",
                    "i  Media info / technical overlay",
                    "m  Context menu",
                    "Esc / Backspace  Back",
                    "Enter  Activate",
                    "Space  Play / pause",
                    "h j k l  Directional navigation",
                    "g g / G  Top / bottom",
                    "Ctrl+D  Diagnostics",
                    "s  Open subtitle menu",
                    "a  Open audio menu",
                    "t  Skip intro / outro",
                    "Hold Down  Cycle subtitle tracks",
                    "Red / Green / Yellow / Blue  Remappable color buttons",
                    "q  Quit player"
                ]
                delegate: MonoText { required property string modelData; Layout.fillWidth: true; text: modelData; font.pixelSize: Metrics.bodyPx(root.width); color: Theme.textSecondary }
            }
        }
    }
}
