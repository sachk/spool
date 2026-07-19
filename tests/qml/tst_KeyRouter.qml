import QtQuick
import QtTest
import "../../qml/shell" as Shell

TestCase {
    id: testCase
    name: "KeyRouter"

    property int routeCalls: 0
    property int backCalls: 0
    property int activateCalls: 0
    property int finishOpeningCalls: 0
    property int typeAheadCalls: 0
    property string typeAheadText: ""
    property bool routeResult: true
    property bool typeAheadResult: true

    property bool lastRouteRepeat: false
    property string lastRoutePhase: ""
    Item {
        // Reproduces AppShell's route property, which previously shadowed the
        // router's unqualified route() helper at runtime.
        property string route: "home"

        Shell.KeyRouter {
            id: keyRouter
            activeTarget: target
        }

        QtObject {
            id: target
            property bool directionRelease: true

            function routeKey(key, phase, repeat) {
                ++testCase.routeCalls
                testCase.lastRouteRepeat = repeat
                testCase.lastRoutePhase = phase
                return testCase.routeResult
            }

            function back() {
                ++testCase.backCalls
                return true
            }

            function activate() {
                ++testCase.activateCalls
            }

            function finishOpeningGesture() {
                ++testCase.finishOpeningCalls
            }

            function typeAhead(text) {
                ++testCase.typeAheadCalls
                testCase.typeAheadText = text
                return testCase.typeAheadResult
            }
        }
    }

    function init() {
        routeCalls = 0
        backCalls = 0
        activateCalls = 0
        finishOpeningCalls = 0
        typeAheadCalls = 0
        typeAheadText = ""
        routeResult = true
        typeAheadResult = true
        lastRouteRepeat = false
        lastRoutePhase = ""
        keyRouter.pressedDirectionKey = 0
        keyRouter.clearAccept()
        keyRouter.backClaimed = false
        keyRouter.typeAheadKey = 0
        keyRouter.textInputActive = false
        keyRouter.backspaceNavigatesInTextInput = false
        keyRouter.webOsColorScanCodes = false
    }

    function test_directionUsesRouterHelper() {
        verify(keyRouter.deliver(target, Qt.Key_Right, "press", false))
        compare(routeCalls, 1)
    }

    function test_unmarkedPressWhilePhysicallyDownBecomesRepeat() {
        verify(keyRouter.routeDirection(Qt.Key_Down, "press", false, Qt.NoModifier))
        compare(lastRouteRepeat, false)
        verify(keyRouter.routeDirection(Qt.Key_Down, "press", false, Qt.NoModifier))
        compare(lastRouteRepeat, true)
        compare(keyRouter.pressedDirectionKey, Qt.Key_Down)
        verify(keyRouter.routeDirection(Qt.Key_Down, "release", false, Qt.NoModifier))
        compare(keyRouter.pressedDirectionKey, 0)
    }

    function test_tenRapidClicksRemainIndependent() {
        for (let click = 0; click < 10; ++click) {
            verify(keyRouter.routeDirection(Qt.Key_Down, "press", false, Qt.NoModifier))
            compare(lastRouteRepeat, false)
            verify(keyRouter.routeDirection(Qt.Key_Down, "release", false, Qt.NoModifier))
            compare(lastRoutePhase, "release")
            compare(keyRouter.pressedDirectionKey, 0)
        }
    }

    function test_backFallsThroughToTarget() {
        routeResult = false
        verify(keyRouter.routeBack("press", false))
        compare(routeCalls, 1)
        compare(backCalls, 1)
        verify(keyRouter.routeBack("release", false))
    }

    function test_releaseOnlyBackFallsThroughToTarget() {
        routeResult = false
        verify(keyRouter.routeBack("release", false))
        compare(routeCalls, 1)
        compare(backCalls, 1)
    }

    function test_backspaceNavigatesOnWebOSWithTextInput() {
        keyRouter.textInputActive = true
        keyRouter.backspaceNavigatesInTextInput = true

        verify(keyRouter.backspaceNavigates())
    }

    function test_backspaceStaysInDesktopTextInput() {
        keyRouter.textInputActive = true

        verify(!keyRouter.backspaceNavigates())
    }

    function test_webOSColorScansNormalizeAsQtColorKeys() {
        keyRouter.webOsColorScanCodes = true
        const scans = [406, 407, 408, 409]
        const keys = [Qt.Key_Red, Qt.Key_Green, Qt.Key_Yellow, Qt.Key_Blue]
        for (let index = 0; index < scans.length; ++index)
            compare(keyRouter.normalizedKey({
                                                "key": 0,
                                                "nativeScanCode": scans[index],
                                                "nativeVirtualKey": 0
                                            }), keys[index])
    }

    function test_acceptWithoutLongPressActivatesOnPress() {
        verify(keyRouter.pressAccept(Qt.Key_Return, false))
        compare(activateCalls, 1)
        verify(keyRouter.releaseAccept(Qt.Key_Return, false))
        compare(activateCalls, 1)
    }

    function test_longPressReleaseFinishesOpeningGesture() {
        verify(keyRouter.pressAccept(Qt.Key_Return, false))
        keyRouter.longPressHandled = true
        verify(keyRouter.releaseAccept(Qt.Key_Return, false))
        compare(activateCalls, 1)
        compare(finishOpeningCalls, 1)
    }

    function test_printableKeyRoutesToTypeAheadWithoutTextInput() {
        const event = {
            "key": Qt.Key_A,
            "text": "a",
            "modifiers": Qt.NoModifier,
            "isAutoRepeat": false,
            "nativeScanCode": 0,
            "nativeVirtualKey": 0
        }
        verify(keyRouter.routeTypeAhead(event, Qt.Key_A, "press"))
        compare(typeAheadCalls, 1)
        compare(typeAheadText, "a")
        compare(keyRouter.typeAheadKey, Qt.Key_A)
        verify(keyRouter.routeTypeAhead(event, Qt.Key_A, "release"))
        compare(keyRouter.typeAheadKey, 0)
        compare(routeCalls, 0)
    }

    function test_typeAheadDoesNotStealModifiedOrTextInputKeys() {
        const event = {
            "key": Qt.Key_A,
            "text": "a",
            "modifiers": Qt.ControlModifier,
            "isAutoRepeat": false,
            "nativeScanCode": 0,
            "nativeVirtualKey": 0
        }
        verify(!keyRouter.routeTypeAhead(event, Qt.Key_A, "press"))
        compare(typeAheadCalls, 0)

        keyRouter.textInputActive = true
        event.modifiers = Qt.NoModifier
        verify(!keyRouter.routeTypeAhead(event, Qt.Key_A, "press"))
        compare(typeAheadCalls, 0)
    }
}
