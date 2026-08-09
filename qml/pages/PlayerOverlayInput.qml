import QtQuick
import "../primitives"

Item {
    id: root
    visible: false

    required property var overlay
    property int seekKey: 0
    property int seekRepeats: 0
    property int downRepeats: 0
    readonly property bool previewing: seekKey !== 0

    function reset() {
        seekKey = 0
        seekRepeats = 0
        downRepeats = 0
    }

    function seekDelta(key) {
        return key === Qt.Key_Left ? -10 : key === Qt.Key_Right ? 10 : 0
    }

    function repeatedSeekDelta(delta) {
        if (seekRepeats >= 16)
            return delta * 12
        if (seekRepeats >= 8)
            return delta * 6
        if (seekRepeats >= 4)
            return delta * 3
        return delta
    }

    function pressed(key, repeat) {
        const delta = seekDelta(key)
        if (delta !== 0 && overlay.controlsVisible && overlay.focusZone !== "timeline")
            return true
        if (delta !== 0 && overlay.hasPlayer && overlay.canPreviewSeek()) {
            if (!repeat || seekKey !== key) {
                seekKey = key
                seekRepeats = 0
                overlay.seekBy(delta)
            } else {
                ++seekRepeats
                overlay.seekBy(repeatedSeekDelta(delta))
            }
            return true
        }
        if (key === Qt.Key_Down && repeat && overlay.hasPlayer) {
            ++downRepeats
            if (downRepeats === 1 || downRepeats % 4 === 0) {
                overlay.player.cycleSubtitles()
                overlay.showControls("actions")
            }
            return true
        }
        if (!repeat && key !== Qt.Key_Down)
            downRepeats = 0
        return false
    }

    function handleControlsKey(key) {
        if (key === Qt.Key_Up) {
            overlay.focusZone = overlay.focusZone === "actions" ? "timeline" : "back"
            overlay.showControls(overlay.focusZone)
            return true
        }
        if (key === Qt.Key_Down) {
            if (overlay.focusZone === "actions" && overlay.actions[overlay.actionIndex] === "pause") {
                overlay.openAudioSync()
                return true
            }
            if (overlay.focusZone === "timeline")
                overlay.actionIndex = overlay.pauseActionIndex
            overlay.focusZone = overlay.focusZone === "back" ? "timeline" : "actions"
            overlay.showControls(overlay.focusZone)
            return true
        }
        if (key === Qt.Key_Left) {
            if (overlay.focusZone === "timeline") {
                overlay.seekBy(-10)
            } else if (overlay.focusZone === "actions") {
                const moved = overlay.actionIndex > 0
                if (moved)
                    --overlay.actionIndex
                else
                    overlay.focusZone = "back"
            }
            overlay.showControls(overlay.focusZone)
            return true
        }
        if (key === Qt.Key_Right) {
            if (overlay.focusZone === "timeline")
                overlay.seekBy(10)
            else if (overlay.focusZone === "actions")
                overlay.actionIndex = Math.min(overlay.actions.length - 1, overlay.actionIndex + 1)
            overlay.showControls(overlay.focusZone)
            return true
        }
        return false
    }

    function dispatchRemapAction(action) {
        if (!overlay.hasPlayer || !action || action === "none")
            return false
        switch (action) {
        case "togglePause":
            overlay.togglePlayback()
            overlay.showControls("actions")
            break
        case "toggleSubs":
            overlay.player.toggleSubtitles()
            break
        case "cycleSubs":
            overlay.player.cycleSubtitles()
            break
        case "cycleAudio":
            overlay.player.cycleAudio()
            break
        case "skipBack10":
            overlay.seekBy(-10)
            break
        case "skipForward10":
            overlay.seekBy(10)
            break
        case "skipBack30":
            overlay.seekBy(-30)
            break
        case "skipForward30":
            overlay.seekBy(30)
            break
        case "skipBack90":
            overlay.seekBy(-90)
            break
        case "skipForward90":
            overlay.seekBy(90)
            break
        case "skipBackAndEnableSubs":
            overlay.seekBy(-10)
            overlay.player.enableSubtitles()
            break
        case "skipSegment":
            overlay.player.skipActiveSegment()
            break
        case "queuePrevious":
            overlay.playPrevious()
            break
        case "queueNext":
            overlay.playNext()
            break
        case "showInfo":
            overlay.toggleDebugStats()
            break
        case "stop":
            overlay.player.stopWithReason("remap-stop")
            break
        default:
            return false
        }
        return true
    }

    function released(key, repeat) {
        if (key === seekKey) {
            seekKey = 0
            seekRepeats = 0
            overlay.maybeRestartAutohide()
            return true
        }
        if (key === Qt.Key_Down && downRepeats > 0) {
            downRepeats = 0
            return true
        }
        if (overlay.controlsVisible && handleControlsKey(key))
            return true
        if (key === Qt.Key_T && overlay.hasPlayer && overlay.player.activeSegmentType.length > 0) {
            overlay.player.skipActiveSegment()
            return true
        }
        if (InputKeys.isColor(key) && dispatchRemapAction(overlay.colorAction(key)))
            return true
        if ((key === Qt.Key_I || key === Qt.Key_Info) && overlay.hasPlayer) {
            overlay.toggleDebugStats()
            return true
        }
        if (key === Qt.Key_Q && overlay.hasPlayer) {
            overlay.player.stopWithReason("player-q")
            return true
        }
        if (InputKeys.isMediaPrevious(key)) {
            overlay.playPrevious()
            return true
        }
        if (InputKeys.isMediaNext(key)) {
            overlay.playNext()
            return true
        }
        if (InputKeys.isMedia(key) && overlay.hasPlayer) {
            overlay.togglePlayback()
            overlay.showControls("actions")
            return true
        }
        if (!overlay.controlsVisible) {
            overlay.showControls("timeline")
            return true
        }
        if (key === Qt.Key_S) {
            overlay.openMenu("subtitles")
            return true
        }
        if (key === Qt.Key_A) {
            overlay.openMenu("audio")
            return true
        }
        return repeat && InputKeys.isDirection(key)
    }

    function routeKey(key, phase, repeat) {
        return phase === "press" ? pressed(key, repeat) : released(key, repeat)
    }
}
