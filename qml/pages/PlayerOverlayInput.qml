import QtQuick
import "../primitives"

Item {
    id: root
    visible: false

    required property var overlay

    function resetDownHold() {
        downHoldTimer.stop()
        overlay.downHoldActive = false
    }

    function handleControlsKey(key) {
        if (key === Qt.Key_Up) {
            overlay.row = overlay.row === "actions" ? "timeline" : "back"
            overlay.showControls(overlay.row)
            return true
        }
        if (key === Qt.Key_Down) {
            if (overlay.row === "actions" && overlay.actionIndex === 1) {
                overlay.openAudioSync()
                return true
            }
            if (overlay.row === "timeline")
                overlay.actionIndex = 1
            overlay.row = overlay.row === "back" ? "timeline" : "actions"
            overlay.showControls(overlay.row)
            return true
        }
        if (key === Qt.Key_Left) {
            if (overlay.row === "timeline")
                overlay.seekBy(-10)
            else if (overlay.row === "actions")
                overlay.actionIndex = Math.max(0, overlay.actionIndex - 1)
            overlay.showControls(overlay.row)
            return true
        }
        if (key === Qt.Key_Right) {
            if (overlay.row === "timeline")
                overlay.seekBy(10)
            else if (overlay.row === "actions")
                overlay.actionIndex = Math.min(overlay.actions.length - 1, overlay.actionIndex + 1)
            overlay.showControls(overlay.row)
            return true
        }
        if (InputKeys.isAccept(key)) {
            if (overlay.row === "back")
                return overlay.stopPlayback("overlay-back")
            if (overlay.row === "timeline") {
                if (!overlay.commitScrub() && overlay.hasPlayer)
                    overlay.player.togglePause()
            } else {
                overlay.activateAction()
            }
            overlay.showControls(overlay.row)
            return true
        }
        return false
    }

    function handleMenuKey(key) {
        const count = overlay.mode === "subtitles" && overlay.hasPlayer ? overlay.player.subtitleTracks.length :
                                                                          overlay.mode === "audio" && overlay.hasPlayer
                                                                          ? overlay.player.audioTracks.length :
                                                                            overlay.mode === "queue"
                                                                            && overlay.playQueue
                                                                            ? overlay.playQueue.count :
                                                                              overlay.debugOptions.length
        if (key === Qt.Key_Up) {
            overlay.menuIndex = Math.max(0, overlay.menuIndex - 1)
            return true
        }
        if (key === Qt.Key_Down) {
            overlay.menuIndex = Math.min(Math.max(0, count - 1), overlay.menuIndex + 1)
            return true
        }
        if (key === Qt.Key_Left || key === Qt.Key_Right)
            return true
        if (InputKeys.isAccept(key)) {
            overlay.activateMenuItem()
            return true
        }
        return false
    }

    function actionForColorKey(key) {
        if (key === Qt.Key_Red)
            return Settings.redButtonAction
        if (key === Qt.Key_Green)
            return Settings.greenButtonAction
        if (key === Qt.Key_Yellow)
            return Settings.yellowButtonAction
        if (key === Qt.Key_Blue)
            return Settings.blueButtonAction
        return ""
    }

    function dispatchRemapAction(action) {
        if (!overlay.hasPlayer || !action || action === "none")
            return false
        if (action === "togglePause") {
            overlay.player.togglePause()
            overlay.showControls("actions")
            return true
        }
        if (action === "toggleSubs") {
            overlay.player.toggleSubtitles()
            return true
        }
        if (action === "cycleSubs") {
            overlay.player.cycleSubtitles()
            return true
        }
        if (action === "cycleAudio") {
            overlay.player.cycleAudio()
            return true
        }
        if (action === "skipBack10") {
            overlay.seekBy(-10)
            return true
        }
        if (action === "skipForward10") {
            overlay.seekBy(10)
            return true
        }
        if (action === "skipBack30") {
            overlay.seekBy(-30)
            return true
        }
        if (action === "skipForward30") {
            overlay.seekBy(30)
            return true
        }
        if (action === "skipBack90") {
            overlay.seekBy(-90)
            return true
        }
        if (action === "skipForward90") {
            overlay.seekBy(90)
            return true
        }
        if (action === "skipBackAndEnableSubs") {
            overlay.seekBy(-10)
            overlay.player.enableSubtitles()
            return true
        }
        if (action === "skipSegment") {
            overlay.player.skipActiveSegment()
            return true
        }
        if (action === "queuePrevious") {
            overlay.App.playQueuePrevious()
            return true
        }
        if (action === "queueNext") {
            overlay.App.playQueueNext()
            return true
        }
        if (action === "showInfo") {
            overlay.toggleDebugStats()
            return true
        }
        if (action === "stop") {
            overlay.player.stopWithReason("remap-stop")
            return true
        }
        return false
    }

    function handleReleased(event) {
        if (InputKeys.isIgnoredPlayerNoise(event))
            return true
        if (event.key === Qt.Key_Down) {
            downHoldTimer.stop()
            if (overlay.downHoldActive) {
                overlay.downHoldActive = false
                return true
            }
        }
        if (event.key === Qt.Key_T && overlay.hasPlayer && overlay.player.activeSegmentType.length > 0) {
            overlay.player.skipActiveSegment()
            return true
        }
        if (InputKeys.isColor(event.key)) {
            if (dispatchRemapAction(actionForColorKey(event.key)))
                return true
        }
        if (overlay.seekHoldKey !== 0 && event.key === overlay.seekHoldKey) {
            if (!event.isAutoRepeat) {
                if (overlay.seekHoldActive) {
                    overlay.stopPreviewSeekHold()
                } else {
                    const delta = overlay.seekHoldDelta
                    overlay.stopPreviewSeekHold()
                    overlay.seekBy(delta)
                }
            }
            return true
        }
        const releaseSeekDelta = overlay.seekDeltaForKey(event.key)
        if (releaseSeekDelta !== 0 && event.isAutoRepeat)
            return true
        if (InputKeys.isBackEvent(event, !(overlay.shell && overlay.shell.textInputActive)))
            return overlay.handleBack()
        if (event.key === Qt.Key_I || event.key === Qt.Key_Info) {
            overlay.toggleDebugStats()
            return true
        }
        if (event.key === Qt.Key_Q && overlay.hasPlayer) {
            overlay.player.stopWithReason("player-q")
            return true
        }
        if (InputKeys.isMediaPrevious(event.key)) {
            overlay.App.playQueuePrevious()
            return true
        }
        if (InputKeys.isMediaNext(event.key)) {
            overlay.App.playQueueNext()
            return true
        }
        if (overlay.mode === "hidden") {
            if (event.key === Qt.Key_Left) {
                overlay.seekBy(-10)
                return true
            }
            if (event.key === Qt.Key_Right) {
                overlay.seekBy(10)
                return true
            }
            if (InputKeys.isAccept(event.key) && overlay.hasPlayer) {
                overlay.actionIndex = 1
                overlay.player.togglePause()
                overlay.showControls("actions")
                return true
            }
            overlay.showControls("timeline")
            return true
        }
        if (overlay.isAudioSyncOpen() && overlay.handleAudioSyncKey(event.key))
            return true
        if (overlay.isMenuOpen() && handleMenuKey(event.key))
            return true
        if (handleControlsKey(event.key))
            return true
        if (event.key === Qt.Key_S) {
            overlay.openSubtitles()
            return true
        }
        if (event.key === Qt.Key_A) {
            overlay.openAudio()
            return true
        }
        return false
    }

    function handlePressed(event) {
        if (InputKeys.isIgnoredPlayerNoise(event))
            return true
        if (event.key !== Qt.Key_Down && downHoldTimer.running)
            resetDownHold()
        if (event.key === Qt.Key_Left)
            return overlay.startPreviewSeekHold(event.key, -10)
        if (event.key === Qt.Key_Right)
            return overlay.startPreviewSeekHold(event.key, 10)
        if (event.key === Qt.Key_Down) {
            if (!event.isAutoRepeat) {
                overlay.downHoldActive = false
                downHoldTimer.interval = 450
                downHoldTimer.restart()
            }
            return false
        }
        return false
    }

    Timer {
        id: downHoldTimer
        interval: 450
        repeat: false
        onTriggered: {
            if (!overlay.hasPlayer) {
                stop()
                return
            }
            overlay.downHoldActive = true
            overlay.player.cycleSubtitles()
            overlay.showControls("actions")
            interval = 400
            restart()
        }
    }
}
