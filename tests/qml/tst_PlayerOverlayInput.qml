import QtQuick
import QtTest
import "../../qml/pages" as Pages

TestCase {
    id: testCase
    name: "PlayerOverlayInput"

    property var seekDeltas: []
    property int subtitleCycles: 0
    property int controlsShown: 0

    QtObject {
        id: playerStub
        property string activeSegmentType: ""
        function cycleSubtitles() {
            ++testCase.subtitleCycles
        }
    }

    QtObject {
        id: overlayStub
        property bool hasPlayer: true
        property bool controlsVisible: true
        property string focusZone: "timeline"
        property int actionIndex: 1
        property var actions: ["back", "pause", "forward"]
        property var player: playerStub

        function canPreviewSeek() {
            return true
        }
        function seekBy(delta) {
            testCase.seekDeltas.push(delta)
        }
        function showControls(zone) {
            focusZone = zone
            ++testCase.controlsShown
        }
        function maybeRestartAutohide() {
        }
    }

    Pages.PlayerOverlayInput {
        id: input
        overlay: overlayStub
    }

    function init() {
        seekDeltas = []
        subtitleCycles = 0
        controlsShown = 0
        input.reset()
        overlayStub.controlsVisible = true
        overlayStub.focusZone = "timeline"
        overlayStub.actionIndex = 1
    }

    function test_shortSeekUsesOneRouterGesture() {
        verify(input.pressed(Qt.Key_Left, false))
        compare(seekDeltas, [-10])
        verify(input.previewing)
        verify(input.released(Qt.Key_Left, false))
        verify(!input.previewing)
    }

    function test_actionRowNavigatesWithoutSeeking() {
        overlayStub.focusZone = "actions"
        verify(input.pressed(Qt.Key_Right, false))
        compare(seekDeltas, [])
        verify(input.released(Qt.Key_Right, false))
        compare(overlayStub.actionIndex, 2)
    }

    function test_repeatedSeekAccelerates() {
        verify(input.pressed(Qt.Key_Right, false))
        for (let repeat = 0; repeat < 16; ++repeat)
            verify(input.pressed(Qt.Key_Right, true))
        compare(seekDeltas.length, 17)
        compare(seekDeltas[3], 10)
        compare(seekDeltas[4], 30)
        compare(seekDeltas[seekDeltas.length - 1], 120)
        verify(input.released(Qt.Key_Right, false))
    }

    function test_downRepeatCyclesAtBoundedRate() {
        for (let repeat = 0; repeat < 4; ++repeat)
            verify(input.pressed(Qt.Key_Down, true))
        compare(subtitleCycles, 2)
        compare(controlsShown, 2)
        verify(input.released(Qt.Key_Down, false))
        compare(input.downRepeats, 0)
    }
}
