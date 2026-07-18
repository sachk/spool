import QtQuick

GridView {
    id: root

    property var shell
    property int fallbackColumns: 1
    signal edgeUp
    signal accepted(int index)
    readonly property bool directionRelease: true
    property int heldKey: 0
    property int holdStartedAt: 0
    property int lastHoldTickAt: 0
    property real holdAccumulator: 0
    property int holdReleaseTimeout: 150
    property int holdDelay: 320
    property int holdRampDuration: 5000
    property real holdInitialRate: 2.5
    property real holdMaximumRate: 8

    focus: true
    keyNavigationEnabled: false

    function columnCount() {
        return Math.max(1, Math.floor(width / Math.max(1, cellWidth || fallbackColumns)))
    }

    function moveBy(delta) {
        if (count <= 0)
            return true
        currentIndex = Math.max(0, Math.min(count - 1, currentIndex + delta))
        return true
    }

    function stopAccelerating() {
        holdTimer.stop()
        holdReleaseWatchdog.stop()
        heldKey = 0
        holdAccumulator = 0
    }

    function beginAccelerating(key) {
        heldKey = key
        holdStartedAt = Date.now()
        lastHoldTickAt = holdStartedAt
        holdAccumulator = 0
        holdTimer.start()
        holdReleaseWatchdog.restart()
    }

    function heldDelta() {
        const columns = columnCount()
        if (heldKey === Qt.Key_Left)
            return -1
        if (heldKey === Qt.Key_Right)
            return 1
        if (heldKey === Qt.Key_Up)
            return -columns
        return columns
    }

    function accelerationRate(heldMs) {
        if (heldMs < holdDelay)
            return 0
        const progress = Math.min(1, Math.max(0, (heldMs - holdDelay) / holdRampDuration))
        const eased = progress * progress * (3 - 2 * progress)
        return holdInitialRate + (holdMaximumRate - holdInitialRate) * eased
    }

    function accelerate() {
        if (!heldKey || count <= 0)
            return
        const now = Date.now()
        const heldMs = now - holdStartedAt
        const frameMs = Math.min(50, Math.max(0, now - lastHoldTickAt))
        lastHoldTickAt = now
        const rate = accelerationRate(heldMs)
        if (rate <= 0)
            return
        holdAccumulator += rate * frameMs / 1000
        let moves = Math.min(2, Math.floor(holdAccumulator))
        holdAccumulator -= moves
        while (moves-- > 0) {
            const before = currentIndex
            moveBy(heldDelta())
            if (currentIndex === before) {
                stopAccelerating()
                break
            }
        }
    }

    function routeKey(key, phase, repeat) {
        if (phase === "release") {
            if (key === heldKey)
                stopAccelerating()
            return true
        }
        const directional = key === Qt.Key_Left || key === Qt.Key_Right || key === Qt.Key_Up || key === Qt.Key_Down
        if (repeat && directional) {
            if (key !== heldKey)
                beginAccelerating(key)
            else
                holdReleaseWatchdog.restart()
            return true
        }
        if (directional && heldKey)
            stopAccelerating()
        const columns = columnCount()
        if (key === Qt.Key_Left) {
            if (currentIndex % columns === 0)
                return true
            const handled = moveBy(-1)
            return handled
        }
        if (key === Qt.Key_Right) {
            const handled = moveBy(1)
            return handled
        }
        if (key === Qt.Key_Up) {
            if (currentIndex < columns) {
                edgeUp()
                return true
            }
            const handled = moveBy(-columns)
            return handled
        }
        if (key === Qt.Key_Down) {
            const handled = moveBy(columns)
            return handled
        }
        return false
    }

    function activate() {
        if (currentIndex >= 0)
            accepted(currentIndex)
    }

    Timer {
        id: holdTimer
        interval: 16
        repeat: true
        onTriggered: root.accelerate()
    }

    Timer {
        id: holdReleaseWatchdog
        interval: root.holdReleaseTimeout
        repeat: false
        onTriggered: root.stopAccelerating()
    }
}
