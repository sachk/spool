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
    property var releasedKeys: []

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
        onHoldReleased: key => testCase.releasedKeys.push(key)
        stepDelay: 0
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
        controller.rampShape = 1
        controller.stepDelay = 0
        fakeNow = 1000
        releasedKeys = []
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

    function test_rampShapeHoldsTheEarlyRampBack() {
        controller.rampShape = 2
        // Halfway through the ramp the symmetric curve is at half speed; the
        // bent one is a quarter of the way up and still climbing.
        compare(controller.rateAt(2250), 3 + 15 * 0.25)
        compare(controller.rateAt(2000), 3)
        compare(controller.rateAt(2500), 18)
    }

    function test_stepDelayWaitsForTheKeyToRepeatOrForTheDelay() {
        controller.stepDelay = 300
        controller.cruiseDuration = 100000
        controller.routeKey(Qt.Key_Right, "press", false)
        compare(stepCount, 1)

        advance(200)
        compare(stepCount, 1);

        // The repeat is the fast path; the delay is what stands in for one on
        // a remote that never sends it.
        controller.routeKey(Qt.Key_Right, "press", true)
        advance(1000)
        compare(stepCount, 4)

        controller.routeKey(Qt.Key_Right, "release", false)
        controller.routeKey(Qt.Key_Left, "press", false)
        advance(200)
        compare(stepCount, 5)
        advance(1000)
        compare(stepCount, 7)
    }

    function test_holdReleasedAnnouncesOnlyTheEndOfTheGesture() {
        controller.routeKey(Qt.Key_Right, "press", false)
        controller.routeKey(Qt.Key_Left, "press", false)
        compare(releasedKeys.length, 0)

        controller.routeKey(Qt.Key_Left, "release", true)
        compare(releasedKeys.length, 0)

        controller.routeKey(Qt.Key_Left, "release", false)
        compare(releasedKeys, [Qt.Key_Left])
        compare(controller.state, Primitives.HoldNavigationController.Idle)
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
