import QtQuick

Item {
    id: root
    visible: false

    required property var overlay

    function restart(delayMs) {
        autohideTimer.interval = delayMs
        autohideTimer.restart()
    }

    function stop() {
        autohideTimer.stop()
    }

    Timer {
        id: autohideTimer
        interval: 3000
        onTriggered: root.overlay.hideControls()
    }
}
