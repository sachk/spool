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
        model: [
            {
                "name": "Movies",
                "collectionType": "movies",
                "item": ({})
            }
        ]
    }

    function test_libraryCardOmitsCollectionTypeSubtitle() {
        tryCompare(libraryRow, "count", 1)
        tryVerify(function () {
            return libraryRow.currentCard() !== null
        })
        compare(libraryRow.currentCard().titleText(), "Movies")
        compare(libraryRow.currentCard().subtitleText(), "movies")
        compare(libraryRow.currentCard().showSubtitle, false)
        compare(libraryRow.currentCard().emphasizedTitle, true)
    }
}
