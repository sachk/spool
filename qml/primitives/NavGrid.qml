import QtQuick

GridView {
    id: root

    property var shell
    property int fallbackColumns: 1
    signal edgeUp
    signal accepted(int index)
    signal holdStarted(int key)
    readonly property bool directionRelease: true
    property int heldKey: 0
    property double holdStartedAt: 0
    property double lastHoldTickAt: 0
    property real holdAccumulator: 0
    property int holdReleaseTimeout: 220
    property int holdInitialReleaseTimeout: 800
    property bool holdRepeatSeen: false
    property int holdDelay: 500
    property int holdCruiseDuration: 700
    property int holdRampDuration: 1700
    property real holdInitialRate: 18
    property real holdTraversalSeconds: 5
    readonly property real holdMaximumRate: Math.max(30, count / Math.max(1, holdTraversalSeconds))
    property int holdTickInterval: 16
    property bool reducedMotion: false
    // Keep the initial traversal readable, then shorten the highlight glide
    // as held navigation accelerates.
    property int singleStepDurationMs: 55
    property int stepDurationMs: singleStepDurationMs
    property var nowProvider: function () {
        return Date.now()
    }

    focus: true
    keyNavigationEnabled: false
    highlightMoveDuration: reducedMotion ? 0 : stepDurationMs

    onActiveFocusChanged: if (!activeFocus)
                              stopAccelerating()
    onModelChanged: stopAccelerating()

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
        holdRepeatSeen = false
        holdAccumulator = 0
        stepDurationMs = singleStepDurationMs
    }

    function nowMs() {
        return Number(nowProvider())
    }

    function beginAccelerating(key) {
        heldKey = key
        holdStartedAt = nowMs()
        lastHoldTickAt = holdStartedAt
        holdRepeatSeen = false
        holdAccumulator = 0
        holdTimer.start()
        holdReleaseWatchdog.restart()
    }

    function confirmHold(moveImmediately) {
        if (!holdRepeatSeen) {
            holdRepeatSeen = true
            lastHoldTickAt = nowMs()
            if (moveImmediately)
                moveBy(heldDelta())
            holdStarted(heldKey)
        }
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
        const rampElapsed = heldMs - holdDelay - holdCruiseDuration
        if (rampElapsed <= 0)
            return holdInitialRate
        const progress = Math.min(1, rampElapsed / holdRampDuration)
        const eased = progress * progress * (3 - 2 * progress)
        return holdInitialRate + (holdMaximumRate - holdInitialRate) * eased
    }

    function accelerate() {
        if (!heldKey || !holdRepeatSeen || count <= 0)
            return
        const now = nowMs()
        const heldMs = Math.max(0, now - holdStartedAt)
        const frameMs = Math.min(50, Math.max(0, now - lastHoldTickAt))
        lastHoldTickAt = now
        const rate = accelerationRate(heldMs)
        if (rate <= 0)
            return
        stepDurationMs = Math.max(24, Math.min(singleStepDurationMs, Math.round(700 / rate)))
        holdAccumulator += rate * frameMs / 1000
        let moves = Math.floor(holdAccumulator)
        holdAccumulator -= moves
        while (moves-- > 0) {
            const before = currentIndex
            moveBy(heldDelta())
            if (currentIndex === before) {
                holdAccumulator = 0
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
        if (!directional)
            return false
        if (key === heldKey) {
            // Some webOS compositors send repeated presses without setting
            // QKeyEvent::isAutoRepeat. A second press for the armed direction
            // is sufficient to confirm the hold on every platform.
            confirmHold(true)
            return true
        }
        if (heldKey)
            stopAccelerating()
        if (repeat) {
            // Preserve acceleration if the first event observed by this view
            // is already marked as an auto-repeat.
            beginAccelerating(key)
            moveBy(heldDelta())
            confirmHold(false)
            return true
        }
        const columns = columnCount()
        const before = currentIndex
        if (key === Qt.Key_Left) {
            if (currentIndex % columns === 0)
                return true
            moveBy(-1)
        } else if (key === Qt.Key_Right) {
            moveBy(1)
        } else if (key === Qt.Key_Up) {
            if (currentIndex < columns) {
                edgeUp()
                return true
            }
            moveBy(-columns)
        } else {
            moveBy(columns)
        }
        if (currentIndex !== before)
            beginAccelerating(key)
        return true
    }

    function activate() {
        if (currentIndex >= 0)
            accepted(currentIndex)
    }

    Connections {
        target: root.model && typeof root.model === "object" ? root.model : null
        ignoreUnknownSignals: true

        function onModelReset() {
            root.stopAccelerating()
        }
    }

    Timer {
        id: holdTimer
        interval: root.holdTickInterval
        repeat: true
        onTriggered: root.accelerate()
    }

    Timer {
        id: holdReleaseWatchdog
        interval: root.holdRepeatSeen ? root.holdReleaseTimeout : root.holdInitialReleaseTimeout
        repeat: false
        onTriggered: root.stopAccelerating()
    }
}
