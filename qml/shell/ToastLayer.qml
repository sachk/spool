import QtQuick
import "../theme"
import "../primitives"

Item {
    id: root
    property string message: ""
    property var pending: []

    function show(text) {
        const normalized = String(text || "")
        if (normalized.length === 0)
            return
        if (message.length === 0) {
            message = normalized
            timer.restart()
            return
        }
        const next = pending.slice()
        next.push(normalized)
        while (next.length > 2)
            next.shift()
        pending = next
    }

    function showNext() {
        if (pending.length === 0) {
            message = ""
            return
        }
        const next = pending.slice()
        message = next.shift()
        pending = next
        timer.restart()
    }

    Timer {
        id: timer
        interval: 2400
        onTriggered: root.showNext()
    }

    Surface {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 34
        width: Math.min(parent.width - 96, toastText.implicitWidth + 42)
        height: 48
        visible: root.message.length > 0
        elevated: true
        AppText {
            id: toastText
            anchors.centerIn: parent
            text: root.message
            color: Theme.textPrimary
        }
    }
}
