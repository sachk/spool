import QtQuick
import QtTest
import "../../qml/primitives" as Primitives

TestCase {
    name: "AtomicViewReveal"

    QtObject {
        id: firstDelegate
        property bool artworkReady: true
    }

    QtObject {
        id: secondDelegate
        property bool artworkReady: false
    }

    QtObject {
        id: fakeView
        property int count: 2
        function forceLayout() {}
        function itemAtIndex(index) {
            return index === 0 ? firstDelegate : secondDelegate
        }
    }

    Primitives.AtomicViewReveal {
        id: reveal
        view: fakeView
        firstIndex: 0
        lastIndex: 1
    }

    function init() {
        firstDelegate.artworkReady = true
        secondDelegate.artworkReady = false
        reveal.reset()
        reveal.update()
    }

    function test_waitsForEveryArtwork() {
        compare(reveal.delegatesReady, true)
        compare(reveal.artworkReady, false)

        secondDelegate.artworkReady = true
        reveal.schedule()
        tryCompare(reveal, "artworkReady", true)
    }

    function test_resetClosesTheGateAgain() {
        secondDelegate.artworkReady = true
        reveal.update()
        compare(reveal.artworkReady, true)

        secondDelegate.artworkReady = false
        reveal.reset()
        reveal.update()
        compare(reveal.delegatesReady, true)
        compare(reveal.artworkReady, false)
    }
}
