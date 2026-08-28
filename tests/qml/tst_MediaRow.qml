import QtQuick
import QtTest
import "../../qml/primitives" as Primitives

TestCase {
    id: testCase
    name: "MediaRow"
    width: 640
    height: 240
    when: windowShown

    Primitives.MediaRow {
        id: libraryRow
        width: parent.width
        cardKind: "library"
        cardWidth: 156
        cardGap: 16
        model: [
            {
                "name": "Movies",
                "collectionType": "movies",
                "item": ({})
            },
            {
                "name": "Shows",
                "collectionType": "tvshows",
                "item": ({})
            }
        ]
    }

    function test_libraryCardOmitsCollectionTypeSubtitle() {
        tryCompare(libraryRow, "count", 2)
        tryVerify(function () {
            return libraryRow.currentCard() !== null
        })
        compare(libraryRow.currentCard().titleText(), "Movies")
        compare(libraryRow.currentCard().subtitleText(), "movies")
        compare(libraryRow.currentCard().showSubtitle, false)
        compare(libraryRow.currentCard().emphasizedTitle, true)
    }

    function test_touchPressDoesNotMoveSelectionBeforeClick() {
        libraryRow.currentIndex = 0
        libraryRow.beginPointerSelection(1)
        compare(libraryRow.currentIndex, 0, "a scroll gesture must not briefly select the pressed card")
        compare(libraryRow.commitPointerSelection(), 1)
        compare(libraryRow.currentIndex, 1, "a completed tap should select the pressed card")
    }

    function test_touchOnlyModeHidesFocusRing() {
        libraryRow.keyboardFocusActive = false
        libraryRow.forceActiveFocus()
        tryVerify(function () {
            return libraryRow.currentCard() !== null
        })
        compare(libraryRow.currentCard().focused, false)
        libraryRow.keyboardFocusActive = true
    }
}
