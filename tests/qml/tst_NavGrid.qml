import QtQuick
import QtTest
import "../../qml/primitives" as Primitives

TestCase {
    id: testCase
    name: "NavGrid"
    width: 400
    height: 300
    when: windowShown

    Primitives.NavGrid {
        id: grid
        anchors.fill: parent
        cellWidth: 100
        cellHeight: 100
        model: 100
        delegate: Rectangle {
            required property int index
            width: grid.cellWidth
            height: grid.cellHeight
        }
    }

    function init() {
        grid.stopAccelerating()
        grid.currentIndex = 0
    }

    function test_singlePressMovesOneRowOnly() {
        verify(grid.routeKey(Qt.Key_Down, "press", false))
        compare(grid.currentIndex, 4)
        compare(grid.heldKey, 0)
        wait(150)
        compare(grid.currentIndex, 4)
        verify(grid.routeKey(Qt.Key_Down, "release", false))
    }

    function test_autoRepeatArmsKodiStyleAcceleration() {
        verify(grid.routeKey(Qt.Key_Down, "press", false))
        compare(grid.currentIndex, 4)
        verify(grid.routeKey(Qt.Key_Down, "press", true))
        compare(grid.heldKey, Qt.Key_Down)

        grid.holdStartedAt = Date.now() - 500
        grid.lastHoldTickAt = Date.now()
        grid.holdAccumulator = 1
        grid.accelerate()
        verify(grid.currentIndex > 4)

        verify(grid.routeKey(Qt.Key_Down, "release", false))
        compare(grid.heldKey, 0)
    }

    function test_accelerationRampsGraduallyAndRemainsBounded() {
        compare(grid.accelerationRate(grid.holdDelay - 1), 0)
        compare(grid.accelerationRate(grid.holdDelay), grid.holdInitialRate)
        const halfway = grid.accelerationRate(grid.holdDelay + grid.holdRampDuration / 2)
        verify(halfway > grid.holdInitialRate)
        verify(halfway < grid.holdMaximumRate)
        compare(grid.accelerationRate(grid.holdDelay + grid.holdRampDuration), grid.holdMaximumRate)
    }

    function test_accelerationStopsWhenTvOmitsReleaseEvent() {
        verify(grid.routeKey(Qt.Key_Down, "press", false))
        verify(grid.routeKey(Qt.Key_Down, "press", true))
        compare(grid.heldKey, Qt.Key_Down)

        grid.holdStartedAt = Date.now() - 500
        grid.lastHoldTickAt = Date.now()
        grid.holdAccumulator = 1
        grid.accelerate()
        wait(grid.holdReleaseTimeout + 50)
        compare(grid.heldKey, 0)
        const stoppedAt = grid.currentIndex
        wait(100)
        compare(grid.currentIndex, stoppedAt)
    }
}
