import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import JellyfinWebOS

FocusScope {
    id: overlay
    focus: false

    property var shell
    readonly property var player: appController ? appController.player : null
    readonly property bool hasPlayer: player !== null && player !== undefined
    readonly property int currentAudioDelayMs: appController ? appController.audioDelayMs : 0
    readonly property bool nightModeEnabled: appController ? appController.nightModeEnabled : false
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
    readonly property real uiScale: Math.max(0.78, Math.min(1.0, height / 1440))
    readonly property int actionTargetSize: Math.round(64 * uiScale)
    property real menuAnchorX: width - Math.round(240 * uiScale)
    property real menuAnchorY: height - Math.round(170 * uiScale)

    readonly property var actions: [
        { label: "Rewind", value: "back" },
        { label: hasPlayer && player.paused ? "Play" : "Pause", value: "pause" },
        { label: "Fast forward", value: "forward" },
        { label: "Subtitles", value: "subtitles" },
        { label: "Audio", value: "audio" },
        { label: "Settings", value: "debug" }
    ]
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
        return (hasPlayer && player.paused) || scrubbing || isMenuOpen() || isAudioSyncOpen()
    }

    function isControlsActive() {
        return mode === "controls" || isAudioSyncOpen()
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
        if (value === "subtitles") return "closed_caption"
        if (value === "audio") return "audiotrack"
        return "settings"
    }

    function actionCenterX(actionValue) {
        const idx = actions.findIndex(action => action.value === actionValue)
        if (idx < 0 || actionRow.width <= 0)
            return width - Math.round(240 * uiScale)
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
        else autohideTimer.restart()
    }

    function maybeRestartAutohide() {
        if (mode !== "hidden" && !isPinned())
            autohideTimer.restart()
    }

    function hideControls() {
        if (isPinned()) {
            if (scrubbing)
                autohideTimer.restart()
            return false
        }
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
        if (hasPlayer)
            player.previewSeekBy(delta)
    }

    function previewSeek(delta) {
        seekBy(delta)
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
        maybeRestartAutohide()
        return true
    }

    function isBackEvent(event) {
        const scanCode = Number(event.nativeScanCode || 0)
        return event.key === Qt.Key_Back
                || event.key === Qt.Key_Escape
                || event.key === Qt.Key_Backspace
                || event.key === Qt.Key_BrowserBack
                || event.key === 0x01200003
                || (event.key === 0 && scanCode === 420)
    }

    function isIgnoredPlayerNoise(event) {
        const scanCode = Number(event.nativeScanCode || 0)
        return event.key === 0 && (scanCode === 1206 || scanCode === 1207)
    }

    function isAcceptKey(key) {
        return key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select || key === Qt.Key_Space
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
        menuList.positionViewAtBeginning()
        autohideTimer.stop()
    }

    function openAudio() {
        setMenuAnchor("audio")
        mode = "audio"
        menuIndex = hasPlayer ? Math.max(0, player.selectedAudioIndex) : 0
        menuList.positionViewAtBeginning()
        autohideTimer.stop()
    }

    function openDebugMenu() {
        setMenuAnchor("debug")
        mode = "debug"
        menuIndex = 0
        menuList.positionViewAtBeginning()
        autohideTimer.stop()
    }

    function openAudioSync() {
        mode = "audiosync"
        audioSyncRow = "delay"
        autohideTimer.stop()
    }

    function activateAction() {
        const action = actions[Math.max(0, Math.min(actions.length - 1, actionIndex))].value
        if (!hasPlayer)
            return
        if (action === "back") player.seekBack()
        else if (action === "pause") player.togglePause()
        else if (action === "forward") player.seekForward()
        else if (action === "subtitles") openSubtitles()
        else if (action === "audio") openAudio()
        else if (action === "debug") openDebugMenu()
    }

    function activateMenuItem() {
        if (mode === "subtitles") {
            if (hasPlayer) player.selectSubtitle(menuIndex)
            closeMenu()
            return
        }
        if (mode === "audio") {
            if (hasPlayer) player.selectAudio(menuIndex)
            closeMenu()
            return
        }
        if (menuIndex === 0) { openAudioSync(); return }
        else if (menuIndex === 1) toggleDebugStats()
        else if (menuIndex === 2 && appController) appController.setNightModeEnabled(!nightModeEnabled)
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
            appController.setAudioDelayMs(clampAudioDelayMs(value))
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
        if (isAcceptKey(key)) {
            if (audioSyncRow === "delay" && hasPlayer) player.togglePause()
            return true
        }
        return false
    }

    function handleBack() {
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
        if (mode !== "hidden" || hud.opacity > 0.01 || backButton.opacity > 0.01) {
            autohideTimer.stop()
            mode = "hidden"
            row = "timeline"
            return true
        }
        // Hidden controls: back exits playback.
        if (hasPlayer && player.backAllowed)
            player.stopWithReason("overlay-back-key")
        return true
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
            row = row === "back" ? "timeline" : "actions"
            showControls(row)
            return true
        }
        if (key === Qt.Key_Left) {
            if (row === "timeline") previewSeek(-10)
            else if (row === "actions") actionIndex = Math.max(0, actionIndex - 1)
            showControls(row)
            return true
        }
        if (key === Qt.Key_Right) {
            if (row === "timeline") previewSeek(10)
            else if (row === "actions") actionIndex = Math.min(actions.length - 1, actionIndex + 1)
            showControls(row)
            return true
        }
        if (isAcceptKey(key)) {
            if (row === "timeline") {
                if (!commitScrub() && hasPlayer) player.togglePause()
            } else if (row === "back") {
                handleBack()
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
        if (isAcceptKey(key)) { activateMenuItem(); return true }
        return false
    }

    function actionForColorKey(key) {
        if (!appController)
            return ""
        if (key === Qt.Key_Red)    return appController.redButtonAction
        if (key === Qt.Key_Green)  return appController.greenButtonAction
        if (key === Qt.Key_Yellow) return appController.yellowButtonAction
        if (key === Qt.Key_Blue)   return appController.blueButtonAction
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
        if (action === "showInfo") { toggleDebugStats(); return true }
        if (action === "stop") { player.stopWithReason("remap-stop"); return true }
        return false
    }

    function handleReleased(event) {
        if (isIgnoredPlayerNoise(event)) return true
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
        if (event.key === Qt.Key_Red || event.key === Qt.Key_Green
            || event.key === Qt.Key_Yellow || event.key === Qt.Key_Blue) {
            if (dispatchRemapAction(actionForColorKey(event.key))) return true
        }
        if (seekHoldKey !== 0 && event.key === seekHoldKey) {
            if (!event.isAutoRepeat) {
                if (seekHoldActive) {
                    stopPreviewSeekHold()
                } else {
                    const delta = seekHoldDelta
                    stopPreviewSeekHold()
                    previewSeek(delta)
                }
            }
            return true
        }
        const releaseSeekDelta = seekDeltaForKey(event.key)
        if (releaseSeekDelta !== 0 && event.isAutoRepeat)
            return true
        if (isBackEvent(event)) return handleBack()
        if (event.key === Qt.Key_I || event.key === Qt.Key_Info) { toggleDebugStats(); return true }
        if (event.key === Qt.Key_Q && hasPlayer) { player.stopWithReason("player-q"); return true }
        if (mode === "hidden") {
            if (event.key === Qt.Key_Left) { previewSeek(-10); return true }
            if (event.key === Qt.Key_Right) { previewSeek(10); return true }
            if (isAcceptKey(event.key) && hasPlayer) { player.togglePause(); showControls("actions"); return true }
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
        if (isIgnoredPlayerNoise(event)) return true
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
            actionIndex = 0
            showControls("timeline")
        } else {
            autohideTimer.stop()
            scrubTimer.stop()
            stopPreviewSeekHold()
            downHoldTimer.stop()
            downHoldActive = false
            mode = "hidden"
            row = "timeline"
            scrubbing = false
            audioSyncRow = "delay"
        }
    }

    onScrubbingChanged: if (!scrubbing) maybeRestartAutohide()

    Timer { id: autohideTimer; interval: 3000; onTriggered: overlay.hideControls() }
    Timer { id: scrubTimer; interval: 650; onTriggered: overlay.commitScrub() }
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
    // Embedded video surface (desktop / non-Starfish builds). On Starfish the
    // video lives on a separate exported surface so this item is harmless —
    // MpvVideoItem just sits unused. z=-1 keeps it behind the HUD.
    MpvVideoItem {
        anchors.fill: parent
        z: -1
    }

    TapHandler { onTapped: overlay.showControls("timeline") }
    HoverHandler { onHoveredChanged: if (hovered && overlay.mode !== "hidden") overlay.showControls(overlay.row) }

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
        height: Math.round(150 * overlay.uiScale)
        opacity: 0
        visible: opacity > 0.01
        color: "transparent"
    }

    Rectangle {
        id: bottomScrim
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: Math.round(360 * overlay.uiScale)
        opacity: 0
        visible: opacity > 0.01
        color: "transparent"
    }

    Rectangle {
        id: backButton
        readonly property bool focused: overlay.isControlsActive() && overlay.row === "back" && !overlay.isAudioSyncOpen()
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: Math.round(40 * overlay.uiScale)
        width: Math.round(64 * overlay.uiScale)
        height: width
        radius: width / 2
        color: focused ? "#3300A4DC" : "transparent"
        border.width: focused ? 2 : 0
        border.color: "#EAF8FF"
        opacity: 0
        visible: opacity > 0.01

        MaterialIcon {
            anchors.centerIn: parent
            name: "arrow_back"
            iconColor: backButton.focused ? "#FFFFFF" : "#EEEEEE"
            iconSize: Math.round(34 * overlay.uiScale)
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                // The back-button click is an explicit exit gesture; bypass
                // handleBack's progressive-back logic (which only hides the
                // controls overlay on the first invocation when `mode ===
                // "controls"`, leaving the user to click an invisible button
                // for a no-op).
                if (overlay.hasPlayer && overlay.player.backAllowed)
                    overlay.player.stopWithReason("overlay-back-button")
            }
        }
    }

    transitions: Transition {
        NumberAnimation { properties: "opacity"; duration: 140; easing.type: Easing.OutCubic }
    }

    // Trickplay scrubber preview. Shown above the timeline when the user is
    // actively scrubbing or holding seek. The player returns a sprite-sheet
    // tile + offset to display from /Videos/<id>/Trickplay/<width>/<n>.jpg.
    Item {
        id: trickplayPreview
        readonly property bool active: overlay.hasPlayer && overlay.player.trickplayAvailable
                                     && (overlay.scrubbing || overlay.seekHoldActive)
                                     && overlay.mode !== "hidden"
        readonly property var trickplayData: active
            ? overlay.player.trickplayForSeconds(overlay.scrubbing ? overlay.scrubSeconds : overlay.positionSeconds())
            : ({})
        readonly property bool dataReady: trickplayData && trickplayData.available === true

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Math.round(310 * overlay.uiScale)
        height: dataReady ? Math.round((trickplayData.height || 0) * overlay.uiScale * 1.4) : 0
        visible: dataReady
        opacity: visible ? 1 : 0
        z: 22
        Behavior on opacity { NumberAnimation { duration: 100 } }

        Item {
            id: thumbContainer
            readonly property real thumbWidth: trickplayPreview.dataReady ? trickplayPreview.trickplayData.width * overlay.uiScale * 1.4 : 0
            readonly property real thumbHeight: trickplayPreview.dataReady ? trickplayPreview.trickplayData.height * overlay.uiScale * 1.4 : 0
            // Position the thumbnail above the timeline scrubber x.
            x: Math.max(Math.round(52 * overlay.uiScale),
                       Math.min(parent.width - thumbWidth - Math.round(52 * overlay.uiScale),
                                overlay.positionRatio() * parent.width - thumbWidth / 2))
            y: 0
            width: thumbWidth
            height: thumbHeight + Math.round(24 * overlay.uiScale)

            Rectangle {
                id: thumbFrame
                width: thumbContainer.thumbWidth
                height: thumbContainer.thumbHeight
                color: "#000"
                border.color: "#FFFFFF"
                border.width: 2
                radius: 4
                clip: true

                Image {
                    id: thumbSheet
                    source: trickplayPreview.dataReady ? trickplayPreview.trickplayData.url : ""
                    visible: status === Image.Ready
                    // Scale the entire sheet so each tile lines up at the
                    // requested 1.4x. The tile we want lives at (offsetX,
                    // offsetY) within the unscaled sheet.
                    x: trickplayPreview.dataReady ? trickplayPreview.trickplayData.offsetX * overlay.uiScale * 1.4 : 0
                    y: trickplayPreview.dataReady ? trickplayPreview.trickplayData.offsetY * overlay.uiScale * 1.4 : 0
                    width: trickplayPreview.dataReady ? trickplayPreview.trickplayData.sheetWidth * overlay.uiScale * 1.4 : 0
                    height: trickplayPreview.dataReady ? trickplayPreview.trickplayData.sheetHeight * overlay.uiScale * 1.4 : 0
                    fillMode: Image.Stretch
                    cache: true
                    asynchronous: true
                }
            }

            Text {
                anchors.horizontalCenter: thumbFrame.horizontalCenter
                anchors.top: thumbFrame.bottom
                anchors.topMargin: Math.round(4 * overlay.uiScale)
                text: overlay.formatClock(overlay.scrubbing ? overlay.scrubSeconds : overlay.positionSeconds())
                color: "#FFFFFF"
                font.pixelSize: Math.round(16 * overlay.uiScale)
                font.weight: Font.Medium
                font.hintingPreference: Font.PreferNoHinting
                renderType: Text.QtRendering
            }
        }
    }

    // Skip Intro / Skip Outro / Skip Recap card. Visible whenever the player
    // reports an active media segment. The user can press the dedicated focus
    // (right-anchored) button, or press T on a keyboard, or activate from
    // any state by clicking. Auto-dismisses when playback leaves the segment.
    Rectangle {
        id: skipSegmentCard
        readonly property string segmentType: overlay.hasPlayer ? overlay.player.activeSegmentType : ""
        readonly property string label: segmentType === "Intro" ? "Skip Intro"
                                      : segmentType === "Outro" ? "Skip Outro"
                                      : segmentType === "Recap" ? "Skip Recap"
                                      : segmentType === "Preview" ? "Skip Preview"
                                      : segmentType.length > 0 ? "Skip " + segmentType
                                      : ""
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: Math.round(48 * overlay.uiScale)
        anchors.bottomMargin: Math.round(220 * overlay.uiScale)
        width: Math.round(220 * overlay.uiScale)
        height: Math.round(60 * overlay.uiScale)
        radius: Math.round(8 * overlay.uiScale)
        color: "#E000A4DC"
        border.width: 1
        border.color: "#80EAF8FF"
        visible: segmentType.length > 0 && overlay.mode !== "hidden"
        opacity: visible ? 1 : 0
        z: 30

        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Math.round(16 * overlay.uiScale)
            anchors.rightMargin: Math.round(16 * overlay.uiScale)
            spacing: Math.round(10 * overlay.uiScale)

            MaterialIcon {
                name: "skip_next"
                iconColor: "#FFFFFF"
                iconSize: Math.round(24 * overlay.uiScale)
            }

            Text {
                Layout.fillWidth: true
                text: skipSegmentCard.label
                color: "#FFFFFF"
                font.pixelSize: Math.round(18 * overlay.uiScale)
                font.weight: Font.DemiBold
                font.hintingPreference: Font.PreferNoHinting
                renderType: Text.QtRendering
            }

            Text {
                text: "T"
                color: "#FFFFFF"
                font.pixelSize: Math.round(13 * overlay.uiScale)
                font.weight: Font.Medium
                opacity: 0.7
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: if (overlay.hasPlayer) overlay.player.skipActiveSegment()
        }
    }

    Item {
        id: hud
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Math.round(52 * overlay.uiScale)
        height: Math.round(236 * overlay.uiScale)
        visible: opacity > 0.01
        opacity: 0

        ColumnLayout {
            anchors.fill: parent
            spacing: Math.round(16 * overlay.uiScale)

            RowLayout {
                Layout.fillWidth: true
                    spacing: Math.round(20 * overlay.uiScale)

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Math.round(6 * overlay.uiScale)
                    Text {
                        Layout.fillWidth: true
                        text: overlay.hasPlayer ? overlay.player.title : ""
                        color: "#F4F8FA"
                        font.pixelSize: Math.round(40 * overlay.uiScale)
                        font.weight: Font.Bold
                        font.hintingPreference: Font.PreferNoHinting
                        renderType: Text.QtRendering
                        maximumLineCount: 1
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: overlay.hasPlayer ? overlay.player.statusText : ""
                        color: overlay.hasPlayer && (overlay.player.buffering || overlay.player.seeking) ? "#9DE8FF" : "#AAB7BF"
                        font.pixelSize: Math.round(24 * overlay.uiScale)
                        font.weight: Font.Medium
                        font.hintingPreference: Font.PreferNoHinting
                        renderType: Text.QtRendering
                        maximumLineCount: 1
                        elide: Text.ElideRight
                    }
                }

                Text {
                    text: overlay.hasPlayer && overlay.player.paused ? "Paused" : "Playing"
                    color: overlay.hasPlayer && overlay.player.paused ? "#FFFFFF" : "#B8C4CA"
                    font.pixelSize: Math.round(23 * overlay.uiScale)
                    font.weight: Font.DemiBold
                    font.hintingPreference: Font.PreferNoHinting
                    renderType: Text.QtRendering
                }
            }

            Item {
                id: timeline
                Layout.fillWidth: true
                Layout.preferredHeight: Math.round(82 * overlay.uiScale)
                readonly property double ratio: overlay.hasPlayer && overlay.player.durationSeconds > 0 ? Math.max(0, Math.min(1, overlay.positionSeconds() / overlay.player.durationSeconds)) : 0
                readonly property bool focused: overlay.isControlsActive() && overlay.row === "timeline" && !overlay.isAudioSyncOpen()

                Text {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    text: overlay.formatClock(overlay.positionSeconds())
                    color: timeline.focused ? "#FFFFFF" : "#B8C4CA"
                    font.pixelSize: Math.round(23 * overlay.uiScale)
                    font.weight: Font.Medium
                    font.hintingPreference: Font.PreferNoHinting
                    renderType: Text.QtRendering
                }
                Text {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    text: overlay.formatClock(overlay.hasPlayer ? overlay.player.durationSeconds : 0)
                    color: "#B8C4CA"
                    font.pixelSize: Math.round(23 * overlay.uiScale)
                    font.weight: Font.Medium
                    font.hintingPreference: Font.PreferNoHinting
                    renderType: Text.QtRendering
                }
                Rectangle {
                    id: track
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: timeline.focused ? Math.round(16 * overlay.uiScale) : Math.round(10 * overlay.uiScale)
                    radius: height / 2
                    color: "#55606A72"
                    border.width: timeline.focused ? 1 : 0
                    border.color: "#B8DDEB"
                    Behavior on height { NumberAnimation { duration: 100 } }
                }
                Rectangle {
                    anchors.left: track.left
                    anchors.verticalCenter: track.verticalCenter
                    width: Math.max(track.height, track.width * timeline.ratio)
                    height: track.height
                    radius: height / 2
                    color: overlay.hasPlayer && overlay.player.buffering ? "#80DFFF" : "#00A4DC"
                    Behavior on width { enabled: !overlay.scrubbing; NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                }
                Rectangle {
                    x: Math.max(0, Math.min(track.width - width, track.width * timeline.ratio - width / 2))
                    anchors.verticalCenter: track.verticalCenter
                    width: timeline.focused ? Math.round(32 * overlay.uiScale) : Math.round(22 * overlay.uiScale)
                    height: width
                    radius: width / 2
                    color: "#FFFFFF"
                    border.width: 2
                    border.color: overlay.scrubbing ? "#AA5CC3" : "#00A4DC"
                    Behavior on width { NumberAnimation { duration: 100 } }
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
                spacing: Math.round(8 * overlay.uiScale)

                Repeater {
                    model: overlay.actions.length
                    delegate: Rectangle {
                        required property int index
                        readonly property bool focused: overlay.isControlsActive() && overlay.row === "actions" && overlay.actionIndex === index && !overlay.isAudioSyncOpen()
                        readonly property string actionValue: overlay.actions[index].value
                        Layout.preferredWidth: overlay.actionTargetSize
                        Layout.preferredHeight: overlay.actionTargetSize
                        radius: Math.round(overlay.actionTargetSize / 2)
                        color: focused ? "#3300A4DC" : "transparent"
                        border.width: focused ? 2 : 0
                        border.color: "#EAF8FF"

                        MaterialIcon {
                            anchors.centerIn: parent
                            name: overlay.actionIcon(actionValue)
                            iconColor: focused ? "#FFFFFF" : "#EEEEEE"
                            iconSize: actionValue === "debug" ? Math.round(30 * overlay.uiScale) : Math.round(36 * overlay.uiScale)
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
            }

            Text {
                Layout.fillWidth: true
                visible: overlay.hasPlayer && overlay.player.errorText.length > 0
                text: overlay.hasPlayer ? overlay.player.errorText : ""
                color: "#FFB8BD"
                font.pixelSize: Math.round(18 * overlay.uiScale)
                font.hintingPreference: Font.PreferNoHinting
                renderType: Text.QtRendering
                wrapMode: Text.Wrap
            }
        }
    }

    Rectangle {
        id: audioSyncPanel
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(parent.width - Math.round(120 * overlay.uiScale), Math.round(760 * overlay.uiScale))
        height: Math.round(330 * overlay.uiScale)
        visible: opacity > 0.01
        opacity: 0
        radius: Math.round(12 * overlay.uiScale)
        color: "#B00B1116"
        border.width: 1
        border.color: "#668CA5B5"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Math.round(24 * overlay.uiScale)
            spacing: Math.round(18 * overlay.uiScale)

            Text {
                Layout.fillWidth: true
                text: "Audio sync"
                color: "#F4F8FA"
                font.pixelSize: Math.round(25 * overlay.uiScale)
                font.weight: Font.DemiBold
                font.hintingPreference: Font.PreferNoHinting
                renderType: Text.QtRendering
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.round(128 * overlay.uiScale)
                spacing: Math.round(22 * overlay.uiScale)

                Rectangle {
                    Layout.preferredWidth: Math.round(92 * overlay.uiScale)
                    Layout.preferredHeight: Math.round(92 * overlay.uiScale)
                    radius: width / 2
                    color: overlay.audioSyncRow === "delay" ? "#3300A4DC" : "#331A232A"
                    border.width: overlay.audioSyncRow === "delay" ? 3 : 1
                    border.color: overlay.audioSyncRow === "delay" ? "#EAF8FF" : "#56707F"

                    MaterialIcon {
                        anchors.centerIn: parent
                        name: "remove"
                        iconColor: "#FFFFFF"
                        iconSize: Math.round(42 * overlay.uiScale)
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            overlay.audioSyncRow = "delay"
                            overlay.adjustAudioDelay(-1)
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: Math.round(10 * overlay.uiScale)

                    Text {
                        Layout.fillWidth: true
                        text: overlay.formatAudioDelay(overlay.currentAudioDelayMs)
                        color: "#FFFFFF"
                        font.pixelSize: Math.round(58 * overlay.uiScale)
                        font.weight: Font.Bold
                        font.hintingPreference: Font.PreferNoHinting
                        renderType: Text.QtRendering
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.round(26 * overlay.uiScale)

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            height: Math.round(8 * overlay.uiScale)
                            radius: height / 2
                            color: "#55606A72"
                        }
                        Rectangle {
                            anchors.left: parent.horizontalCenter
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.abs(overlay.currentAudioDelayMs) / 2000 * parent.width / 2
                            height: Math.round(12 * overlay.uiScale)
                            radius: height / 2
                            color: "#00A4DC"
                            visible: overlay.currentAudioDelayMs >= 0
                        }
                        Rectangle {
                            anchors.right: parent.horizontalCenter
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.abs(overlay.currentAudioDelayMs) / 2000 * parent.width / 2
                            height: Math.round(12 * overlay.uiScale)
                            radius: height / 2
                            color: "#AA5CC3"
                            visible: overlay.currentAudioDelayMs < 0
                        }
                        Rectangle {
                            x: Math.max(0, Math.min(parent.width - width, ((overlay.currentAudioDelayMs + 2000) / 4000) * parent.width - width / 2))
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.round(24 * overlay.uiScale)
                            height: width
                            radius: width / 2
                            color: "#FFFFFF"
                            border.width: 2
                            border.color: "#00A4DC"
                        }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: Math.round(92 * overlay.uiScale)
                    Layout.preferredHeight: Math.round(92 * overlay.uiScale)
                    radius: width / 2
                    color: overlay.audioSyncRow === "delay" ? "#3300A4DC" : "#331A232A"
                    border.width: overlay.audioSyncRow === "delay" ? 3 : 1
                    border.color: overlay.audioSyncRow === "delay" ? "#EAF8FF" : "#56707F"

                    MaterialIcon {
                        anchors.centerIn: parent
                        name: "add"
                        iconColor: "#FFFFFF"
                        iconSize: Math.round(42 * overlay.uiScale)
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            overlay.audioSyncRow = "delay"
                            overlay.adjustAudioDelay(1)
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.round(62 * overlay.uiScale)
                spacing: Math.round(10 * overlay.uiScale)

                Text {
                    text: "Step"
                    color: overlay.audioSyncRow === "step" ? "#FFFFFF" : "#B8C4CA"
                    font.pixelSize: Math.round(22 * overlay.uiScale)
                    font.weight: Font.DemiBold
                    font.hintingPreference: Font.PreferNoHinting
                    renderType: Text.QtRendering
                    Layout.preferredWidth: Math.round(88 * overlay.uiScale)
                    verticalAlignment: Text.AlignVCenter
                }

                Repeater {
                    model: overlay.audioSyncSteps.length
                    delegate: Rectangle {
                        required property int index
                        readonly property bool selected: overlay.audioSyncStepIndex === index
                        readonly property bool focused: overlay.audioSyncRow === "step" && selected
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.round(52 * overlay.uiScale)
                        radius: Math.round(8 * overlay.uiScale)
                        color: focused ? "#3300A4DC" : selected ? "#243E5360" : "#221A232A"
                        border.width: focused ? 3 : selected ? 2 : 1
                        border.color: focused ? "#EAF8FF" : selected ? "#00A4DC" : "#48606D"

                        Text {
                            anchors.centerIn: parent
                            text: overlay.audioSyncSteps[index] + " ms"
                            color: selected ? "#FFFFFF" : "#C9D0D4"
                            font.pixelSize: Math.round(21 * overlay.uiScale)
                            font.weight: Font.DemiBold
                            font.hintingPreference: Font.PreferNoHinting
                            renderType: Text.QtRendering
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                overlay.audioSyncRow = "step"
                                overlay.audioSyncStepIndex = index
                            }
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: menuPanel
        readonly property real edgeMargin: Math.round(20 * overlay.uiScale)
        x: Math.max(edgeMargin, Math.min(parent.width - width - edgeMargin, overlay.menuAnchorX - width / 2))
        y: Math.max(edgeMargin, Math.min(parent.height - height - edgeMargin, overlay.menuAnchorY - height - Math.round(12 * overlay.uiScale)))
        width: Math.round(Math.min(parent.width - edgeMargin * 2, 360 * overlay.uiScale))
        height: Math.min(Math.round(parent.height * 0.48), Math.round(menuHeader.implicitHeight + menuList.contentHeight + 36 * overlay.uiScale))
        visible: overlay.isMenuOpen()
        opacity: 0
        radius: 8
        color: "#E0101418"
        border.width: 1
        border.color: "#52636E"

        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: Math.round(12 * overlay.uiScale)
            spacing: Math.round(8 * overlay.uiScale)

            Text {
                id: menuHeader
                Layout.fillWidth: true
                text: overlay.mode === "subtitles" ? "Subtitles"
                    : overlay.mode === "audio" ? "Audio"
                    : "Playback Debug"
                color: "#F4F8FA"
                font.pixelSize: Math.round(18 * overlay.uiScale)
                font.weight: Font.DemiBold
                font.hintingPreference: Font.PreferNoHinting
                renderType: Text.QtRendering
            }

            ListView {
                id: menuList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: overlay.mode === "subtitles" && overlay.hasPlayer ? overlay.player.subtitleTracks
                     : overlay.mode === "audio" && overlay.hasPlayer ? overlay.player.audioTracks
                     : overlay.debugOptions
                currentIndex: overlay.menuIndex
                boundsBehavior: Flickable.StopAtBounds
                highlightMoveDuration: 90
                onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    width: menuList.width
                    height: Math.round(40 * overlay.uiScale)
                    radius: 5
                    color: overlay.menuIndex === index ? "#2600A4DC" : "transparent"
                    border.width: overlay.menuIndex === index ? 2 : 0
                    border.color: "#EAF8FF"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Math.round(12 * overlay.uiScale)
                        anchors.rightMargin: Math.round(12 * overlay.uiScale)
                        spacing: Math.round(8 * overlay.uiScale)
                        Text {
                            text: (overlay.mode === "subtitles" && overlay.hasPlayer && overlay.player.selectedSubtitleIndex === index)
                                || (overlay.mode === "audio" && overlay.hasPlayer && overlay.player.selectedAudioIndex === index) ? "✓" : ""
                            color: "#80DFFF"
                            font.pixelSize: Math.round(17 * overlay.uiScale)
                            font.hintingPreference: Font.PreferNoHinting
                            renderType: Text.QtRendering
                            Layout.preferredWidth: Math.round(22 * overlay.uiScale)
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Text {
                            Layout.fillWidth: true
                            text: String(modelData)
                            color: overlay.menuIndex === index ? "#FFFFFF" : "#C9D0D4"
                            font.pixelSize: Math.round(16 * overlay.uiScale)
                            font.weight: Font.Medium
                            font.hintingPreference: Font.PreferNoHinting
                            renderType: Text.QtRendering
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            overlay.menuIndex = index
                            overlay.activateMenuItem()
                        }
                    }
                }
            }
        }
    }
}
