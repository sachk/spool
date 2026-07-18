import QtQuick
import "../primitives"

FocusScope {
    id: router

    property var activeTarget: null
    property bool textInputActive: false
    property bool backspaceNavigatesInTextInput: false
    property bool webOsColorScanCodes: false
    property var backHandler: null
    property var globalHandler: null
    property int longPressInterval: 520

    property var armedTarget: null
    property int armedKey: 0
    property bool longPressHandled: false
    property bool pressActivated: false
    property bool backClaimed: false
    property int typeAheadKey: 0

    focus: true
    Keys.priority: Keys.BeforeItem

    function backspaceNavigates() {
        return backspaceNavigatesInTextInput || !textInputActive
    }

    function normalizedKey(event) {
        if (webOsColorScanCodes && event.key === 0) {
            const scanCode = Number(event.nativeScanCode || 0)
            if (scanCode === 406)
                return Qt.Key_Red
            if (scanCode === 407)
                return Qt.Key_Green
            if (scanCode === 408)
                return Qt.Key_Yellow
            if (scanCode === 409)
                return Qt.Key_Blue
        }
        if (InputKeys.isIgnoredPlayerNoise(event))
            return 0
        return InputKeys.isBackEvent(event, backspaceNavigates()) ? Qt.Key_Back : event.key
    }

    function deliver(target, key, phase, repeat) {
        return Boolean(target && target.routeKey && target.routeKey(key, phase, repeat))
    }

    function clearAccept() {
        longPressTimer.stop()
        armedTarget = null
        armedKey = 0
        longPressHandled = false
        pressActivated = false
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
        pressActivated = false
        if (target.longPress) {
            longPressTimer.restart()
        } else {
            // Nothing to disambiguate without a long-press gesture: fire on
            // press so activation doesn't wait out the physical key release.
            pressActivated = true
            target.activate()
        }
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
        const activatedOnPress = pressActivated
        clearAccept()
        if (swallowed && activeTarget && activeTarget.finishOpeningGesture)
            activeTarget.finishOpeningGesture()
        else if (!swallowed && !activatedOnPress && target.activate)
            target.activate()
        return true
    }

    function routeBack(phase, repeat) {
        if (phase === "release" && backClaimed)
            return true
        const target = activeTarget
        let claimed = router.deliver(target, Qt.Key_Back, phase, repeat)
        if (!claimed && target && target.back)
            claimed = Boolean(target.back())
        if (!claimed && backHandler)
            claimed = Boolean(backHandler())
        if (phase !== "release")
            backClaimed = claimed
        return claimed
    }

    function routeTypeAhead(event, key, phase) {
        if (textInputActive)
            return false
        if (phase === "release" && typeAheadKey === key) {
            typeAheadKey = 0
            return true
        }
        if (phase !== "press" || !event.text || event.text.length <= 0 || event.modifiers & (Qt.ControlModifier | Qt.AltModifier
                                                                                             | Qt.MetaModifier))
            return false
        if (!activeTarget || !activeTarget.typeAhead || !activeTarget.typeAhead(event.text))
            return false
        typeAheadKey = key
        return true
    }

    function dispatch(event, phase) {
        const key = normalizedKey(event)
        const repeat = Boolean(event.isAutoRepeat)
        if (key === 0)
            return true
        if (InputKeys.isBack(key, false)) {
            const handled = routeBack(phase, repeat)
            if (phase === "release")
                backClaimed = false
            return handled
        }
        if (routeTypeAhead(event, key, phase))
            return true
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
