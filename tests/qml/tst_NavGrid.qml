import QtQuick
import QtTest
import "../../qml/primitives" as Primitives

TestCase {
    id: testCase
    name: "NavGrid"
    width: 400
    height: 300
    when: windowShown

    property int modelSize: 100
    property double fakeNow: 1000

    Primitives.NavGrid {
        id: grid
        anchors.fill: parent
        cellWidth: 100
        cellHeight: 100
        model: testCase.modelSize
        holdTickInterval: 10000
        nowProvider: function () {
            return testCase.fakeNow
        }
        delegate: Rectangle {
            required property int index
            width: grid.cellWidth
            height: grid.cellHeight
        }
    }

    SignalSpy {
        id: holdStartedSpy
        target: grid
        signalName: "holdStarted"
    }

    ListModel {
        id: pagedModel
    }

    Primitives.NavGrid {
        id: pagedGrid
        width: 100
        height: 100
        visible: false
        cellWidth: 100
        cellHeight: 100
        model: pagedModel
        holdTickInterval: 10000
        nowProvider: function () {
            return testCase.fakeNow
        }
        delegate: Item {
            required property int index
            width: pagedGrid.cellWidth
            height: pagedGrid.cellHeight
        }
    }

    Item {
        id: alternateFocus
        focus: true
    }

    function arm(target, key, heldMs, frameMs, accumulator) {
        target.heldKey = key
        target.holdStartedAt = fakeNow - heldMs
        target.lastHoldTickAt = fakeNow - frameMs
        target.holdAccumulator = accumulator || 0
        target.holdRepeatSeen = true
    }

    function init() {
        grid.stopAccelerating()
        pagedGrid.stopAccelerating()
        modelSize = 100
        grid.currentIndex = 0
        grid.reducedMotion = false
        grid.acceptUnmarkedHoldRepeats = false
        fakeNow = 1000
        grid.holdTraversalSeconds = 5
        holdStartedSpy.clear()
        pagedModel.clear()
    }

    function test_singlePressMovesOneRowOnly() {
        verify(grid.routeKey(Qt.Key_Down, "press", false))
        compare(grid.currentIndex, 4)
        compare(grid.heldKey, Qt.Key_Down)
        wait(150)
        compare(grid.currentIndex, 4)
        verify(grid.routeKey(Qt.Key_Down, "release", false))
        compare(grid.heldKey, 0)
    }

    function test_singlePressNeverAcceleratesWithoutRepeat() {
        verify(grid.routeKey(Qt.Key_Down, "press", false))
        compare(grid.currentIndex, 4)
        fakeNow += grid.holdDelay + 100
        grid.accelerate()
        compare(grid.currentIndex, 4)
        compare(grid.holdRepeatSeen, false)
        verify(grid.routeKey(Qt.Key_Down, "release", false))
    }

    function test_desktopUnmarkedDuplicateCannotConfirmHold() {
        verify(grid.routeKey(Qt.Key_Down, "press", false))
        compare(grid.currentIndex, 4)
        fakeNow += grid.holdDelay
        verify(grid.routeKey(Qt.Key_Down, "press", false))
        compare(grid.currentIndex, 4)
        compare(grid.holdRepeatSeen, false)
        fakeNow += 100
        grid.accelerate()
        compare(grid.currentIndex, 4)
        verify(grid.routeKey(Qt.Key_Down, "release", false))
    }

    function test_modelSizesKeepFirstPressPrecise_data() {
        return [
                    {
                        "tag": "one",
                        "size": 1,
                        "expected": 0
                    },
                    {
                        "tag": "ten",
                        "size": 10,
                        "expected": 4
                    },
                    {
                        "tag": "two-hundred-fifty",
                        "size": 250,
                        "expected": 4
                    },
                    {
                        "tag": "five-thousand",
                        "size": 5000,
                        "expected": 4
                    }
                ]
    }

    function test_modelSizesKeepFirstPressPrecise(data) {
        modelSize = data.size
        grid.currentIndex = 0
        verify(grid.routeKey(Qt.Key_Down, "press", false))
        compare(grid.currentIndex, data.expected)
    }

    function test_kodiAccelerationCurveScalesWithLibrary() {
        modelSize = 1
        compare(grid.holdMaximumRate, 30)
        modelSize = 10
        compare(grid.holdMaximumRate, 30)
        modelSize = 250
        compare(grid.holdMaximumRate, 250 / 5)
        modelSize = 5000
        compare(grid.holdMaximumRate, 5000 / 5)
        compare(grid.holdInitialRate, 40)
        compare(grid.accelerationRate(grid.holdDelay - 1), 0)
        compare(grid.accelerationRate(grid.holdDelay), grid.holdInitialRate)
        compare(grid.accelerationRate(grid.holdDelay + grid.holdCruiseDuration), grid.holdInitialRate)
        verify(grid.accelerationRate(grid.holdDelay + grid.holdCruiseDuration + 500) > grid.holdInitialRate)
        compare(grid.accelerationRate(grid.holdDelay + grid.holdCruiseDuration + grid.holdRampDuration),
                grid.holdMaximumRate)
    }

    function test_largeLibraryMaximumTraversesInThreePointFiveSeconds() {
        modelSize = 250
        grid.holdTraversalSeconds = 3.5
        compare(grid.holdMaximumRate, 250 / 3.5)
        compare(modelSize / grid.holdMaximumRate, 3.5)
    }

    function test_firstRepeatMovesImmediatelyThenCruises() {
        modelSize = 250
        grid.currentIndex = 0
        verify(grid.routeKey(Qt.Key_Down, "press", false))
        compare(grid.currentIndex, 4)
        fakeNow += 500
        verify(grid.routeKey(Qt.Key_Down, "press", true))
        compare(grid.currentIndex, 8)
        compare(grid.accelerationRate(grid.holdCruiseDuration - 1), grid.holdInitialRate)
        fakeNow += 50
        grid.accelerate()
        compare(grid.currentIndex, 16)
        verify(grid.routeKey(Qt.Key_Down, "release", false))
    }

    function test_webOsStyleUnmarkedRepeatConfirmsHold() {
        modelSize = 250
        grid.currentIndex = 0
        grid.acceptUnmarkedHoldRepeats = true
        verify(grid.routeKey(Qt.Key_Down, "press", false))
        compare(grid.currentIndex, 4)
        fakeNow += 500
        verify(grid.routeKey(Qt.Key_Down, "press", false))
        compare(grid.currentIndex, 8)
        compare(grid.heldKey, Qt.Key_Down)
        compare(grid.holdRepeatSeen, true)
        compare(holdStartedSpy.count, 1)
        fakeNow += 100
        verify(grid.routeKey(Qt.Key_Down, "press", false))
        compare(holdStartedSpy.count, 1)
        verify(grid.routeKey(Qt.Key_Down, "release", false))
    }

    function test_fractionalMovementUsesElapsedTime() {
        modelSize = 250
        grid.currentIndex = 0
        arm(grid, Qt.Key_Down, 3000, 10, 0)
        grid.accelerate()
        compare(grid.currentIndex, 0)
        verify(grid.holdAccumulator > 0)
        fakeNow += 20
        grid.accelerate()
        compare(grid.currentIndex, 4)
    }

    function test_frameStallsClampToFiftyMilliseconds() {
        modelSize = 5000
        grid.currentIndex = 0
        fakeNow = 5000
        arm(grid, Qt.Key_Down, 3000, 1000, 0)
        grid.accelerate()
        compare(grid.currentIndex, 200)
        compare(grid.holdAccumulator, 0)
    }

    function test_directionReversalAndReleaseResetAccumulator() {
        arm(grid, Qt.Key_Down, 800, 20, 0.75)
        verify(grid.routeKey(Qt.Key_Left, "press", true))
        compare(grid.heldKey, Qt.Key_Left)
        compare(grid.holdAccumulator, 0)
        verify(grid.routeKey(Qt.Key_Left, "release", false))
        compare(grid.heldKey, 0)
        compare(grid.holdAccumulator, 0)
    }

    function test_focusZoneChangeStopsAcceleration() {
        grid.forceActiveFocus()
        arm(grid, Qt.Key_Down, 800, 20, 0.75)
        alternateFocus.forceActiveFocus()
        tryCompare(grid, "heldKey", 0)
        compare(grid.holdAccumulator, 0)
    }

    function test_modelReplacementStopsAcceleration() {
        arm(grid, Qt.Key_Down, 800, 20, 0.75)
        modelSize = 10
        tryCompare(grid, "heldKey", 0)
        compare(grid.holdAccumulator, 0)
    }

    function test_paginationAppendContinuesHeldTraversal() {
        for (let index = 0; index < 10; ++index)
            pagedModel.append({
                                  "value": index
                              })
        pagedGrid.currentIndex = 9
        arm(pagedGrid, Qt.Key_Down, 3000, 50, 1)
        pagedGrid.accelerate()
        compare(pagedGrid.currentIndex, 9)
        compare(pagedGrid.heldKey, Qt.Key_Down)
        pagedModel.append({
                              "value": 10
                          })
        fakeNow += 50
        pagedGrid.lastHoldTickAt = fakeNow - 50
        pagedGrid.holdAccumulator = 1
        pagedGrid.accelerate()
        compare(pagedGrid.currentIndex, 10)
    }

    function test_reducedMotionChangesAnimationNotTraversal() {
        modelSize = 100
        grid.currentIndex = 0
        grid.reducedMotion = true
        compare(grid.highlightMoveDuration, 0)
        verify(grid.routeKey(Qt.Key_Down, "press", false))
        compare(grid.currentIndex, 4)
        arm(grid, Qt.Key_Down, 3000, 50, 1)
        grid.accelerate()
        verify(grid.currentIndex > 4)
    }

    function test_accelerationStopsWhenTvOmitsReleaseEvent() {
        grid.forceActiveFocus()
        verify(grid.routeKey(Qt.Key_Down, "press", false))
        verify(grid.routeKey(Qt.Key_Down, "press", true))
        compare(grid.heldKey, Qt.Key_Down)
        wait(grid.holdReleaseTimeout + 50)
        compare(grid.heldKey, 0)
    }
}
