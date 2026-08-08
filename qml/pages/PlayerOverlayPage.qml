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
    readonly property bool smartTvPlatform: Platform.isTV
    readonly property bool desktopControlsAvailable: !smartTvPlatform
    readonly property int currentSyncDelayMs: syncTarget === "audioOutput" ? Settings.audioDelayMs : syncTarget
                                                                             === "subtitle" ? player.subtitleDelayMs :
                                                                                              player.fileAudioDelayMs
    readonly property bool nightModeEnabled: Settings.nightModeEnabled
    readonly property int timelineAutohideMs: 4000
    readonly property int actionAutohideMs: 6200
    signal playbackBackRequested(var item)

    property bool tooltipSessionHadFile: false
    property bool controlsVisible: false
    property string focusZone: "timeline"
    property int actionIndex: 1
    property string menuKind: ""
    property bool audioSyncVisible: false
    property bool subtitleSettingsVisible: false
    property string audioSyncRow: "delay"
    property int audioSyncStepIndex: 2
    property string syncTarget: "audioFile"
    property bool scrubbing: false
    property double scrubSeconds: 0
    property bool timelineHovering: false
    property double timelineHoverSeconds: 0

    readonly property bool previewing: input.previewing
    readonly property real uiScale: Math.max(0.65, Math.min(1.25, Math.max(0.78, Math.min(1.0, height / 1440))
                                                            * Metrics.uiScale))
    readonly property int actionTargetSize: dp(68)
    readonly property var audioSyncSteps: [1, 5, 10, 100]
    readonly property bool audioSelectable: hasPlayer && player.audioTracks.length > 1
    // Nothing hides over a now playing stage, and the controls that only make
    // sense against a picture are left out of it.
    readonly property bool audioOnly: hasPlayer && player.mediaKind === "audio"
    readonly property var currentQueueItem: playQueue && playQueue.currentIndex >= 0 ? playQueue.get(
                                                                                           playQueue.currentIndex) : (
                                                                                           {})
    readonly property bool syncPlayMenuOpen: chrome.syncPlayMenuOpen
    readonly property string episodeContextText: {
        if (!episodeQueue)
            return ""
        const showName = String(currentQueueItem.seriesName || "").trim()
        const code = String(currentQueueItem.episodeCode || "").trim()
        if (showName.length > 0 && code.length > 0)
            return showName + " · " + code
        return showName.length > 0 ? showName : code
    }
    readonly property string overlayMetadataText: {
        if (episodeQueue)
            return episodeContextText
        if (String(currentQueueItem.itemType || "") !== "Movie")
            return ""
        const year = Number(currentQueueItem.year || 0)
        return year > 0 ? String(year) : ""
    }
    readonly property bool showEpisodeTitle: episodeQueue && !Boolean(currentQueueItem.genericEpisodeTitle) && String(
                                                 currentQueueItem.title || "").trim().length > 0
    readonly property string overlayTitle: episodeQueue ? String(currentQueueItem.title || "") : hasPlayer
                                                          ? player.title : ""
    readonly property bool playlistQueue: {
        if (!playQueue)
            return false
        for (let index = 0; index < playQueue.count; ++index) {
            const item = playQueue.get(index)
            if (item && String(item.playlistItemId || "").length > 0)
                return true
        }
        return false
    }
    readonly property bool episodeQueue: String(currentQueueItem.itemType || "") === "Episode"
    readonly property bool episodeContextMissing: episodeQueue && playQueue.count <= 1 && String(
                                                      currentQueueItem.seriesId || "").length > 0
    readonly property bool queueNavigationAvailable: {
        if (!playQueue || playQueue.count <= 1)
            return false
        return playlistQueue || episodeQueue
    }
    readonly property var transportActions: {
        const values = []
        const previousEpisode = (queueNavigationAvailable && playQueue.canGoPrevious) || episodeContextMissing
        const nextEpisode = (queueNavigationAvailable && playQueue.canGoNext) || episodeContextMissing
        if (previousEpisode)
            values.push("prevQueue")
        if (hasPlayer && player.hasChapters)
            values.push("prevChapter")
        values.push("back", "pause", "forward")
        if (hasPlayer && player.hasChapters)
            values.push("nextChapter")
        if (nextEpisode)
            values.push("nextQueue")
        return values
    }
    readonly property int pauseActionIndex: Math.max(0, actions.indexOf("pause"))
    readonly property var utilityActions: {
        const values = []
        if (!audioOnly)
            values.push("subtitles")
        if (audioSelectable)
            values.push("audio")
        if (playQueue && playQueue.count > 0)
            values.push("queue")
        values.push("syncplay")
        if (desktopControlsAvailable)
            values.push("fullscreen")
        values.push("debug")
        return values
    }
    readonly property var actions: transportActions.concat(utilityActions)
    // Each row carries what it does, so inserting one cannot silently rewire
    // the others the way a list of bare labels dispatched by position did.
    readonly property var debugOptions: {
        const values = [
                  {
                      "action": "speed",
                      "label": "Playback speed"
                  },
                  {
                      "action": "quality",
                      "label": "Quality"
                  }
              ]
        if (!audioOnly)
            values.push({
                            "action": "subtitleSettings",
                            "label": "Subtitle settings"
                        }, {
                            "action": "subtitleSync",
                            "label": "Subtitle sync"
                        })
        values.push({
                        "action": "audioSync",
                        "label": "Audio sync"
                    })
        if (!audioOnly)
            values.push({
                            "action": "nightMode",
                            "label": nightModeEnabled ? "Disable night mode" : "Enable night mode"
                        })
        values.push({
                        "action": "stats",
                        "label": hasPlayer && player.debugOsdVisible ? "Hide performance stats" :
                                                                       "Show performance stats"
                    })
        return values
    }
    property var qualityOptions: []
    readonly property var menuOptions: {
        if (menuKind === "subtitles")
            return hasPlayer ? player.subtitleTracks : []
        if (menuKind === "audio")
            return hasPlayer ? player.audioTracks : []
        if (menuKind === "quality")
            return qualityOptions
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
        if (action === "pause" && SyncPlay.enabled && SyncPlay.waitingForPlayback)
            return "schedule"
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
        if (action === "syncplay")
            return "groups"
        if (action === "fullscreen")
            return NativeWindow.fullScreen ? "fullscreen_exit" : "fullscreen"
        return "settings"
    }

    function actionTooltip(action) {
        if (action === "prevQueue")
            return playlistQueue ? "Play previous item" : "Play previous episode"
        if (action === "nextQueue")
            return playlistQueue ? "Play next item" : "Play next episode"
        if (action === "back")
            return "Back 10 seconds"
        if (action === "forward")
            return "Forward 10 seconds"
        if (action === "pause" && SyncPlay.enabled && SyncPlay.waitingForPlayback)
            return "Waiting for group playback"
        if (action === "pause")
            return hasPlayer && player.paused ? "Resume" : "Pause"
        if (action === "prevChapter")
            return "Previous chapter"
        if (action === "nextChapter")
            return "Next chapter"
        if (action === "subtitles")
            return "Subtitles"
        if (action === "audio")
            return "Audio track"
        if (action === "queue")
            return "Play queue"
        if (action === "syncplay")
            return SyncPlay.enabled ? "SyncPlay group" : "Join or create a SyncPlay group"
        if (action === "fullscreen")
            return NativeWindow.fullScreen ? "Exit fullscreen" : "Fullscreen"
        return "Playback settings"
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
        return menuKind.length > 0 || syncPlayMenuOpen
    }

    function isPinned() {
        return audioOnly || scrubbing || timelineHovering || isMenuOpen() || audioSyncVisible
    }

    function isControlsActive() {
        return controlsVisible && !isMenuOpen()
    }

    function restartAutohide() {
        autohide.interval = focusZone === "actions" ? actionAutohideMs : timelineAutohideMs
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

    function showControlsFromPointer() {
        if (!controlsVisible)
            showControls("timeline")
        else
            maybeRestartAutohide()
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
        actionIndex = pauseActionIndex
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
            seekTo(scrubSeconds)
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
        if (menuKind === "quality")
            return "Quality"
        return "Playback settings"
    }

    function menuPlaceholder() {
        return menuKind === "subtitles" ? "No subtitles available" : menuKind === "audio" ? "No audio tracks available" :
                                                                                            "No items available"
    }

    function menuLabel(item) {
        if (menuKind === "queue")
            return String(item && (item.displayTitle || item.title) || "Untitled")
        if (menuKind === "quality" || menuKind === "debug")
            return String(item && item.label || "")
        return String(item)
    }

    function debugAction(index) {
        return menuKind === "debug" && debugOptions[index] ? String(debugOptions[index].action) : ""
    }

    function menuDetail(index) {
        if (menuKind === "quality")
            return String(qualityOptions[index] && qualityOptions[index].detail || "")
        // Playback speed is the one row a SyncPlay group takes away.
        return debugAction(index) === "speed" && SyncPlay.enabled ? "Disabled by SyncPlay" : ""
    }

    function menuItemSelected(index) {
        if (menuKind === "quality")
            return Boolean(qualityOptions[index] && qualityOptions[index].selected)
        if (!hasPlayer)
            return false
        if (menuKind === "subtitles")
            return player.selectedSubtitleIndex === index
        if (menuKind === "audio")
            return player.selectedAudioIndex === index
        return menuKind === "queue" && playQueue && playQueue.currentIndex === index
    }

    function openMenu(kind) {
        if (syncPlayMenuOpen)
            chrome.closeSyncPlayMenu()
        if (kind === "quality")
            qualityOptions = App.streamingQualityOptions()
        menuKind = kind
        audioSyncVisible = false
        subtitleSettingsVisible = false
        controlsVisible = true
        const initialIndex = kind === "audio" && hasPlayer ? player.selectedAudioIndex : kind === "queue" && playQueue
                                                             ? playQueue.currentIndex : 0
        chrome.resetMenu(Math.max(0, initialIndex))
        autohide.stop()
    }

    function openSyncPlayMenu() {
        if (syncPlayMenuOpen) {
            closeMenu()
            return
        }
        menuKind = ""
        audioSyncVisible = false
        subtitleSettingsVisible = false
        controlsVisible = true
        chrome.openSyncPlayMenu()
        autohide.stop()
    }

    function closeMenu() {
        if (syncPlayMenuOpen)
            chrome.closeSyncPlayMenu()
        if (menuKind.length <= 0) {
            showControls(focusZone)
            return
        }
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
        else if (kind === "quality") {
            const option = qualityOptions[index]
            if (!option)
                return
            App.selectStreamingQuality(option.bitrate)
        } else if (kind === "debug") {
            const action = debugAction(index);
            // The speed row is a stepper, not a destination.
            if (action === "" || action === "speed")
                return
            menuKind = ""
            if (action === "quality") {
                openMenu("quality")
                return
            }
            if (action === "subtitleSettings") {
                openSubtitleSettings()
                return
            }
            if (action === "subtitleSync") {
                openSubtitleSync()
                return
            }
            if (action === "audioSync") {
                openAudioSync()
                return
            }
            if (action === "nightMode")
                Settings.setNightModeEnabled(!nightModeEnabled)
            else if (action === "stats")
                toggleDebugStats()
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
        syncTarget = "audioFile"
        audioSyncVisible = true
        controlsVisible = true
        audioSyncRow = "target"
        autohide.stop()
    }

    function openSubtitleSync() {
        menuKind = ""
        syncTarget = "subtitle"
        audioSyncVisible = true
        controlsVisible = true
        audioSyncRow = "delay"
        autohide.stop()
    }

    function openSubtitleSettings() {
        menuKind = ""
        audioSyncVisible = false
        subtitleSettingsVisible = true
        hideControls()
        Qt.callLater(function () {
            subtitleSettings.forceActiveFocus()
            subtitleSettings.focusRow(0)
        })
    }

    function closeSubtitleSettings() {
        if (!subtitleSettingsVisible)
            return
        subtitleSettingsVisible = false
        showControls("actions")
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

    function formatPlaybackSpeed(value) {
        return Number(value || 1).toFixed(2) + "×"
    }

    function adjustPlaybackSpeed(direction) {
        if (!hasPlayer || SyncPlay.enabled || direction === 0)
            return
        player.setPlaybackSpeed(Math.max(0.25, Math.min(4, Number(player.playbackSpeed) + direction * 0.25)))
    }

    function adjustAudioDelay(direction) {
        const index = Math.max(0, Math.min(audioSyncSteps.length - 1, audioSyncStepIndex))
        const next = clampAudioDelayMs(currentSyncDelayMs + direction * audioSyncSteps[index])
        if (syncTarget === "audioOutput")
            Settings.setAudioDelayMs(next)
        else if (syncTarget === "subtitle")
            player.setSubtitleDelayMs(next)
        else
            player.setFileAudioDelayMs(next)
    }

    function handleAudioSyncKey(key) {
        const subtitleSync = syncTarget === "subtitle"
        if (key === Qt.Key_Up) {
            if (audioSyncRow === "step")
                audioSyncRow = "delay"
            else if (audioSyncRow === "delay" && !subtitleSync)
                audioSyncRow = "target"
            else
                closeAudioSync()
            return true
        }
        if (key === Qt.Key_Down) {
            if (audioSyncRow === "target" || audioSyncRow === "delay")
                audioSyncRow = audioSyncRow === "target" ? "delay" : "step"
            else
                closeAudioSync()
            return true
        }
        if (audioSyncRow === "target") {
            const targets = ["audioFile", "audioOutput"]
            const direction = key === Qt.Key_Left ? -1 : key === Qt.Key_Right ? 1 : 0
            if (direction !== 0) {
                syncTarget = targets[(targets.indexOf(syncTarget) + direction + targets.length) % targets.length]
                return true
            }
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

    function togglePlayback() {
        if (SyncPlay.enabled)
            SyncPlay.requestTogglePause()
        else
            player.togglePause()
    }

    function seekTo(seconds) {
        if (SyncPlay.enabled)
            SyncPlay.requestSeek(clampSeconds(seconds))
        else
            player.seek(clampSeconds(seconds))
    }

    function seekRelative(seconds) {
        if (SyncPlay.enabled)
            SyncPlay.requestRelativeSeek(seconds)
        else if (seconds < 0)
            player.seekBack()
        else
            player.seekForward()
    }

    function activateAction() {
        if (!hasPlayer || actions.length === 0)
            return
        const action = actions[Math.max(0, Math.min(actions.length - 1, actionIndex))]
        if (action === "back")
            seekRelative(-10)
        else if (action === "pause")
            togglePlayback()
        else if (action === "forward")
            seekRelative(10)
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
        else if (action === "syncplay")
            openSyncPlayMenu()
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
        if (hasPlayer && player.backAllowed) {
            if (reason === "overlay-back")
                playbackBackRequested(currentQueueItem)
            player.stopWithReason(reason)
        }
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
        if (subtitleSettingsVisible) {
            closeSubtitleSettings()
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
        if (subtitleSettingsVisible)
            return subtitleSettings.routeKey(key, phase, repeat)
        if (syncPlayMenuOpen && InputKeys.isDirection(key))
            return phase === "press" ? chrome.routeSyncPlayMenuKey(key, repeat) : true
        if (menuKind.length > 0 && InputKeys.isDirection(key))
            return phase === "press" ? chrome.routeMenuKey(key, repeat) : true
        if (audioSyncVisible && InputKeys.isDirection(key))
            return phase === "press" ? handleAudioSyncKey(key) : true
        return input.routeKey(key, phase, repeat)
    }

    function activate() {
        if (subtitleSettingsVisible) {
            subtitleSettings.activate()
            return
        }
        if (syncPlayMenuOpen) {
            chrome.activateSyncPlayMenu()
            return
        }
        if (isMenuOpen()) {
            chrome.activateMenu()
            return
        }
        if (audioSyncVisible) {
            if (audioSyncRow === "delay" && hasPlayer)
                togglePlayback()
            return
        }
        if (!controlsVisible) {
            if (hasPlayer)
                togglePlayback()
            showControls("actions")
            return
        }
        if (focusZone === "back") {
            stopPlayback("overlay-back")
            return
        }
        if (focusZone === "timeline") {
            if (!commitScrub() && hasPlayer)
                togglePlayback()
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
        chrome.closeSyncPlayMenu()
        audioSyncVisible = false
        subtitleSettingsVisible = false
        focusZone = "timeline"
        actionIndex = pauseActionIndex
        controlsVisible = visible
        if (visible)
            restartAutohide()
        if (visible && hasPlayer && player.fileLoaded)
            tooltipSessionHadFile = true
    }
    onScrubbingChanged: if (!scrubbing)
                            maybeRestartAutohide()

    Connections {
        target: player

        function onPlaybackStateChanged() {
            if (player.sessionActive && player.fileLoaded)
                overlay.tooltipSessionHadFile = true
        }

        function onSessionActiveChanged() {
            if (player.sessionActive) {
                // A now playing stage is only the chrome and the cover, so it
                // opens with the controls already up rather than on a keypress.
                if (overlay.audioOnly)
                    overlay.showControls("actions")
                return
            }
            if (overlay.tooltipSessionHadFile)
                Settings.completePlayerControlTooltipSession()
            overlay.tooltipSessionHadFile = false
        }
    }

    PlayerOverlayInput {
        id: input
        overlay: parent
    }

    Timer {
        id: autohide
        interval: overlay.timelineAutohideMs
        onTriggered: overlay.hideControls()
    }

    SubtitleSettingsPanel {
        id: subtitleSettings
        anchors.fill: parent
        visible: overlay.subtitleSettingsVisible
        enabled: visible
        z: 60
        overVideo: true
        onDismissed: overlay.closeSubtitleSettings()
    }

    PlayerOverlayChrome {
        id: chrome
        anchors.fill: parent
        overlay: parent
    }
}
