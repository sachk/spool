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
    property bool routeResult: true

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

            function routeKey(key, phase, repeat) {
                ++testCase.routeCalls
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
        }
    }

    function init() {
        routeCalls = 0
        backCalls = 0
        activateCalls = 0
        finishOpeningCalls = 0
        routeResult = true
        keyRouter.clearAccept()
        keyRouter.backClaimed = false
        keyRouter.textInputActive = false
        keyRouter.backspaceNavigatesInTextInput = false
        keyRouter.webOsColorScanCodes = false
    }

    function test_directionUsesRouterHelper() {
        verify(keyRouter.deliver(target, Qt.Key_Right, "press", false))
        compare(routeCalls, 1)
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

    function test_acceptActivatesOnRelease() {
        verify(keyRouter.pressAccept(Qt.Key_Return, false))
        compare(activateCalls, 0)
        verify(keyRouter.releaseAccept(Qt.Key_Return, false))
        compare(activateCalls, 1)
    }

    function test_longPressReleaseFinishesOpeningGesture() {
        verify(keyRouter.pressAccept(Qt.Key_Return, false))
        keyRouter.longPressHandled = true
        verify(keyRouter.releaseAccept(Qt.Key_Return, false))
        compare(activateCalls, 0)
        compare(finishOpeningCalls, 1)
    }
}
