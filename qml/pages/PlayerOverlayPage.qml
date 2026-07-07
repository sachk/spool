import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import JellyfinWebOS
import "../theme"
import "../primitives"

FocusScope {
    id: overlay
    focus: false

    property var shell
    readonly property var player: appController ? appController.player : null
    readonly property var playQueue: appController ? appController.playQueue : null
    readonly property bool hasPlayer: player !== null && player !== undefined
    readonly property bool smartTvPlatform: nativeWindow ? nativeWindow.smartTvPlatform : true
    readonly property bool desktopControlsAvailable: !smartTvPlatform
    readonly property int currentAudioDelayMs: appController ? appController.settings.audioDelayMs : 0
    readonly property bool nightModeEnabled: appController ? appController.settings.nightModeEnabled : false
    property bool mediaInfoVisible: false
    property bool diagnosticsVisible: false

    property string mode: "hidden"       // hidden, controls, subtitles, audio, debug, audiosync
    property string row: "timeline"      // back, timeline, actions
    property int actionIndex: 1
    property int menuIndex: 0
    property string audioSyncRow: "delay"
    property int audioSyncStepIndex: 2
    property bool scrubbing: false
    property double scrubSeconds: 0
    property int seekHoldKey: 0
    property double seekHoldDelta: 0
    property double seekHoldElapsedMs: 0
    property double seekHoldStartMs: 0
    property bool seekHoldActive: false
    property bool seekHoldFirstRepeat: true
    property bool downHoldActive: false
    property bool previewBurstActive: false
    readonly property real uiScale: Math.max(0.78, Math.min(1.0, height / 1440))
    readonly property color accent: Theme.accent
    readonly property color accentBright: Qt.lighter(Theme.accent, 1.35)
    readonly property color accentPurple: Theme.accentPurple
    readonly property color colText: "#F4F8FA"
    readonly property color colTextDim: "#B8C4CA"
    readonly property color colTextMuted: "#8FA0A9"
    readonly property color colTextSubtle: "#BCC6CB"
    readonly property color colTextStrong: "#FFFFFF"
    readonly property color colIconDim: "#EEEEEE"
    readonly property color colStatus: "#AAB7BF"
    readonly property color colSelectedText: "#EAF2F6"
    readonly property color colError: "#FFB8BD"
    readonly property color colPanelBg: "#F0121519"
    readonly property color colHairline: "#26FFFFFF"
    readonly property color colHairlineSoft: "#1AFFFFFF"
    readonly property color colFillSubtle: "#14FFFFFF"
    readonly property color colTrackOuter: "#5C0B1117"
    readonly property color colTrackInner: "#66364249"
    readonly property color colBackdropSoft: "#30000000"
    readonly property color colDelayTrack: "#55606A72"
    readonly property int actionTargetSize: dp(64)
    property real menuAnchorX: width - dp(240)
    property real menuAnchorY: height - dp(170)

    function dp(n) {
        return Math.round(n * uiScale)
    }

    // Only offer the audio-track button when there's an actual choice to make.
    readonly property bool audioSelectable: hasPlayer && player.audioTracks.length > 1
    readonly property var actions: {
        const list = [
            { label: "Rewind", value: "back" },
            { label: hasPlayer && player.paused ? "Play" : "Pause", value: "pause" },
            { label: "Fast forward", value: "forward" }
        ]
        if (playQueue && playQueue.canGoPrevious)
            list.push({ label: "Previous item", value: "prevQueue" })
        if (playQueue && playQueue.canGoNext)
            list.push({ label: "Next item", value: "nextQueue" })
        if (hasPlayer && player.hasChapters) {
            list.push({ label: "Previous chapter", value: "prevChapter" })
            list.push({ label: "Next chapter", value: "nextChapter" })
        }
        list.push({ label: "Subtitles", value: "subtitles" })
        if (audioSelectable)
            list.push({ label: "Audio", value: "audio" })
        if (desktopControlsAvailable)
            list.push({ label: nativeWindow.fullScreen ? "Exit full screen" : "Full screen", value: "fullscreen" })
        list.push({ label: "Settings", value: "debug" })
        return list
    }
    readonly property var audioSyncSteps: [1, 5, 10, 100]
    readonly property var debugOptions: [
        "Audio sync",
        hasPlayer && player.debugOsdVisible ? "Hide debug stats" : "Show debug stats",
        nightModeEnabled ? "Disable night mode" : "Enable night mode",
        "Stop playback"
    ]
    function isMenuOpen() {
        return mode === "subtitles" || mode === "audio" || mode === "debug"
    }

    function isAudioSyncOpen() {
        return mode === "audiosync"
    }

    function isPinned() {
        return scrubbing || isMenuOpen() || isAudioSyncOpen()
    }

    function isControlsActive() {
        return mode === "controls" || isAudioSyncOpen()
    }

    function autohideDelayMs() {
        return row === "actions" ? 5200 : 3000
    }

    function restartAutohide() {
        autohideTimer.interval = autohideDelayMs()
        autohideTimer.restart()
    }

    function formatClock(seconds) {
        const total = Math.max(0, Math.floor(seconds || 0))
        const hours = Math.floor(total / 3600)
        const minutes = Math.floor((total % 3600) / 60)
        const secs = total % 60
        return hours > 0 ? hours + ":" + String(minutes).padStart(2, "0") + ":" + String(secs).padStart(2, "0") : minutes + ":" + String(secs).padStart(2, "0")
    }

    function formatAudioDelay(value) {
        const ms = clampAudioDelayMs(value)
        if (ms > 0) return "+" + ms + " ms"
        return ms + " ms"
    }

    function actionIcon(value) {
        if (value === "back") return "fast_rewind"
        if (value === "pause") return hasPlayer && player.paused ? "play_arrow" : "pause"
        if (value === "forward") return "fast_forward"
        if (value === "prevChapter") return "skip_previous"
        if (value === "nextChapter") return "skip_next"
        if (value === "subtitles") return "closed_caption"
        if (value === "audio") return "audiotrack"
        if (value === "fullscreen") return nativeWindow.fullScreen ? "fullscreen_exit" : "fullscreen"
        return "settings"
    }

    function actionCenterX(actionValue) {
        const idx = actions.findIndex(action => action.value === actionValue)
        if (idx < 0 || actionRow.width <= 0)
            return width - dp(240)
        return hud.x + actionRow.x + (idx * (actionTargetSize + actionRow.spacing)) + actionTargetSize / 2
    }

    function setMenuAnchor(actionValue) {
        menuAnchorX = actionCenterX(actionValue)
        menuAnchorY = hud.y + actionRow.y
    }

    function clampSeconds(seconds) {
        const value = Math.max(0, seconds || 0)
        const duration = hasPlayer ? player.durationSeconds || 0 : 0
        return duration > 0 ? Math.min(duration, value) : value
    }

    function positionSeconds() {
        if (scrubbing) return scrubSeconds
        return hasPlayer ? player.positionSeconds : 0
    }

    function positionRatio() {
        if (!hasPlayer || player.durationSeconds <= 0) return 0
        return Math.max(0, Math.min(1, positionSeconds() / player.durationSeconds))
    }

    function showControls(preferredRow) {
        if (mode === "hidden")
            row = preferredRow || "timeline"
        if (!isMenuOpen() && !isAudioSyncOpen())
            mode = "controls"
        if (isPinned()) autohideTimer.stop()
        else restartAutohide()
    }

    function maybeRestartAutohide() {
        if (mode !== "hidden" && !isPinned())
            restartAutohide()
    }

    function hideControls() {
        if (isPinned()) {
            if (scrubbing)
                autohideTimer.restart()
            return false
        }
        cancelHeldNavigation()
        autohideTimer.stop()
        mode = "hidden"
        row = "timeline"
        return true
    }

    function seekTo(seconds) {
        if (hasPlayer)
            player.seek(clampSeconds(seconds))
    }

    function adjustTimeline(delta) {
        showControls("timeline")
        row = "timeline"
        scrubbing = true
        scrubSeconds = clampSeconds(positionSeconds() + delta)
        scrubTimer.restart()
    }

    function seekBy(delta) {
        showControls("timeline")
        row = "timeline"
        scrubTimer.stop()
        scrubbing = false
        if (hasPlayer) {
            player.previewSeekBy(delta)
            previewBurstActive = true
            previewBurstTimer.restart()
        }
    }

    function seekHoldStepSeconds() {
        const direction = seekHoldDelta < 0 ? -1 : 1
        if (seekHoldElapsedMs >= 2600)
            return direction * 120
        if (seekHoldElapsedMs >= 1500)
            return direction * 60
        if (seekHoldElapsedMs >= 800)
            return direction * 30
        return direction * 10
    }

    function seekHoldIntervalMs() {
        const elapsed = Math.max(0, Math.min(1000, seekHoldElapsedMs))
        return Math.round(80 - elapsed * 0.035)
    }

    function scheduleSeekHoldTick() {
        seekHoldTimer.interval = seekHoldFirstRepeat ? 360 : seekHoldIntervalMs()
        seekHoldTimer.restart()
    }

    function canPreviewSeekFromCurrentMode() {
        return mode === "hidden" || (mode === "controls" && row === "timeline")
    }

    function seekDeltaForKey(key) {
        if (key === Qt.Key_Left)
            return -10
        if (key === Qt.Key_Right)
            return 10
        return 0
    }

    function startPreviewSeekHold(key, delta) {
        if (!hasPlayer || !canPreviewSeekFromCurrentMode())
            return false
        if (seekHoldKey === key) {
            if (!seekHoldTimer.running)
                scheduleSeekHoldTick()
            return true
        }
        stopPreviewSeekHold()
        seekHoldKey = key
        seekHoldDelta = delta
        seekHoldStartMs = Date.now()
        seekHoldElapsedMs = 0
        seekHoldActive = false
        seekHoldFirstRepeat = true
        scheduleSeekHoldTick()
        return true
    }

    function stopPreviewSeekHold() {
        seekHoldTimer.stop()
        seekHoldKey = 0
        seekHoldDelta = 0
        seekHoldElapsedMs = 0
        seekHoldStartMs = 0
        seekHoldActive = false
        seekHoldFirstRepeat = true
    }

    function commitScrub() {
        if (!scrubbing)
            return false
        scrubTimer.stop()
        seekTo(scrubSeconds)
        scrubbing = false
        previewBurstActive = false
        previewBurstTimer.stop()
        maybeRestartAutohide()
        return true
    }

    function cancelHeldNavigation() {
        downHoldTimer.stop()
        downHoldActive = false
        previewBurstTimer.stop()
        previewBurstActive = false
        stopPreviewSeekHold()
    }

    function closeMenu() {
        mode = "controls"
        menuIndex = 0
        showControls(row)
    }

    function openSubtitles() {
        setMenuAnchor("subtitles")
        mode = "subtitles"
        menuIndex = 0
        menuPanel.positionAtTop()
        autohideTimer.stop()
    }

    function openAudio() {
        setMenuAnchor("audio")
        mode = "audio"
        menuIndex = hasPlayer ? Math.max(0, player.selectedAudioIndex) : 0
        menuPanel.positionAtTop()
        autohideTimer.stop()
    }

    function openDebugMenu() {
        setMenuAnchor("debug")
        mode = "debug"
        menuIndex = 0
        menuPanel.positionAtTop()
        autohideTimer.stop()
    }

    function openAudioSync() {
        mode = "audiosync"
        audioSyncRow = "delay"
        autohideTimer.stop()
    }

    function wheelVolumeDelta(event) {
        const rawSteps = event.angleDelta.y !== 0 ? event.angleDelta.y / 120
                       : event.pixelDelta.y !== 0 ? event.pixelDelta.y / 80
                       : 0
        if (rawSteps === 0)
            return 0
        const roundedSteps = Math.round(rawSteps)
        const steps = roundedSteps !== 0 ? roundedSteps : (rawSteps > 0 ? 1 : -1)
        return steps * 5
    }

    function adjustVolumeFromWheel(event) {
        if (!desktopControlsAvailable || !hasPlayer)
            return
        const delta = wheelVolumeDelta(event)
        if (delta === 0)
            return
        player.adjustVolume(delta)
        row = "actions"
        showControls("actions")
        event.accepted = true
    }

    function activateAction() {
        const action = actions[Math.max(0, Math.min(actions.length - 1, actionIndex))].value
        if (!hasPlayer)
            return
        if (action === "back") player.seekBack()
        else if (action === "pause") player.togglePause()
        else if (action === "forward") player.seekForward()
        else if (action === "prevQueue") appController.playQueuePrevious()
        else if (action === "nextQueue") appController.playQueueNext()
        else if (action === "prevChapter") player.previousChapter()
        else if (action === "nextChapter") player.nextChapter()
        else if (action === "subtitles") openSubtitles()
        else if (action === "audio") openAudio()
        else if (action === "fullscreen" && nativeWindow) nativeWindow.toggleFullScreen()
        else if (action === "debug") openDebugMenu()
    }

    function activateMenuItem() {
        if (mode === "subtitles") {
            if (!hasPlayer || player.subtitleTracks.length === 0)
                return
            if (hasPlayer) player.selectSubtitle(menuIndex)
            closeMenu()
            return
        }
        if (mode === "audio") {
            if (!hasPlayer || player.audioTracks.length === 0)
                return
            if (hasPlayer) player.selectAudio(menuIndex)
            closeMenu()
            return
        }
        if (menuIndex === 0) { openAudioSync(); return }
        else if (menuIndex === 1) toggleDebugStats()
        else if (menuIndex === 2 && appController) appController.settings.setNightModeEnabled(!nightModeEnabled)
        else if (menuIndex === 3 && hasPlayer) player.stopWithReason("debug-menu-stop")
        if (mode === "debug") closeMenu()
    }

    function toggleDebugStats() {
        if (!hasPlayer)
            return
        const showing = !player.debugOsdVisible
        player.toggleDebugOsd()
        if (showing) {
            autohideTimer.stop()
            mode = "hidden"
            row = "timeline"
        } else {
            showControls(row)
        }
    }

    function clampAudioDelayMs(value) {
        return Math.max(-2000, Math.min(2000, Math.round(value || 0)))
    }

    function setAudioDelayMs(value) {
        if (appController)
            appController.settings.setAudioDelayMs(clampAudioDelayMs(value))
    }

    function adjustAudioDelay(direction) {
        const step = audioSyncSteps[Math.max(0, Math.min(audioSyncSteps.length - 1, audioSyncStepIndex))]
        setAudioDelayMs(currentAudioDelayMs + direction * step)
    }

    function handleAudioSyncKey(key) {
        if (key === Qt.Key_Up) {
            if (audioSyncRow === "step") audioSyncRow = "delay"
            else {
                mode = "controls"
                row = "actions"
                actionIndex = 1
                showControls(row)
            }
            return true
        }
        if (key === Qt.Key_Down) {
            if (audioSyncRow === "delay") audioSyncRow = "step"
            else {
                mode = "controls"
                row = "actions"
                actionIndex = 1
                showControls(row)
            }
            return true
        }
        if (key === Qt.Key_Left) {
            if (audioSyncRow === "delay") adjustAudioDelay(-1)
            else audioSyncStepIndex = Math.max(0, audioSyncStepIndex - 1)
            return true
        }
        if (key === Qt.Key_Right) {
            if (audioSyncRow === "delay") adjustAudioDelay(1)
            else audioSyncStepIndex = Math.min(audioSyncSteps.length - 1, audioSyncStepIndex + 1)
            return true
        }
        if (InputKeys.isAccept(key)) {
            if (audioSyncRow === "delay" && hasPlayer) player.togglePause()
            return true
        }
        return false
    }

    function stopPlayback(reason) {
        if (hasPlayer && player.backAllowed)
            player.stopWithReason(reason)
        return true
    }

    // Progressive back: peel back the most-nested layer first, only exit
    // playback once nothing is left to dismiss. The on-screen back button
    // (mouse click or row="back" activation) is an explicit exit gesture
    // and should call stopPlayback() directly instead.
    function handleBack() {
        cancelHeldNavigation()
        if (scrubbing) {
            scrubTimer.stop()
            scrubbing = false
            autohideTimer.stop()
            mode = "hidden"
            row = "timeline"
            return true
        }
        if (isAudioSyncOpen()) { mode = "controls"; showControls("actions"); return true }
        if (isMenuOpen()) { closeMenu(); return true }
        if (mode !== "hidden") {
            autohideTimer.stop()
            mode = "hidden"
            row = "timeline"
            return true
        }
        return stopPlayback("overlay-back")
    }

    function handleControlsKey(key) {
        if (key === Qt.Key_Up) {
            row = row === "actions" ? "timeline" : "back"
            showControls(row)
            return true
        }
        if (key === Qt.Key_Down) {
            if (row === "actions" && actionIndex === 1) {
                openAudioSync()
                return true
            }
            if (row === "timeline")
                actionIndex = 1
            row = row === "back" ? "timeline" : "actions"
            showControls(row)
            return true
        }
        if (key === Qt.Key_Left) {
            if (row === "timeline") seekBy(-10)
            else if (row === "actions") actionIndex = Math.max(0, actionIndex - 1)
            showControls(row)
            return true
        }
        if (key === Qt.Key_Right) {
            if (row === "timeline") seekBy(10)
            else if (row === "actions") actionIndex = Math.min(actions.length - 1, actionIndex + 1)
            showControls(row)
            return true
        }
        if (InputKeys.isAccept(key)) {
            if (row === "back") {
                // Explicit exit — don't reshow controls on the way out.
                return stopPlayback("overlay-back")
            }
            if (row === "timeline") {
                if (!commitScrub() && hasPlayer) player.togglePause()
            } else {
                activateAction()
            }
            showControls(row)
            return true
        }
        return false
    }

    function handleMenuKey(key) {
        const count = mode === "subtitles" && hasPlayer ? player.subtitleTracks.length
                    : mode === "audio" && hasPlayer ? player.audioTracks.length
                    : debugOptions.length
        if (key === Qt.Key_Up) { menuIndex = Math.max(0, menuIndex - 1); return true }
        if (key === Qt.Key_Down) { menuIndex = Math.min(Math.max(0, count - 1), menuIndex + 1); return true }
        if (key === Qt.Key_Left || key === Qt.Key_Right) return true
        if (InputKeys.isAccept(key)) { activateMenuItem(); return true }
        return false
    }

    function actionForColorKey(key) {
        if (!appController)
            return ""
        if (key === Qt.Key_Red)    return appController.settings.redButtonAction
        if (key === Qt.Key_Green)  return appController.settings.greenButtonAction
        if (key === Qt.Key_Yellow) return appController.settings.yellowButtonAction
        if (key === Qt.Key_Blue)   return appController.settings.blueButtonAction
        return ""
    }

    function dispatchRemapAction(action) {
        if (!hasPlayer || !action || action === "none")
            return false
        if (action === "togglePause") { player.togglePause(); showControls("actions"); return true }
        if (action === "toggleSubs") { player.toggleSubtitles(); return true }
        if (action === "cycleSubs") { player.cycleSubtitles(); return true }
        if (action === "cycleAudio") { player.cycleAudio(); return true }
        if (action === "skipBack10") { seekBy(-10); return true }
        if (action === "skipForward10") { seekBy(10); return true }
        if (action === "skipBack30") { seekBy(-30); return true }
        if (action === "skipForward30") { seekBy(30); return true }
        if (action === "skipBack90") { seekBy(-90); return true }
        if (action === "skipForward90") { seekBy(90); return true }
        if (action === "skipBackAndEnableSubs") { seekBy(-10); player.enableSubtitles(); return true }
        if (action === "skipSegment") { player.skipActiveSegment(); return true }
        if (action === "queuePrevious") { appController.playQueuePrevious(); return true }
        if (action === "queueNext") { appController.playQueueNext(); return true }
        if (action === "showInfo") { toggleDebugStats(); return true }
        if (action === "stop") { player.stopWithReason("remap-stop"); return true }
        return false
    }

    function handleReleased(event) {
        if (InputKeys.isIgnoredPlayerNoise(event)) return true
        if (event.key === Qt.Key_Down) {
            downHoldTimer.stop()
            if (downHoldActive) {
                // Hold consumed the release — don't ALSO move row down.
                downHoldActive = false
                return true
            }
        }
        if (event.key === Qt.Key_T && hasPlayer && player.activeSegmentType.length > 0) {
            player.skipActiveSegment()
            return true
        }
        if (InputKeys.isColor(event.key)) {
            if (dispatchRemapAction(actionForColorKey(event.key))) return true
        }
        if (seekHoldKey !== 0 && event.key === seekHoldKey) {
            if (!event.isAutoRepeat) {
                if (seekHoldActive) {
                    stopPreviewSeekHold()
                } else {
                    const delta = seekHoldDelta
                    stopPreviewSeekHold()
                    seekBy(delta)
                }
            }
            return true
        }
        const releaseSeekDelta = seekDeltaForKey(event.key)
        if (releaseSeekDelta !== 0 && event.isAutoRepeat)
            return true
        if (InputKeys.isBackEvent(event, !(shell && shell.textInputActive))) return handleBack()
        if (event.key === Qt.Key_I || event.key === Qt.Key_Info) { toggleDebugStats(); return true }
        if (event.key === Qt.Key_Q && hasPlayer) { player.stopWithReason("player-q"); return true }
        if (InputKeys.isMediaPrevious(event.key)) { appController.playQueuePrevious(); return true }
        if (InputKeys.isMediaNext(event.key)) { appController.playQueueNext(); return true }
        if (mode === "hidden") {
            if (event.key === Qt.Key_Left) { seekBy(-10); return true }
            if (event.key === Qt.Key_Right) { seekBy(10); return true }
            if (InputKeys.isAccept(event.key) && hasPlayer) { actionIndex = 1; player.togglePause(); showControls("actions"); return true }
            showControls("timeline")
            return true
        }
        if (isAudioSyncOpen() && handleAudioSyncKey(event.key)) return true
        if (isMenuOpen() && handleMenuKey(event.key)) return true
        if (handleControlsKey(event.key)) return true
        if (event.key === Qt.Key_S) { openSubtitles(); return true }
        if (event.key === Qt.Key_A) { openAudio(); return true }
        return false
    }

    function handlePressed(event) {
        if (InputKeys.isIgnoredPlayerNoise(event)) return true
        if (event.key !== Qt.Key_Down && downHoldTimer.running) {
            downHoldTimer.stop()
            downHoldActive = false
        }
        if (event.key === Qt.Key_Left)
            return startPreviewSeekHold(event.key, -10)
        if (event.key === Qt.Key_Right)
            return startPreviewSeekHold(event.key, 10)
        if (event.key === Qt.Key_Down) {
            if (!event.isAutoRepeat) {
                downHoldActive = false
                downHoldTimer.interval = 450
                downHoldTimer.restart()
            }
            return false  // let the normal release-handler decide what Down means
        }
        return false
    }

    onVisibleChanged: {
        if (visible) {
            mode = "controls"
            row = "timeline"
            actionIndex = 1
            showControls("timeline")
        } else {
            autohideTimer.stop()
            scrubTimer.stop()
            previewBurstTimer.stop()
            stopPreviewSeekHold()
            downHoldTimer.stop()
            downHoldActive = false
            previewBurstActive = false
            mode = "hidden"
            row = "timeline"
            scrubbing = false
            audioSyncRow = "delay"
        }
    }

    onScrubbingChanged: if (!scrubbing) maybeRestartAutohide()

    Timer { id: autohideTimer; interval: 3000; onTriggered: overlay.hideControls() }
    Timer { id: scrubTimer; interval: 650; onTriggered: overlay.commitScrub() }
    Timer {
        id: previewBurstTimer
        interval: 1300
        repeat: false
        onTriggered: overlay.previewBurstActive = false
    }
    // Long-press of Down cycles subtitles (off -> first -> ... -> off). The
    // first 450 ms is the "hold detection" window; after that we keep firing
    // at ~400 ms so the user can flick through tracks quickly.
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
    Timer {
        id: seekHoldTimer
        interval: 200
        repeat: false
        onTriggered: {
            if (overlay.seekHoldKey === 0) {
                stop()
                return
            }
            overlay.seekHoldElapsedMs = Math.max(0, Date.now() - overlay.seekHoldStartMs)
            if (!overlay.seekHoldActive)
                overlay.seekHoldActive = true
            overlay.seekHoldFirstRepeat = false
            overlay.seekBy(overlay.seekHoldStepSeconds())
            overlay.scheduleSeekHoldTick()
        }
    }

    Item {
        id: trickplayPreloadPool
        visible: false
        Repeater {
            model: overlay.visible && overlay.hasPlayer ? overlay.player.trickplaySheetUrls : []
            delegate: Image {
                required property string modelData
                source: modelData
                asynchronous: true
                cache: true
            }
        }
    }

    // Embedded video surface (desktop / non-Starfish builds). On Starfish the
    // video lives on a separate exported surface so this item is harmless —
    // MpvVideoItem just sits unused. z=-1 keeps it behind the HUD.
    MpvVideoItem {
        anchors.fill: parent
        z: -1
    }

    TapHandler { onTapped: overlay.showControls("timeline") }
    HoverHandler { onHoveredChanged: if (hovered && overlay.mode !== "hidden") overlay.showControls(overlay.row) }
    WheelHandler {
        target: null
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: (event) => overlay.adjustVolumeFromWheel(event)
    }

    states: [
        State { name: "hidden"; when: overlay.mode === "hidden"; PropertyChanges { target: hud; opacity: 0 } },
        State { name: "controls"; when: overlay.mode === "controls"; PropertyChanges { target: hud; opacity: 1 } PropertyChanges { target: backButton; opacity: 1 } PropertyChanges { target: topScrim; opacity: 1 } PropertyChanges { target: bottomScrim; opacity: 1 } },
        State {
            name: "subtitles"
            when: overlay.mode === "subtitles"
            PropertyChanges { target: hud; opacity: 1 }
            PropertyChanges { target: backButton; opacity: 1 }
            PropertyChanges { target: topScrim; opacity: 1 }
            PropertyChanges { target: bottomScrim; opacity: 1 }
            PropertyChanges { target: menuPanel; opacity: 1 }
        },
        State {
            name: "audio"
            when: overlay.mode === "audio"
            PropertyChanges { target: hud; opacity: 1 }
            PropertyChanges { target: backButton; opacity: 1 }
            PropertyChanges { target: topScrim; opacity: 1 }
            PropertyChanges { target: bottomScrim; opacity: 1 }
            PropertyChanges { target: menuPanel; opacity: 1 }
        },
        State {
            name: "debug"
            when: overlay.mode === "debug"
            PropertyChanges { target: hud; opacity: 1 }
            PropertyChanges { target: backButton; opacity: 1 }
            PropertyChanges { target: topScrim; opacity: 1 }
            PropertyChanges { target: bottomScrim; opacity: 1 }
            PropertyChanges { target: menuPanel; opacity: 1 }
        },
        State {
            name: "audiosync"
            when: overlay.isAudioSyncOpen()
            PropertyChanges { target: hud; opacity: 1 }
            PropertyChanges { target: backButton; opacity: 1 }
            PropertyChanges { target: topScrim; opacity: 0 }
            PropertyChanges { target: bottomScrim; opacity: 0.35 }
            PropertyChanges { target: audioSyncPanel; opacity: 1 }
        }
    ]

    Rectangle {
        id: topScrim
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: dp(150)
        opacity: 0
        visible: opacity > 0.01
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#99000000" }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    Rectangle {
        id: bottomScrim
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: dp(360)
        opacity: 0
        visible: opacity > 0.01
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 1.0; color: "#CC000000" }
        }
    }

    Rectangle {
        id: backButton
        readonly property bool focused: overlay.isControlsActive() && overlay.row === "back" && !overlay.isAudioSyncOpen()
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: dp(40)
        width: dp(64)
        height: width
        radius: width / 2
        color: focused ? Qt.alpha(overlay.accent, 0.2) : "transparent"
        border.width: focused ? 2 : 0
        border.color: overlay.accentBright
        opacity: 0
        visible: opacity > 0.01

        MaterialIcon {
            anchors.centerIn: parent
            name: "arrow_back"
            iconColor: backButton.focused ? overlay.colTextStrong : overlay.colIconDim
            iconSize: dp(34)
        }

        MouseArea {
            anchors.fill: parent
            // Explicit exit gesture — skip handleBack's progressive layering.
            onClicked: overlay.stopPlayback("overlay-back")
        }
    }

    transitions: [
        Transition {
            to: "debug"
            NumberAnimation { properties: "opacity"; duration: 0 }
        },
        Transition {
            to: "audiosync"
            NumberAnimation { properties: "opacity"; duration: 0 }
        },
        Transition {
            NumberAnimation { properties: "opacity"; duration: 140; easing.type: Easing.OutCubic }
        }
    ]

    PlayerTrickplayPreview {
        id: trickplayPreview
        overlay: parent
    }

    PlayerSkipSegmentCard {
        id: skipSegmentCard
        overlay: parent
    }

    Item {
        id: hud
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: dp(52)
        height: dp(236)
        visible: opacity > 0.01
        opacity: 0

        ColumnLayout {
            anchors.fill: parent
            spacing: dp(16)

            RowLayout {
                Layout.fillWidth: true
                    spacing: dp(20)

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: dp(6)
                    Text {
                        Layout.fillWidth: true
                        text: overlay.hasPlayer ? overlay.player.title : ""
                        color: overlay.colText
                        font.pixelSize: dp(40)
                        font.weight: Font.Bold
                        font.hintingPreference: Font.PreferNoHinting
                        renderType: Text.QtRendering
                        maximumLineCount: 1
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: overlay.hasPlayer ? overlay.player.statusText : ""
                        color: overlay.hasPlayer && (overlay.player.buffering || overlay.player.seeking) ? overlay.accentBright : overlay.colStatus
                        font.pixelSize: dp(24)
                        font.weight: Font.Medium
                        font.hintingPreference: Font.PreferNoHinting
                        renderType: Text.QtRendering
                        maximumLineCount: 1
                        elide: Text.ElideRight
                    }
                }

                Text {
                    text: overlay.hasPlayer && overlay.player.paused ? "Paused" : "Playing"
                    color: overlay.hasPlayer && overlay.player.paused ? overlay.colTextStrong : overlay.colTextDim
                    font.pixelSize: dp(23)
                    font.weight: Font.DemiBold
                    font.hintingPreference: Font.PreferNoHinting
                    renderType: Text.QtRendering
                }
            }

            Item {
                id: timeline
                Layout.fillWidth: true
                Layout.preferredHeight: dp(82)
                readonly property double ratio: overlay.hasPlayer && overlay.player.durationSeconds > 0 ? Math.max(0, Math.min(1, overlay.positionSeconds() / overlay.player.durationSeconds)) : 0
                readonly property bool hasActiveSegment: overlay.hasPlayer && overlay.player.activeSegmentType.length > 0 && overlay.player.durationSeconds > 0
                readonly property double activeSegmentRatio: hasActiveSegment ? Math.max(0, Math.min(1, overlay.player.activeSegmentEndSeconds / overlay.player.durationSeconds)) : 0
                readonly property bool focused: overlay.isControlsActive() && overlay.row === "timeline" && !overlay.isAudioSyncOpen()

                Text {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    text: overlay.formatClock(overlay.positionSeconds())
                    color: timeline.focused ? overlay.colTextStrong : overlay.colTextDim
                    font.pixelSize: dp(23)
                    font.weight: Font.Medium
                    font.hintingPreference: Font.PreferNoHinting
                    renderType: Text.QtRendering
                }
                Text {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    text: overlay.formatClock(overlay.hasPlayer ? overlay.player.durationSeconds : 0)
                    color: overlay.colTextDim
                    font.pixelSize: dp(23)
                    font.weight: Font.Medium
                    font.hintingPreference: Font.PreferNoHinting
                    renderType: Text.QtRendering
                }
                Rectangle {
                    id: track
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: timeline.focused ? dp(18) : dp(11)
                    radius: height / 2
                    color: overlay.colTrackOuter
                    antialiasing: true
                    Behavior on height { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: Math.max(1, dp(2))
                        radius: height / 2
                        color: overlay.colTrackInner
                        antialiasing: true
                    }
                }
                Rectangle {
                    anchors.left: track.left
                    anchors.verticalCenter: track.verticalCenter
                    width: Math.max(track.height, track.width * timeline.ratio)
                    height: track.height
                    radius: height / 2
                    color: overlay.hasPlayer && overlay.player.buffering ? overlay.accentBright : overlay.accent
                    antialiasing: true
                    Behavior on width { enabled: !overlay.scrubbing && (!overlay.hasPlayer || !overlay.player.seeking); NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                }
                Repeater {
                    model: overlay.hasPlayer ? overlay.player.chapters : []
                    delegate: Rectangle {
                        required property var modelData
                        readonly property double startRatio: overlay.hasPlayer && overlay.player.durationSeconds > 0
                            ? Math.max(0, Math.min(1, (modelData.start || 0) / overlay.player.durationSeconds)) : 0
                        visible: startRatio > 0.002 && startRatio < 0.998
                        x: Math.max(0, Math.min(track.width - width, track.width * startRatio - width / 2))
                        anchors.verticalCenter: track.verticalCenter
                        width: Math.max(1, dp(2))
                        height: track.height
                        radius: width / 2
                        color: overlay.colTextStrong
                        opacity: 0.5
                        antialiasing: true
                    }
                }
                Rectangle {
                    x: Math.max(0, Math.min(track.width - width, track.width * timeline.activeSegmentRatio - width / 2))
                    anchors.verticalCenter: track.verticalCenter
                    width: Math.max(1, dp(3))
                    height: track.height + dp(12)
                    radius: width / 2
                    color: overlay.accentBright
                    antialiasing: true
                    opacity: 0.95
                    visible: timeline.hasActiveSegment
                }
                Rectangle {
                    id: thumbGlow
                    x: thumb.x - dp(5)
                    anchors.verticalCenter: track.verticalCenter
                    width: thumb.width + dp(10)
                    height: width
                    radius: width / 2
                    color: overlay.scrubbing || timeline.focused ? Qt.alpha(overlay.accent, 0.33) : overlay.colBackdropSoft
                    antialiasing: true
                    opacity: timeline.focused || overlay.scrubbing ? 1 : 0.55
                    Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                    Behavior on width { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                }
                Rectangle {
                    id: thumb
                    x: Math.max(0, Math.min(track.width - width, track.width * timeline.ratio - width / 2))
                    anchors.verticalCenter: track.verticalCenter
                    width: timeline.focused ? dp(34) : dp(20)
                    height: width
                    radius: width / 2
                    color: overlay.colTextStrong
                    antialiasing: true
                    Behavior on width { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }

                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * (timeline.focused || overlay.scrubbing ? 0.48 : 0.34)
                        height: width
                        radius: width / 2
                        color: overlay.accent
                        opacity: timeline.focused || overlay.scrubbing ? 1 : 0.8
                        antialiasing: true
                        Behavior on width { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    function scrub(mouse) {
                        overlay.row = "timeline"
                        overlay.mode = "controls"
                        overlay.scrubbing = true
                        overlay.scrubSeconds = overlay.clampSeconds((mouse.x / Math.max(1, width)) * (overlay.hasPlayer ? overlay.player.durationSeconds : 0))
                    }
                    onPressed: (mouse) => scrub(mouse)
                    onPositionChanged: (mouse) => { if (overlay.scrubbing) scrub(mouse) }
                    onReleased: (mouse) => { scrub(mouse); overlay.commitScrub() }
                    onCanceled: overlay.scrubbing = false
                }
            }

            RowLayout {
                id: actionRow
                Layout.fillWidth: true
                spacing: dp(8)

                Repeater {
                    model: overlay.actions.length
                    delegate: Rectangle {
                        required property int index
                        readonly property bool focused: overlay.isControlsActive() && overlay.row === "actions" && overlay.actionIndex === index && !overlay.isAudioSyncOpen()
                        readonly property string actionValue: overlay.actions[index].value
                        Layout.preferredWidth: overlay.actionTargetSize
                        Layout.preferredHeight: overlay.actionTargetSize
                        radius: Math.round(overlay.actionTargetSize / 2)
                        color: focused ? Qt.alpha(overlay.accent, 0.2) : "transparent"
                        border.width: focused ? 2 : 0
                        border.color: overlay.accentBright

                        MaterialIcon {
                            anchors.centerIn: parent
                            name: overlay.actionIcon(actionValue)
                            iconColor: focused ? overlay.colTextStrong : overlay.colIconDim
                            iconSize: actionValue === "debug" ? dp(30) : dp(36)
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                overlay.mode = "controls"
                                overlay.row = "actions"
                                overlay.actionIndex = index
                                overlay.activateAction()
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                RowLayout {
                    visible: overlay.desktopControlsAvailable
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: visible ? dp(300) : 0
                    spacing: dp(10)

                    MaterialIcon {
                        name: overlay.hasPlayer && overlay.player.volume === 0 ? "volume_off" : "volume_up"
                        iconColor: overlay.colIconDim
                        iconSize: dp(30)
                    }

                    Slider {
                        id: volumeSlider
                        Layout.preferredWidth: dp(210)
                        from: 0
                        to: 100
                        stepSize: 1
                        value: overlay.hasPlayer ? overlay.player.volume : 100
                        focusPolicy: Qt.NoFocus

                        background: Rectangle {
                            x: volumeSlider.leftPadding
                            y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                            width: volumeSlider.availableWidth
                            height: dp(7)
                            radius: height / 2
                            color: overlay.colTrackOuter

                            Rectangle {
                                width: volumeSlider.visualPosition * parent.width
                                height: parent.height
                                radius: parent.radius
                                color: overlay.accent
                            }
                        }

                        handle: Rectangle {
                            x: volumeSlider.leftPadding + volumeSlider.visualPosition * (volumeSlider.availableWidth - width)
                            y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                            width: dp(22)
                            height: width
                            radius: width / 2
                            color: overlay.colTextStrong
                            border.width: 2
                            border.color: overlay.accent
                        }

                        onMoved: if (overlay.hasPlayer) overlay.player.setVolume(Math.round(value))
                    }

                    Text {
                        text: overlay.hasPlayer ? Math.round(overlay.player.volume) + "%" : "100%"
                        color: overlay.colTextDim
                        font.pixelSize: dp(20)
                        font.weight: Font.DemiBold
                        font.hintingPreference: Font.PreferNoHinting
                        renderType: Text.QtRendering
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                visible: overlay.hasPlayer && overlay.player.errorText.length > 0
                text: overlay.hasPlayer ? overlay.player.errorText : ""
                color: overlay.colError
                font.pixelSize: dp(18)
                font.hintingPreference: Font.PreferNoHinting
                renderType: Text.QtRendering
                wrapMode: Text.Wrap
            }
        }
    }

    PlayerAudioSyncPanel {
        id: audioSyncPanel
        overlay: parent
    }

    PlayerOverlayMenu {
        id: menuPanel
        overlay: parent
    }
}
