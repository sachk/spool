import QtQuick
import QtTest
import "../../qml/primitives" as Primitives

TestCase {
    id: testCase
    name: "HoldNavigationController"

    property double fakeNow: 1000
    property int callbackCount: 0
    property int stepCount: 0
    property var callbackKeys: []

    Primitives.HoldNavigationController {
        id: controller
        tickInterval: 10000
        nowProvider: function () {
            return testCase.fakeNow
        }
        stepCallback: function (key, steps) {
            ++testCase.callbackCount
            testCase.stepCount += steps
            testCase.callbackKeys.push(key)
        }
    }

    function advance(milliseconds, frameMilliseconds) {
        const frame = frameMilliseconds || 16
        const end = fakeNow + milliseconds
        while (fakeNow < end) {
            fakeNow = Math.min(end, fakeNow + frame)
            controller.tick()
        }
    }

    function init() {
        controller.stopTracking()
        controller.initialRate = 3
        controller.maximumRate = 18
        controller.cruiseDuration = 2000
        controller.rampDuration = 500
        fakeNow = 1000
        callbackCount = 0
        stepCount = 0
        callbackKeys = []
    }

    function test_rateHasReadableCruiseThenShortRamp() {
        compare(controller.rateAt(0), 3)
        compare(controller.rateAt(1999), 3)
        compare(controller.rateAt(2000), 3)
        compare(controller.rateAt(2250), 10.5)
        compare(controller.rateAt(2500), 18)
        compare(controller.rateAt(5000), 18)
    }

    function test_pressMovesImmediatelyThenCruisesAtThreePerSecond() {
        verify(controller.routeKey(Qt.Key_Right, "press", false))
        compare(controller.state, Primitives.HoldNavigationController.Tracking)
        compare(stepCount, 1)

        advance(1000)
        compare(stepCount, 4)
        compare(callbackCount, 4)
    }

    function test_repeatConfirmationDoesNotAddAnExtraStep() {
        controller.routeKey(Qt.Key_Right, "press", false)
        controller.routeKey(Qt.Key_Right, "press", true)
        compare(controller.state, Primitives.HoldNavigationController.Repeating)
        compare(stepCount, 1)
    }

    function test_syntheticReleasePreservesHoldAndPhysicalReleaseStopsIt() {
        controller.routeKey(Qt.Key_Right, "press", false)
        controller.routeKey(Qt.Key_Right, "press", true)
        controller.routeKey(Qt.Key_Right, "release", true)
        compare(controller.state, Primitives.HoldNavigationController.Repeating)

        controller.routeKey(Qt.Key_Right, "release", false)
        compare(controller.state, Primitives.HoldNavigationController.Idle)
        const stoppedAt = stepCount
        advance(1000)
        compare(stepCount, stoppedAt)
    }

    function test_directionChangeResetsFractionAndMovesImmediately() {
        controller.routeKey(Qt.Key_Right, "press", false)
        advance(200)
        verify(controller.accumulator > 0)

        controller.routeKey(Qt.Key_Left, "press", false)
        compare(controller.state, Primitives.HoldNavigationController.Tracking)
        compare(controller.heldKey, Qt.Key_Left)
        compare(controller.accumulator, 0)
        compare(stepCount, 2)
        compare(callbackKeys[callbackKeys.length - 1], Qt.Key_Left)
    }

    function test_fastTickCoalescesAllStepsIntoOneCallback() {
        controller.initialRate = 100
        controller.maximumRate = 100
        controller.routeKey(Qt.Key_Right, "press", false)
        callbackCount = 0
        stepCount = 0

        advance(50, 50)
        compare(callbackCount, 1)
        compare(stepCount, 5)
    }
}
