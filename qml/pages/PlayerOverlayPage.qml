import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import JellyfinWebOS
import "../theme"
import "../primitives"
import "PlayerOverlayFormat.js" as PlayerOverlayFormat

FocusScope {
    id: overlay
    focus: false

    property var shell
    readonly property var player: playerController ? playerController : null
    readonly property var playQueue: playQueueController ? playQueueController : null
    readonly property bool hasPlayer: player !== null && player !== undefined && player.sessionActive
    readonly property bool smartTvPlatform: nativeWindow ? nativeWindow.smartTvPlatform : true
    readonly property bool desktopControlsAvailable: !smartTvPlatform
    readonly property int currentAudioDelayMs: settingsController ? settingsController.audioDelayMs : 0
    readonly property bool nightModeEnabled: settingsController ? settingsController.nightModeEnabled : false
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
    // Jellyfin playlist entries carry PlaylistItemId. Library/Next Up rows also
    // populate playQueue for autoplay, but should not expose explicit previous/
    // next-item controls for a standalone movie or one-off episode.
    readonly property bool playlistNavigationAvailable: {
        const count = playQueue ? playQueue.count : 0
        if (count <= 1)
            return false
        for (let i = 0; i < count; ++i) {
            const item = playQueue.get(i)
            if (item && String(item.playlistItemId || "").length > 0)
                return true
        }
        return false
    }
    readonly property var actions: {
        const list = [
                  {
                      label: "Rewind",
                      value: "back"
                  },
                  {
                      label: hasPlayer && player.paused ? "Play" : "Pause",
                      value: "pause"
                  },
                  {
                      label: "Fast forward",
                      value: "forward"
                  }
              ]
        if (playlistNavigationAvailable && playQueue.canGoPrevious)
            list.push({
                          label: "Previous item",
                          value: "prevQueue"
                      })
        if (playlistNavigationAvailable && playQueue.canGoNext)
            list.push({
                          label: "Next item",
                          value: "nextQueue"
                      })
        if (hasPlayer && player.hasChapters) {
            list.push({
                          label: "Previous chapter",
                          value: "prevChapter"
                      })
            list.push({
                          label: "Next chapter",
                          value: "nextChapter"
                      })
        }
        list.push({
                      label: "Subtitles",
                      value: "subtitles"
                  })
        if (audioSelectable)
            list.push({
                          label: "Audio",
                          value: "audio"
                      })
        if (playQueue && playQueue.count > 0)
            list.push({
                          label: "Queue",
                          value: "queue"
                      })
        if (desktopControlsAvailable)
            list.push({
                          label: nativeWindow.fullScreen ? "Exit full screen" : "Full screen",
                          value: "fullscreen"
                      })
        list.push({
                      label: "Settings",
                      value: "debug"
                  })
        return list
    }
    readonly property var queueOptions: {
        const count = playQueue ? playQueue.count : 0
        const list = []
        for (let i = 0; i < count; ++i)
            list.push(playQueue.get(i))
        return list
    }
    readonly property var audioSyncSteps: [1, 5, 10, 100]
    readonly property var debugOptions: ["Audio sync", hasPlayer && player.debugOsdVisible ? "Hide debug stats" : "Show debug stats",
        nightModeEnabled ? "Disable night mode" : "Enable night mode", "Stop playback"]
    function isMenuOpen() {
        return mode === "subtitles" || mode === "audio" || mode === "queue" || mode === "debug"
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
        autohide.restart(autohideDelayMs())
    }

    function formatClock(seconds) {
        return PlayerOverlayFormat.formatClock(seconds)
    }

    function formatAudioDelay(value) {
        return PlayerOverlayFormat.formatAudioDelay(clampAudioDelayMs(value))
    }

    function actionIcon(value) {
        return PlayerOverlayFormat.actionIcon(value, hasPlayer && player.paused, nativeWindow
                                              && nativeWindow.fullScreen)
    }

    function actionCenterX(actionValue) {
        const idx = actions.findIndex(action => action.value === actionValue)
        if (idx < 0 || chrome.actionRowWidth <= 0)
            return width - dp(240)
        return chrome.hudX + chrome.actionRowX + (idx * (actionTargetSize + chrome.actionRowSpacing)) + actionTargetSize
                / 2
    }

    function setMenuAnchor(actionValue) {
        menuAnchorX = actionCenterX(actionValue)
        menuAnchorY = chrome.hudY + chrome.actionRowY
    }

    function clampSeconds(seconds) {
        const value = Math.max(0, seconds || 0)
        const duration = hasPlayer ? player.durationSeconds || 0 : 0
        return duration > 0 ? Math.min(duration, value) : value
    }

    function positionSeconds() {
        if (scrubbing)
            return scrubSeconds
        return hasPlayer ? player.positionSeconds : 0
    }

    function positionRatio() {
        if (!hasPlayer || player.durationSeconds <= 0)
            return 0
        return Math.max(0, Math.min(1, positionSeconds() / player.durationSeconds))
    }

    function showControls(preferredRow) {
        if (mode === "hidden")
            row = preferredRow || "timeline"
        if (!isMenuOpen() && !isAudioSyncOpen())
            mode = "controls"
        if (isPinned())
            autohide.stop()
        else
            restartAutohide()
    }

    function maybeRestartAutohide() {
        if (mode !== "hidden" && !isPinned())
            restartAutohide()
    }

    function hideControls() {
        if (isPinned()) {
            if (scrubbing)
                autohide.restart(autohideDelayMs())
            return false
        }
        cancelHeldNavigation()
        autohide.stop()
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
        input.resetDownHold()
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
        chrome.positionMenuAtTop()
        autohide.stop()
    }

    function openAudio() {
        setMenuAnchor("audio")
        mode = "audio"
        menuIndex = hasPlayer ? Math.max(0, player.selectedAudioIndex) : 0
        chrome.positionMenuAtTop()
        autohide.stop()
    }

    function openQueue() {
        setMenuAnchor("queue")
        mode = "queue"
        menuIndex = playQueue ? Math.max(0, playQueue.currentIndex) : 0
        chrome.positionMenuAtTop()
        autohide.stop()
    }

    function openDebugMenu() {
        setMenuAnchor("debug")
        mode = "debug"
        menuIndex = 0
        chrome.positionMenuAtTop()
        autohide.stop()
    }

    function openAudioSync() {
        mode = "audiosync"
        audioSyncRow = "delay"
        autohide.stop()
    }

    function wheelVolumeDelta(event) {
        const rawSteps = event.angleDelta.y !== 0 ? event.angleDelta.y / 120 : event.pixelDelta.y !== 0
                                                    ? event.pixelDelta.y / 80 : 0
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
        if (action === "back")
            player.seekBack()
        else if (action === "pause")
            player.togglePause()
        else if (action === "forward")
            player.seekForward()
        else if (action === "prevQueue")
            appController.playQueuePrevious()
        else if (action === "nextQueue")
            appController.playQueueNext()
        else if (action === "prevChapter")
            player.previousChapter()
        else if (action === "nextChapter")
            player.nextChapter()
        else if (action === "subtitles")
            openSubtitles()
        else if (action === "audio")
            openAudio()
        else if (action === "queue")
            openQueue()
        else if (action === "fullscreen" && nativeWindow)
            nativeWindow.toggleFullScreen()
        else if (action === "debug")
            openDebugMenu()
    }

    function activateMenuItem() {
        if (mode === "subtitles") {
            if (!hasPlayer || player.subtitleTracks.length === 0)
                return
            if (hasPlayer)
                player.selectSubtitle(menuIndex)
            closeMenu()
            return
        }
        if (mode === "audio") {
            if (!hasPlayer || player.audioTracks.length === 0)
                return
            if (hasPlayer)
                player.selectAudio(menuIndex)
            closeMenu()
            return
        }
        if (mode === "queue") {
            if (!playQueue || playQueue.count === 0)
                return
            appController.playQueueItem(menuIndex)
            closeMenu()
            return
        }
        if (menuIndex === 0) {
            openAudioSync()
            return
        } else if (menuIndex === 1)
            toggleDebugStats()
        else if (menuIndex === 2 && settingsController)
            settingsController.setNightModeEnabled(!nightModeEnabled)
        else if (menuIndex === 3 && hasPlayer)
            player.stopWithReason("debug-menu-stop")
        if (mode === "debug")
            closeMenu()
    }

    function toggleDebugStats() {
        if (!hasPlayer)
            return
        const showing = !player.debugOsdVisible
        player.toggleDebugOsd()
        if (showing) {
            autohide.stop()
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
        if (settingsController)
            settingsController.setAudioDelayMs(clampAudioDelayMs(value))
    }

    function adjustAudioDelay(direction) {
        const step = audioSyncSteps[Math.max(0, Math.min(audioSyncSteps.length - 1, audioSyncStepIndex))]
        setAudioDelayMs(currentAudioDelayMs + direction * step)
    }

    function handleAudioSyncKey(key) {
        if (key === Qt.Key_Up) {
            if (audioSyncRow === "step")
                audioSyncRow = "delay"
            else {
                mode = "controls"
                row = "actions"
                actionIndex = 1
                showControls(row)
            }
            return true
        }
        if (key === Qt.Key_Down) {
            if (audioSyncRow === "delay")
                audioSyncRow = "step"
            else {
                mode = "controls"
                row = "actions"
                actionIndex = 1
                showControls(row)
            }
            return true
        }
        if (key === Qt.Key_Left) {
            if (audioSyncRow === "delay")
                adjustAudioDelay(-1)
            else
                audioSyncStepIndex = Math.max(0, audioSyncStepIndex - 1)
            return true
        }
        if (key === Qt.Key_Right) {
            if (audioSyncRow === "delay")
                adjustAudioDelay(1)
            else
                audioSyncStepIndex = Math.min(audioSyncSteps.length - 1, audioSyncStepIndex + 1)
            return true
        }
        if (InputKeys.isAccept(key)) {
            if (audioSyncRow === "delay" && hasPlayer)
                player.togglePause()
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
            autohide.stop()
            mode = "hidden"
            row = "timeline"
            return true
        }
        if (isAudioSyncOpen()) {
            mode = "controls"
            showControls("actions")
            return true
        }
        if (isMenuOpen()) {
            closeMenu()
            return true
        }
        if (mode !== "hidden") {
            autohide.stop()
            mode = "hidden"
            row = "timeline"
            return true
        }
        return stopPlayback("overlay-back")
    }

    function handleReleased(event) {
        return input.handleReleased(event)
    }

    function handlePressed(event) {
        return input.handlePressed(event)
    }

    onVisibleChanged: {
        if (visible) {
            mode = "controls"
            row = "timeline"
            actionIndex = 1
            showControls("timeline")
        } else {
            autohide.stop()
            scrubTimer.stop()
            previewBurstTimer.stop()
            stopPreviewSeekHold()
            input.resetDownHold()
            previewBurstActive = false
            mode = "hidden"
            row = "timeline"
            scrubbing = false
            audioSyncRow = "delay"
        }
    }

    onScrubbingChanged: if (!scrubbing)
                            maybeRestartAutohide()

    PlayerOverlayInput {
        id: input
        overlay: parent
    }

    PlayerOverlayAutoHide {
        id: autohide
        overlay: parent
    }

    Timer {
        id: scrubTimer
        interval: 650
        onTriggered: overlay.commitScrub()
    }
    Timer {
        id: previewBurstTimer
        interval: 1300
        repeat: false
        onTriggered: overlay.previewBurstActive = false
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

    PlayerOverlayChrome {
        id: chrome
        anchors.fill: parent
        overlay: parent
    }
}
