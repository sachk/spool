import QtQuick
import JellyfinWebOS
import "../theme"
import "../primitives"

FocusScope {
    id: overlay
    focus: false

    readonly property var player: Player
    readonly property var playQueue: PlayQueue
    readonly property bool hasPlayer: player.sessionActive
    readonly property bool smartTvPlatform: NativeWindow.smartTvPlatform
    readonly property bool desktopControlsAvailable: !smartTvPlatform
    readonly property int currentAudioDelayMs: Settings.audioDelayMs
    readonly property bool nightModeEnabled: Settings.nightModeEnabled

    property bool controlsVisible: false
    property string focusZone: "timeline"
    property int actionIndex: 1
    property string menuKind: ""
    property bool audioSyncVisible: false
    property string audioSyncRow: "delay"
    property int audioSyncStepIndex: 2
    property bool scrubbing: false
    property double scrubSeconds: 0

    readonly property bool previewing: input.previewing
    readonly property real uiScale: Math.max(0.65, Math.min(1.25, Math.max(0.78, Math.min(1.0, height / 1440))
                                                            * Metrics.uiScale))
    readonly property int actionTargetSize: dp(64)
    readonly property var audioSyncSteps: [1, 5, 10, 100]
    readonly property bool audioSelectable: hasPlayer && player.audioTracks.length > 1
    readonly property bool playlistNavigationAvailable: {
        if (!playQueue || playQueue.count <= 1)
            return false
        for (let index = 0; index < playQueue.count; ++index) {
            const item = playQueue.get(index)
            if (item && String(item.playlistItemId || "").length > 0)
                return true
        }
        return false
    }
    readonly property var actions: {
        const values = ["back", "pause", "forward"]
        if (playlistNavigationAvailable && playQueue.canGoPrevious)
            values.push("prevQueue")
        if (playlistNavigationAvailable && playQueue.canGoNext)
            values.push("nextQueue")
        if (hasPlayer && player.hasChapters)
            values.push("prevChapter", "nextChapter")
        values.push("subtitles")
        if (audioSelectable)
            values.push("audio")
        if (playQueue && playQueue.count > 0)
            values.push("queue")
        if (desktopControlsAvailable)
            values.push("fullscreen")
        values.push("debug")
        return values
    }
    readonly property var debugOptions: ["Audio sync", hasPlayer && player.debugOsdVisible ? "Hide debug stats" : "Show debug stats",
        nightModeEnabled ? "Disable night mode" : "Enable night mode", "Stop playback"]
    readonly property var menuOptions: {
        if (menuKind === "subtitles")
            return hasPlayer ? player.subtitleTracks : []
        if (menuKind === "audio")
            return hasPlayer ? player.audioTracks : []
        if (menuKind === "debug")
            return debugOptions
        const values = []
        if (menuKind === "queue" && playQueue)
            for (let index = 0; index < playQueue.count; ++index)
                values.push(playQueue.get(index))
        return values
    }

    function dp(value) {
        return Math.round(value * uiScale)
    }

    function formatClock(seconds) {
        const total = Math.max(0, Math.floor(seconds || 0))
        const hours = Math.floor(total / 3600)
        const minutes = Math.floor((total % 3600) / 60)
        const remainder = total % 60
        const clock = String(minutes).padStart(hours > 0 ? 2 : 1, "0") + ":" + String(remainder).padStart(2, "0")
        return hours > 0 ? hours + ":" + clock : clock
    }

    function actionIcon(action) {
        if (action === "back")
            return "fast_rewind"
        if (action === "pause")
            return hasPlayer && player.paused ? "play_arrow" : "pause"
        if (action === "forward")
            return "fast_forward"
        if (action === "prevQueue" || action === "prevChapter")
            return "skip_previous"
        if (action === "nextQueue" || action === "nextChapter")
            return "skip_next"
        if (action === "subtitles")
            return "closed_caption"
        if (action === "audio")
            return "audiotrack"
        if (action === "queue")
            return "playlist_play"
        if (action === "fullscreen")
            return NativeWindow.fullScreen ? "fullscreen_exit" : "fullscreen"
        return "settings"
    }

    function clampSeconds(seconds) {
        const value = Math.max(0, seconds || 0)
        const duration = hasPlayer ? player.durationSeconds || 0 : 0
        return duration > 0 ? Math.min(duration, value) : value
    }

    function positionSeconds() {
        return scrubbing ? scrubSeconds : hasPlayer ? player.positionSeconds : 0
    }

    function positionRatio() {
        return hasPlayer && player.durationSeconds > 0 ? Math.max(0, Math.min(1, positionSeconds()
                                                                              / player.durationSeconds)) : 0
    }

    function isMenuOpen() {
        return menuKind.length > 0
    }

    function isPinned() {
        return scrubbing || isMenuOpen() || audioSyncVisible
    }

    function isControlsActive() {
        return controlsVisible && !isMenuOpen()
    }

    function restartAutohide() {
        autohide.interval = focusZone === "actions" ? 5200 : 3000
        autohide.restart()
    }

    function showControls(preferredZone) {
        if (preferredZone)
            focusZone = preferredZone
        controlsVisible = true
        if (isPinned())
            autohide.stop()
        else
            restartAutohide()
    }

    function maybeRestartAutohide() {
        if (controlsVisible && !isPinned())
            restartAutohide()
    }

    function hideControls() {
        if (isPinned())
            return false
        input.reset()
        autohide.stop()
        controlsVisible = false
        focusZone = "timeline"
        return true
    }

    function seekBy(delta) {
        showControls("timeline")
        scrubbing = false
        if (hasPlayer)
            player.previewSeekBy(delta)
    }

    function canPreviewSeek() {
        return !controlsVisible || focusZone === "timeline"
    }

    function commitScrub() {
        if (!scrubbing)
            return false
        if (hasPlayer)
            player.seek(clampSeconds(scrubSeconds))
        scrubbing = false
        maybeRestartAutohide()
        return true
    }

    function menuTitle() {
        if (menuKind === "subtitles")
            return "Subtitles"
        if (menuKind === "audio")
            return "Audio"
        if (menuKind === "queue")
            return "Queue"
        return "Playback settings"
    }

    function menuPlaceholder() {
        return menuKind === "subtitles" ? "No subtitles available" : menuKind === "audio" ? "No audio tracks available" :
                                                                                            "No items available"
    }

    function menuLabel(item) {
        return menuKind === "queue" ? String(item && (item.displayTitle || item.title) || "Untitled") : String(item)
    }

    function menuItemSelected(index) {
        if (!hasPlayer)
            return false
        if (menuKind === "subtitles")
            return player.selectedSubtitleIndex === index
        if (menuKind === "audio")
            return player.selectedAudioIndex === index
        return menuKind === "queue" && playQueue && playQueue.currentIndex === index
    }

    function openMenu(kind) {
        menuKind = kind
        audioSyncVisible = false
        controlsVisible = true
        const initialIndex = kind === "audio" && hasPlayer ? player.selectedAudioIndex : kind === "queue" && playQueue
                                                             ? playQueue.currentIndex : 0
        chrome.resetMenu(Math.max(0, initialIndex))
        autohide.stop()
    }

    function closeMenu() {
        if (!isMenuOpen())
            return
        menuKind = ""
        showControls(focusZone)
    }

    function activateMenuItem(index) {
        const kind = menuKind
        if (kind === "subtitles" && hasPlayer && player.subtitleTracks.length > 0)
            player.selectSubtitle(index)
        else if (kind === "audio" && hasPlayer && player.audioTracks.length > 0)
            player.selectAudio(index)
        else if (kind === "queue" && playQueue && playQueue.count > 0)
            App.playQueueItem(index)
        else if (kind === "debug") {
            menuKind = ""
            if (index === 0) {
                openAudioSync()
                return
            }
            if (index === 1)
                toggleDebugStats()
            else if (index === 2)
                Settings.setNightModeEnabled(!nightModeEnabled)
            else if (index === 3 && hasPlayer)
                player.stopWithReason("debug-menu-stop")
            if (controlsVisible)
                maybeRestartAutohide()
            return
        } else {
            return
        }
        closeMenu()
    }

    function openAudioSync() {
        menuKind = ""
        audioSyncVisible = true
        controlsVisible = true
        audioSyncRow = "delay"
        autohide.stop()
    }

    function closeAudioSync() {
        if (!audioSyncVisible)
            return
        audioSyncVisible = false
        showControls("actions")
    }

    function clampAudioDelayMs(value) {
        return Math.max(-2000, Math.min(2000, Math.round(value || 0)))
    }

    function formatAudioDelay(value) {
        const milliseconds = clampAudioDelayMs(value)
        return milliseconds > 0 ? "+" + milliseconds + " ms" : milliseconds + " ms"
    }

    function adjustAudioDelay(direction) {
        const index = Math.max(0, Math.min(audioSyncSteps.length - 1, audioSyncStepIndex))
        Settings.setAudioDelayMs(clampAudioDelayMs(currentAudioDelayMs + direction * audioSyncSteps[index]))
    }

    function handleAudioSyncKey(key) {
        if (key === Qt.Key_Up) {
            if (audioSyncRow === "step")
                audioSyncRow = "delay"
            else
                closeAudioSync()
            return true
        }
        if (key === Qt.Key_Down) {
            if (audioSyncRow === "delay")
                audioSyncRow = "step"
            else
                closeAudioSync()
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
        return false
    }

    function colorAction(key) {
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

    function playPrevious() {
        App.playQueuePrevious()
    }

    function playNext() {
        App.playQueueNext()
    }

    function wheelVolumeDelta(event) {
        const raw = event.angleDelta.y !== 0 ? event.angleDelta.y / 120 : event.pixelDelta.y / 80
        if (raw === 0)
            return 0
        const rounded = Math.round(raw)
        return (rounded !== 0 ? rounded : raw > 0 ? 1 : -1) * 5
    }

    function adjustVolumeFromWheel(event) {
        if (!desktopControlsAvailable || !hasPlayer)
            return
        const delta = wheelVolumeDelta(event)
        if (delta === 0)
            return
        player.adjustVolume(delta)
        showControls("actions")
        event.accepted = true
    }

    function activateAction() {
        if (!hasPlayer || actions.length === 0)
            return
        const action = actions[Math.max(0, Math.min(actions.length - 1, actionIndex))]
        if (action === "back")
            player.seekBack()
        else if (action === "pause")
            player.togglePause()
        else if (action === "forward")
            player.seekForward()
        else if (action === "prevQueue")
            App.playQueuePrevious()
        else if (action === "nextQueue")
            App.playQueueNext()
        else if (action === "prevChapter")
            player.previousChapter()
        else if (action === "nextChapter")
            player.nextChapter()
        else if (action === "subtitles" || action === "audio" || action === "queue" || action === "debug")
            openMenu(action)
        else if (action === "fullscreen")
            NativeWindow.toggleFullScreen()
    }

    function toggleDebugStats() {
        if (!hasPlayer)
            return
        const showing = !player.debugOsdVisible
        player.toggleDebugOsd()
        if (showing) {
            autohide.stop()
            controlsVisible = false
            focusZone = "timeline"
        } else {
            showControls(focusZone)
        }
    }

    function stopPlayback(reason) {
        if (hasPlayer && player.backAllowed)
            player.stopWithReason(reason)
        return true
    }

    function back() {
        input.reset()
        if (scrubbing) {
            scrubbing = false
            controlsVisible = false
            autohide.stop()
            return true
        }
        if (audioSyncVisible) {
            closeAudioSync()
            return true
        }
        if (isMenuOpen()) {
            closeMenu()
            return true
        }
        if (controlsVisible)
            return hideControls()
        return stopPlayback("overlay-back")
    }

    function routeKey(key, phase, repeat) {
        if (phase === "press" && (isMenuOpen() || audioSyncVisible) && InputKeys.isDirection(key))
            return true
        if (phase === "release" && isMenuOpen() && chrome.routeMenuKey(key))
            return true
        if (phase === "release" && audioSyncVisible && handleAudioSyncKey(key))
            return true
        return input.routeKey(key, phase, repeat)
    }

    function activate() {
        if (isMenuOpen()) {
            chrome.activateMenu()
            return
        }
        if (audioSyncVisible) {
            if (audioSyncRow === "delay" && hasPlayer)
                player.togglePause()
            return
        }
        if (!controlsVisible) {
            if (hasPlayer)
                player.togglePause()
            showControls("actions")
            return
        }
        if (focusZone === "back") {
            stopPlayback("overlay-back")
            return
        }
        if (focusZone === "timeline") {
            if (!commitScrub() && hasPlayer)
                player.togglePause()
        } else {
            activateAction()
        }
        if (!isPinned())
            showControls(focusZone)
    }

    onVisibleChanged: {
        input.reset()
        autohide.stop()
        scrubbing = false
        menuKind = ""
        audioSyncVisible = false
        focusZone = "timeline"
        actionIndex = 1
        controlsVisible = visible
        if (visible)
            restartAutohide()
    }
    onScrubbingChanged: if (!scrubbing)
                            maybeRestartAutohide()

    PlayerOverlayInput {
        id: input
        overlay: parent
    }

    Timer {
        id: autohide
        interval: 3000
        onTriggered: overlay.hideControls()
    }

    PlayerOverlayChrome {
        id: chrome
        anchors.fill: parent
        overlay: parent
    }
}
