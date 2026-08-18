import QtQuick
import QtTest
import "../../qml/pages" as Pages

TestCase {
    id: testCase

    name: "PlayerSeekBarHover"
    when: windowShown
    visible: true
    width: 800
    height: 300

    QtObject {
        id: playerDouble

        property real durationSeconds: 120
        property bool buffering: false
        property string activeSegmentType: ""
        property real activeSegmentEndSeconds: 0
        property var chapters: []
    }

    QtObject {
        id: overlayDouble

        property var player: playerDouble
        property bool hasPlayer: true
        property bool controlsVisible: true
        property string focusZone: "timeline"
        property bool scrubbing: false
        property real scrubSeconds: 0
        property bool timelineHovering: false
        property real timelineHoverSeconds: 0
        property bool controlsShownByHover: false
        property bool autohideRestarted: false

        function dp(value) {
            return value
        }
        function positionSeconds() {
            return 30
        }
        function isControlsActive() {
            return controlsVisible
        }
        function clampSeconds(seconds) {
            return Math.max(0, Math.min(player.durationSeconds, seconds))
        }
        function showControls(zone) {
            controlsVisible = true
            focusZone = zone
            controlsShownByHover = true
        }
        function maybeRestartAutohide() {
            autohideRestarted = true
        }
        function commitScrub() {
            scrubbing = false
            return true
        }
    }

    Pages.PlayerSeekBar {
        id: seekBar

        anchors.centerIn: parent
        width: 600
        height: 100
        overlay: overlayDouble
    }

    function init() {
        overlayDouble.timelineHovering = false
        overlayDouble.timelineHoverSeconds = 0
        overlayDouble.controlsShownByHover = false
        overlayDouble.autohideRestarted = false
        overlayDouble.scrubbing = false
    }

    function test_hoverAboveAndBelowTrackPreviewsPosition() {
        mouseMove(seekBar, 150, 5)
        verify(overlayDouble.timelineHovering)
        // Hover previews a position; it must not pin the controls open.
        verify(!overlayDouble.controlsShownByHover)
        compare(Math.round(overlayDouble.timelineHoverSeconds), 30)

        mouseMove(seekBar, 450, 95)
        verify(overlayDouble.timelineHovering)
        compare(Math.round(overlayDouble.timelineHoverSeconds), 90)
        verify(!overlayDouble.scrubbing)
    }
}
