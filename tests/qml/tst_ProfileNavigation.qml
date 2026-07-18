import QtQuick
import QtTest
import "../../qml/pages/ProfileNavigation.js" as ProfileNavigation

TestCase {
    name: "ProfileNavigation"

    function test_movesAcrossProfilesThenAddTile() {
        let state = ProfileNavigation.move(0, 3, false, 1)
        compare(state.profileIndex, 1)
        compare(state.addFocused, false)

        state = ProfileNavigation.move(state.profileIndex, 3, state.addFocused, 1)
        compare(state.profileIndex, 2)
        compare(state.addFocused, false)

        state = ProfileNavigation.move(state.profileIndex, 3, state.addFocused, 1)
        compare(state.profileIndex, 2)
        compare(state.addFocused, true)
    }

    function test_leftFromAddReturnsToLastProfile() {
        const state = ProfileNavigation.move(2, 3, true, -1)
        compare(state.profileIndex, 2)
        compare(state.addFocused, false)
    }

    function test_focusRemainsBounded() {
        let state = ProfileNavigation.move(0, 3, false, -1)
        compare(state.profileIndex, 0)
        compare(state.addFocused, false)

        state = ProfileNavigation.move(-1, 0, false, 1)
        compare(state.profileIndex, -1)
        compare(state.addFocused, true)
    }
}
