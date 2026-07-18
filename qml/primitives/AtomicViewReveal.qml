pragma ComponentBehavior: Bound

import QtQuick

QtObject {
    id: root

    required property var view
    property var latencyMonitor: null
    property bool enabled: true
    property var transitionToken: 0
    property int firstIndex: 0
    property int lastIndex: -1
    property bool delegatesReady: !enabled
    property bool artworkReady: !enabled

    function reset() {
        updateTimer.stop()
        delegatesReady = !enabled
        artworkReady = !enabled
        schedule()
    }

    function schedule() {
        if (enabled && (!delegatesReady || !artworkReady))
            updateTimer.restart()
    }

    function update() {
        if (!enabled || !view)
            return
        view.forceLayout()
        const first = Math.max(0, firstIndex)
        const last = Math.min(view.count - 1, lastIndex)
        if (last < first)
            return
        let delegatesSettled = true
        let artworkSettled = true
        for (let index = first; index <= last; ++index) {
            const delegate = view.itemAtIndex(index)
            if (!delegate) {
                delegatesSettled = false
                artworkSettled = false
                break
            }
            if (delegate.artworkReady === false)
                artworkSettled = false
        }
        if (delegatesSettled && !delegatesReady) {
            delegatesReady = true
            if (latencyMonitor)
                latencyMonitor.mark(transitionToken, "first_delegate")
        }
        if (artworkSettled && !artworkReady) {
            artworkReady = true
            if (latencyMonitor)
                latencyMonitor.mark(transitionToken, "viewport")
        }
        // Delegate creation and asynchronous image status changes are not
        // guaranteed to produce another signal after an early forceLayout(),
        // so keep checking until the state has genuinely settled.
        if (!delegatesReady)
            updateTimer.restart()
    }

    onEnabledChanged: reset()
    onFirstIndexChanged: schedule()
    onLastIndexChanged: schedule()
    Component.onCompleted: reset()

    property Timer updateTimer: Timer {
        interval: 16
        repeat: false
        onTriggered: root.update()
    }
}
