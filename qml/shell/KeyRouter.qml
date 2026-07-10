import QtQuick
import "../primitives"

FocusScope {
    id: router

    property var activeTarget: null
    property bool textInputActive: false
    property var backHandler: null
    property var globalHandler: null
    property int longPressInterval: 520

    property var armedTarget: null
    property int armedKey: 0
    property bool longPressHandled: false
    property bool backClaimed: false

    focus: true
    Keys.priority: Keys.BeforeItem

    function normalizedKey(event) {
        if (InputKeys.isIgnoredPlayerNoise(event))
            return 0
        return InputKeys.isBackEvent(event, !textInputActive) ? Qt.Key_Back : event.key
    }

    function deliver(target, key, phase, repeat) {
        return Boolean(target && target.routeKey && target.routeKey(key, phase, repeat))
    }

    function clearAccept() {
        longPressTimer.stop()
        armedTarget = null
        armedKey = 0
        longPressHandled = false
    }

    function pressAccept(key, repeat) {
        if (repeat || armedTarget)
            return true
        const target = activeTarget
        if (!target || (!target.activate && !target.longPress))
            return router.deliver(target, key, "press", false)
        armedTarget = target
        armedKey = key
        longPressHandled = false
        if (target.longPress)
            longPressTimer.restart()
        return true
    }

    function releaseAccept(key, repeat) {
        if (repeat)
            return true
        const target = armedTarget
        if (!target)
            return router.deliver(activeTarget, key, "release", false)
        longPressTimer.stop()
        const swallowed = longPressHandled
        clearAccept()
        if (!swallowed && target.activate)
            target.activate()
        return true
    }

    function routeBack(phase, repeat) {
        if (phase === "release")
            return backClaimed
        const target = activeTarget
        backClaimed = router.deliver(target, Qt.Key_Back, phase, repeat)
        if (!backClaimed && target && target.back)
            backClaimed = Boolean(target.back())
        if (!backClaimed && backHandler)
            backClaimed = Boolean(backHandler())
        return backClaimed
    }

    function dispatch(event, phase) {
        const key = normalizedKey(event)
        const repeat = Boolean(event.isAutoRepeat)
        if (key === 0)
            return true
        if (InputKeys.isBack(key)) {
            const handled = routeBack(phase, repeat)
            if (phase === "release")
                backClaimed = false
            return handled
        }
        if (textInputActive)
            return false
        if (phase === "release" && InputKeys.isDirection(key)) {
            if (activeTarget && activeTarget.directionRelease)
                router.deliver(activeTarget, key, phase, repeat)
            return true
        }
        if (InputKeys.isAccept(key))
            return phase === "press" ? pressAccept(key, repeat) : releaseAccept(key, repeat)
        const handled = router.deliver(activeTarget, key, phase, repeat)
        return handled || Boolean(globalHandler && globalHandler(key, phase, repeat, event.modifiers))
    }

    Keys.onPressed: event => event.accepted = dispatch(event, "press")
    Keys.onReleased: event => event.accepted = dispatch(event, "release")

    Timer {
        id: longPressTimer
        interval: router.longPressInterval
        repeat: false
        onTriggered: {
            const target = router.armedTarget
            router.longPressHandled = Boolean(target && target.longPress && target.longPress())
        }
    }
}
