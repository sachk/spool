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
    // Bends the ramp: 1 keeps the symmetric S-curve, higher values hold the
    // early part of the ramp back and spend the speed later.
    property real rampShape: 1
    property int tickInterval: 16
    property int releaseTimeout: 220
    property int initialReleaseTimeout: 800
    // Holds that begin with a discrete step — a seek nudge, say — must not ramp
    // straight out of that step. Stepping waits for the key to repeat, or for
    // this long, whichever comes first; zero starts moving immediately.
    property int stepDelay: 0
    // Called as (key, steps, source), where source is "press" for the discrete
    // step a gesture opens with and "hold" for the ones the ramp produces.
    property var stepCallback: function (key, steps, source) {}
    property var nowProvider: function () {
        return Date.now()
    }
    readonly property bool active: state !== HoldNavigationController.Idle

    signal holdConfirmed(int key)
    // A physical release, or the watchdog standing in for one that never
    // arrived. Direction changes mid-hold do not end the gesture.
    signal holdReleased(int key)

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
            // Time spent waiting for the repeat is not travelled distance.
            lastTickAt = nowMs()
            holdConfirmed(heldKey)
        }
        releaseWatchdog.restart()
    }

    function endHold() {
        if (!active)
            return
        const key = heldKey
        stopTracking()
        holdReleased(key)
    }

    function rateAt(heldMs) {
        const lowRate = Math.max(0, Number(initialRate))
        const highRate = Math.max(lowRate, Number(maximumRate))
        if (heldMs <= cruiseDuration)
            return lowRate
        if (rampDuration <= 0)
            return highRate
        const progress = Math.min(1, Math.max(0, (heldMs - cruiseDuration) / rampDuration))
        const smoothed = progress * progress * (3 - 2 * progress)
        const eased = rampShape === 1 ? smoothed : Math.pow(smoothed, Math.max(0.05, rampShape))
        return lowRate + (highRate - lowRate) * eased
    }

    function stepsHeldBack(heldMs) {
        return stepDelay > 0 && state !== HoldNavigationController.Repeating && heldMs < stepDelay
    }

    function tick() {
        if (!active)
            return
        const now = nowMs()
        const heldMs = Math.max(0, now - holdStartedAt)
        if (stepsHeldBack(heldMs)) {
            // Time spent behind the gate is not travelled distance.
            lastTickAt = now
            return
        }
        const frameMs = Math.min(50, Math.max(0, now - lastTickAt))
        lastTickAt = now
        accumulator += rateAt(heldMs) * frameMs / 1000
        const steps = Math.floor(accumulator)
        accumulator -= steps
        if (steps > 0)
            stepCallback(heldKey, steps, "hold")
    }

    function routeKey(key, phase, repeat) {
        if (phase === "release") {
            // Wayland auto-repeat emits synthetic release/press pairs. Only a
            // physical release ends tracking.
            if (!repeat && key === heldKey)
                endHold()
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
        stepCallback(key, 1, "press")
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
        onTriggered: root.endHold()
    }
}
