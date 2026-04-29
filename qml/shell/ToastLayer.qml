import QtQuick
import "../theme"
import "../primitives"

Item {
    id: root
    property string message: ""
    function show(text) { message = text; timer.restart() }
    Timer { id: timer; interval: 2400; onTriggered: root.message = "" }

    Surface {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 34
        width: Math.min(parent.width - 96, toastText.implicitWidth + 42)
        height: 48
        visible: root.message.length > 0
        elevated: true
        AppText { id: toastText; anchors.centerIn: parent; text: root.message; color: Theme.textPrimary }
    }
}
