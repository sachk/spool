import QtQuick
import "../theme"
import "../primitives"

Item {
    id: root
    property string message: ""
    property var pending: []
    property string actionText: ""
    property var actionCallback: null

    function clearAction() {
        actionText = ""
        actionCallback = null
    }

    function show(text) {
        const normalized = String(text || "")
        if (normalized.length === 0)
            return
        if (message.length === 0) {
            message = normalized
            clearAction()
            timer.restart()
            return
        }
        const next = pending.slice()
        next.push(normalized)
        while (next.length > 2)
            next.shift()
        pending = next
    }

    function showAction(text, label, callback) {
        const normalized = String(text || "")
        if (normalized.length === 0)
            return
        message = normalized
        actionText = String(label || "")
        actionCallback = callback
        pending = []
        timer.restart()
    }

    function triggerAction() {
        const callback = actionCallback
        message = ""
        clearAction()
        timer.stop()
        if (callback)
            callback()
        showNext()
    }

    function showNext() {
        clearAction()
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
        width: Math.min(parent.width - 96, toastContent.implicitWidth + 42)
        height: 48
        visible: root.message.length > 0
        elevated: true

        Row {
            id: toastContent
            anchors.centerIn: parent
            spacing: Metrics.scaled(16)

            AppText {
                anchors.verticalCenter: parent.verticalCenter
                text: root.message
                color: Theme.textPrimary
            }

            ActionButton {
                visible: root.actionText.length > 0
                anchors.verticalCenter: parent.verticalCenter
                text: root.actionText
                kind: "flat"
                onClicked: root.triggerAction()
            }
        }
    }
}
