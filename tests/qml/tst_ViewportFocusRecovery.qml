import QtQuick
import QtTest
import "../../qml/primitives" as Primitives
import "viewport-primitives" as Recovery

TestCase {
    id: testCase
    name: "ViewportFocusRecovery"
    width: 560
    height: 420
    when: windowShown

    Item {
        id: rowViewport
        width: 220
        height: 150
        clip: true

        Rectangle {
            width: parent.width
            height: 40
        }

        ListView {
            id: horizontalCards
            y: 40
            width: 300
            height: 100
            orientation: ListView.Horizontal
            model: 8
            spacing: 10
            delegate: Rectangle {
                required property int index
                width: 90
                height: 100
            }
        }
    }

    Item {
        id: columnViewport
        x: 260
        width: 180
        height: 150
        clip: true

        ListView {
            id: verticalItems
            width: 180
            height: 180
            model: 8
            spacing: 10
            delegate: Rectangle {
                required property int index
                width: 180
                height: 70
            }
        }
    }

    ListView {
        id: emptyView
        width: 100
        height: 100
        model: 0
    }

    ListView {
        id: zeroViewportView
        width: 0
        height: 0
        model: 5
        delegate: Item {
            required property int index
            width: 50
            height: 50
        }
    }

    Primitives.NavGrid {
        id: multiColumnGrid
        x: 450
        width: 90
        height: 120
        cellWidth: 30
        cellHeight: 60
        model: 18
        delegate: Item {
            required property int index
            width: multiColumnGrid.cellWidth
            height: multiColumnGrid.cellHeight
        }
    }

    Primitives.NavGrid {
        id: oneColumnGrid
        x: 450
        y: 150
        width: 60
        height: 120
        cellWidth: 60
        cellHeight: 60
        model: 8
        delegate: Item {
            required property int index
            width: oneColumnGrid.cellWidth
            height: oneColumnGrid.cellHeight
        }
    }

    function init() {
        horizontalCards.contentX = 0
        rowViewport.width = 220
        horizontalCards.currentIndex = 0
        verticalItems.contentY = 0
        multiColumnGrid.contentY = 0
        oneColumnGrid.contentY = 0
        horizontalCards.forceLayout()
        verticalItems.forceLayout()
        multiColumnGrid.forceLayout()
        oneColumnGrid.forceLayout()
        wait(0)
    }

    function test_fullyVisibleLeftDelegateWins() {
        horizontalCards.contentX = 35
        horizontalCards.forceLayout()
        const candidate = Recovery.InputKeys.topLeftVisibleCandidate(horizontalCards, rowViewport)
        verify(candidate !== null)
        compare(candidate.index, 1)
        verify(candidate.fullyVisible)
    }

    function test_fullyVisibleTopDelegateWins() {
        verticalItems.contentY = 25
        verticalItems.forceLayout()
        const candidate = Recovery.InputKeys.topLeftVisibleCandidate(verticalItems, columnViewport)
        verify(candidate !== null)
        compare(candidate.index, 1)
        verify(candidate.fullyVisible)
    }

    function test_mostlyVisibleTopLeftDelegateWins() {
        horizontalCards.contentX = 20
        const candidate = Recovery.InputKeys.topLeftVisibleCandidate(horizontalCards, rowViewport)
        verify(candidate !== null)
        compare(candidate.top, 40)
        compare(candidate.index, 0)
        verify(!candidate.fullyVisible)
        verify(candidate.visibleFraction >= Recovery.InputKeys.focusRecoveryVisibleThreshold)
    }

    function test_partiallyVisibleCandidateIsFallback() {
        rowViewport.width = 80
        horizontalCards.contentX = 35
        horizontalCards.forceLayout()
        const candidate = Recovery.InputKeys.topLeftVisibleCandidate(horizontalCards, rowViewport)
        verify(candidate !== null)
        compare(candidate.index, 0)
        verify(!candidate.fullyVisible)
    }

    function test_candidateOrderingPrefersEnoughVisibilityThenTopLeft() {
        verify(Recovery.InputKeys.earlierVisibleCandidate({
                                                              "top": 100,
                                                              "left": 100,
                                                              "index": 3,
                                                              "fullyVisible": true
                                                          }, {
                                                              "top": 0,
                                                              "left": 0,
                                                              "index": 0,
                                                              "fullyVisible": false
                                                          }))
        verify(Recovery.InputKeys.earlierVisibleCandidate({
                                                              "top": 0,
                                                              "left": 0,
                                                              "index": 0,
                                                              "visibleFraction": 0.8
                                                          }, {
                                                              "top": 100,
                                                              "left": 0,
                                                              "index": 3,
                                                              "fullyVisible": true,
                                                              "visibleFraction": 1
                                                          }))
        verify(Recovery.InputKeys.earlierVisibleCandidate({
                                                              "top": 5,
                                                              "left": 100,
                                                              "index": 9
                                                          }, {
                                                              "top": 6,
                                                              "left": 0,
                                                              "index": 0
                                                          }))
        verify(Recovery.InputKeys.earlierVisibleCandidate({
                                                              "top": 5,
                                                              "left": 20,
                                                              "index": 9
                                                          }, {
                                                              "top": 5,
                                                              "left": 21,
                                                              "index": 0
                                                          }))
        verify(Recovery.InputKeys.earlierVisibleCandidate({
                                                              "top": 5,
                                                              "left": 20,
                                                              "index": 1
                                                          }, {
                                                              "top": 5,
                                                              "left": 20,
                                                              "index": 2
                                                          }))
    }

    function test_emptyAndUninstantiatedViewsHaveNoCandidate() {
        compare(Recovery.InputKeys.topLeftVisibleCandidate(emptyView, emptyView), null)
        compare(Recovery.InputKeys.topLeftVisibleCandidate(zeroViewportView, zeroViewportView), null)
    }

    function test_gridLookupsChooseFirstFullyVisibleRow() {
        multiColumnGrid.contentY = 25
        compare(multiColumnGrid.topLeftVisibleIndex(), 3)
        multiColumnGrid.contentY = 85
        compare(multiColumnGrid.topLeftVisibleIndex(), 6)
        oneColumnGrid.contentY = 25
        compare(oneColumnGrid.topLeftVisibleIndex(), 1)
        oneColumnGrid.contentY = 85
        compare(oneColumnGrid.topLeftVisibleIndex(), 2)
    }

    function test_focusIndexPreservesExactOffsetsAfterEventLoop() {
        horizontalCards.contentX = 35
        horizontalCards.forceLayout()
        const contentX = horizontalCards.contentX
        const contentY = horizontalCards.contentY
        verify(Recovery.InputKeys.focusIndexWithoutScrolling(horizontalCards, 3))
        compare(horizontalCards.currentIndex, 3)
        verify(horizontalCards.activeFocus)
        compare(horizontalCards.contentX, contentX)
        compare(horizontalCards.contentY, contentY)
        wait(0)
        compare(horizontalCards.contentX, contentX)
        compare(horizontalCards.contentY, contentY)
    }
}
