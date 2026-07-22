pragma ComponentBehavior: Bound

import QtQuick

QtObject {
    id: root

    enum State {
        Idle,
        Tracking,
        Repeating
    }

    property int state: HoldNavigationController.Idle
    property int heldKey: 0
    property double holdStartedAt: 0
    property double lastTickAt: 0
    property real accumulator: 0
    property real initialRate: 3
    property real maximumRate: 18
    property int cruiseDuration: 2000
    property int rampDuration: 500
    property int tickInterval: 16
    property int releaseTimeout: 220
    property int initialReleaseTimeout: 800
    property var stepCallback: function (key, steps) {}
    property var nowProvider: function () {
        return Date.now()
    }
    readonly property bool active: state !== HoldNavigationController.Idle

    signal holdConfirmed(int key)

    function nowMs() {
        return Number(nowProvider())
    }

    function stopTracking() {
        tickTimer.stop()
        releaseWatchdog.stop()
        state = HoldNavigationController.Idle
        heldKey = 0
        holdStartedAt = 0
        lastTickAt = 0
        accumulator = 0
    }

    function beginTracking(key) {
        const now = nowMs()
        state = HoldNavigationController.Tracking
        heldKey = key
        holdStartedAt = now
        lastTickAt = now
        accumulator = 0
        tickTimer.start()
        releaseWatchdog.restart()
    }

    function confirmTracking() {
        if (!active)
            return
        if (state !== HoldNavigationController.Repeating) {
            state = HoldNavigationController.Repeating
            holdConfirmed(heldKey)
        }
        releaseWatchdog.restart()
    }

    function rateAt(heldMs) {
        const lowRate = Math.max(0, Number(initialRate))
        const highRate = Math.max(lowRate, Number(maximumRate))
        if (heldMs <= cruiseDuration)
            return lowRate
        if (rampDuration <= 0)
            return highRate
        const progress = Math.min(1, Math.max(0, (heldMs - cruiseDuration) / rampDuration))
        const eased = progress * progress * (3 - 2 * progress)
        return lowRate + (highRate - lowRate) * eased
    }

    function tick() {
        if (!active)
            return
        const now = nowMs()
        const frameMs = Math.min(50, Math.max(0, now - lastTickAt))
        lastTickAt = now
        accumulator += rateAt(Math.max(0, now - holdStartedAt)) * frameMs / 1000
        const steps = Math.floor(accumulator)
        accumulator -= steps
        if (steps > 0)
            stepCallback(heldKey, steps)
    }

    function routeKey(key, phase, repeat) {
        if (phase === "release") {
            // Wayland auto-repeat emits synthetic release/press pairs. Only a
            // physical release ends tracking.
            if (!repeat && key === heldKey)
                stopTracking()
            return true
        }
        if (key === heldKey && active) {
            if (repeat)
                confirmTracking()
            return true
        }
        if (active)
            stopTracking()
        beginTracking(key)
        stepCallback(key, 1)
        if (repeat)
            confirmTracking()
        return true
    }

    property Timer tickTimer: Timer {
        interval: root.tickInterval
        repeat: true
        onTriggered: root.tick()
    }

    property Timer releaseWatchdog: Timer {
        interval: root.state === HoldNavigationController.Repeating ? root.releaseTimeout : root.initialReleaseTimeout
        repeat: false
        onTriggered: root.stopTracking()
    }
}
