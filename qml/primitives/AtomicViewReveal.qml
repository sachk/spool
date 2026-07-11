pragma ComponentBehavior: Bound

import QtQuick

QtObject {
    id: root

    required property var view
    property bool enabled: true
    property int firstIndex: 0
    property int lastIndex: -1
    property string logName: ""
    property double startedAtMs: 0
    property int delegatesElapsedMs: -1
    property bool delegatesReady: !enabled
    property bool artworkReady: !enabled

    function reset() {
        updateTimer.stop()
        startedAtMs = Date.now()
        delegatesElapsedMs = -1
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
        const elapsedMs = Math.max(0, Math.round(Date.now() - startedAtMs))
        if (delegatesSettled && !delegatesReady) {
            delegatesReady = true
            delegatesElapsedMs = elapsedMs
        }
        if (artworkSettled && !artworkReady) {
            artworkReady = true
            if (logName.length > 0)
                console.info("library viewport: render complete", logName, "range=" + first + "-" + last, "items=" + (
                                 last - first + 1), "delegatesMs=" + delegatesElapsedMs, "artworkMs=" + elapsedMs)
        }
    }

    onEnabledChanged: reset()
    onFirstIndexChanged: schedule()
    onLastIndexChanged: schedule()
    Component.onCompleted: reset()

    property Timer updateTimer: Timer {
        interval: 0
        repeat: false
        onTriggered: root.update()
    }
}
