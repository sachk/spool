import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import JellyfinWebOS

FocusScope {
    id: overlay
    focus: false

    property var shell
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
    property int seekHoldReleaseKey: 0
    property double seekHoldDelta: 0
    property double seekHoldElapsedMs: 0
    property double seekHoldStartMs: 0
    readonly property real uiScale: Math.max(0.78, Math.min(1.0, height / 1440))
    readonly property int actionTargetSize: Math.round(64 * uiScale)
    property real menuAnchorX: width - Math.round(240 * uiScale)
    property real menuAnchorY: height - Math.round(170 * uiScale)

    readonly property var actions: [
        { label: "Rewind", value: "back" },
        { label: appController.player.paused ? "Play" : "Pause", value: "pause" },
        { label: "Fast forward", value: "forward" },
        { label: "Subtitles", value: "subtitles" },
        { label: "Audio", value: "audio" },
        { label: "Settings", value: "debug" }
    ]
    readonly property var audioSyncSteps: [1, 5, 10, 100]
    readonly property var debugOptions: [
        "Audio sync",
        appController.player.debugOsdVisible ? "Hide debug stats" : "Show debug stats",
        appController.nightModeEnabled ? "Disable night mode" : "Enable night mode",
        "Stop playback"
    ]
    function isMenuOpen() {
        return mode === "subtitles" || mode === "audio" || mode === "debug"
    }

    function isAudioSyncOpen() {
        return mode === "audiosync"
    }

    function isPinned() {
        return appController.player.paused || scrubbing || isMenuOpen() || isAudioSyncOpen()
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
        if (value === "pause") return appController.player.paused ? "play_arrow" : "pause"
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
        const duration = appController.player.durationSeconds || 0
        return duration > 0 ? Math.min(duration, value) : value
    }

    function positionSeconds() {
        if (scrubbing) return scrubSeconds
        return appController.player.positionSeconds
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
        appController.player.seek(clampSeconds(seconds))
    }

    function adjustTimeline(delta) {
        showControls("timeline")
        row = "timeline"
        scrubbing = true
        scrubSeconds = clampSeconds(positionSeconds() + delta)
        scrubTimer.restart()
    }

    function sendPreviewSeek(delta) {
        showControls("timeline")
        row = "timeline"
        scrubTimer.stop()
        scrubbing = false
        appController.player.previewSeekBy(delta)
    }

    function previewSeek(delta) {
        appController.player.beginPreviewSeek()
        sendPreviewSeek(delta)
        appController.player.endPreviewSeek()
    }

    function seekHoldStepSeconds() {
        if (seekHoldDelta < 0)
            return -10
        if (seekHoldElapsedMs >= 6000)
            return 60
        if (seekHoldElapsedMs >= 4000)
            return 30
        return 10
    }

    function seekHoldIntervalMs() {
        const elapsed = Math.max(0, Math.min(1000, seekHoldElapsedMs))
        return Math.round(200 - elapsed * 0.1)
    }

    function scheduleSeekHoldTick() {
        seekHoldTimer.interval = seekHoldIntervalMs()
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
        if (!canPreviewSeekFromCurrentMode())
            return false
        seekHoldReleaseTimer.stop()
        seekHoldReleaseKey = 0
        if (seekHoldKey === key)
            return true
        stopPreviewSeekHold()
        seekHoldKey = key
        seekHoldDelta = delta
        seekHoldStartMs = Date.now()
        seekHoldElapsedMs = 0
        appController.player.beginPreviewSeek()
        sendPreviewSeek(seekHoldStepSeconds())
        scheduleSeekHoldTick()
        return true
    }

    function stopPreviewSeekHold() {
        seekHoldReleaseTimer.stop()
        seekHoldTimer.stop()
        if (seekHoldKey !== 0)
            appController.player.endPreviewSeek()
        seekHoldKey = 0
        seekHoldReleaseKey = 0
        seekHoldDelta = 0
        seekHoldElapsedMs = 0
        seekHoldStartMs = 0
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
        menuIndex = Math.max(0, appController.player.selectedAudioIndex)
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
        if (action === "back") appController.player.seekBack()
        else if (action === "pause") appController.player.togglePause()
        else if (action === "forward") appController.player.seekForward()
        else if (action === "subtitles") openSubtitles()
        else if (action === "audio") openAudio()
        else if (action === "debug") openDebugMenu()
    }

    function activateMenuItem() {
        if (mode === "subtitles") {
            appController.player.selectSubtitle(menuIndex)
            closeMenu()
            return
        }
        if (mode === "audio") {
            appController.player.selectAudio(menuIndex)
            closeMenu()
            return
        }
        if (menuIndex === 0) { openAudioSync(); return }
        else if (menuIndex === 1) toggleDebugStats()
        else if (menuIndex === 2) appController.setNightModeEnabled(!appController.nightModeEnabled)
        else if (menuIndex === 3) appController.player.stopWithReason("debug-menu-stop")
        if (mode === "debug") closeMenu()
    }

    function toggleDebugStats() {
        const showing = !appController.player.debugOsdVisible
        appController.player.toggleDebugOsd()
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
        appController.setAudioDelayMs(clampAudioDelayMs(value))
    }

    function adjustAudioDelay(direction) {
        const step = audioSyncSteps[Math.max(0, Math.min(audioSyncSteps.length - 1, audioSyncStepIndex))]
        setAudioDelayMs(appController.audioDelayMs + direction * step)
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
            if (audioSyncRow === "delay") appController.player.togglePause()
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
        if (appController.player.backAllowed)
            appController.player.stopWithReason("overlay-back-key")
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
                if (!commitScrub()) appController.player.togglePause()
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
        const count = mode === "subtitles" ? appController.player.subtitleTracks.length
                    : mode === "audio" ? appController.player.audioTracks.length
                    : debugOptions.length
        if (key === Qt.Key_Up) { menuIndex = Math.max(0, menuIndex - 1); return true }
        if (key === Qt.Key_Down) { menuIndex = Math.min(Math.max(0, count - 1), menuIndex + 1); return true }
        if (key === Qt.Key_Left || key === Qt.Key_Right) return true
        if (isAcceptKey(key)) { activateMenuItem(); return true }
        return false
    }

    function handleReleased(event) {
        if (isIgnoredPlayerNoise(event)) return true
        if (seekHoldKey !== 0 && event.key === seekHoldKey) {
            if (!event.isAutoRepeat) {
                seekHoldReleaseKey = event.key
                seekHoldReleaseTimer.restart()
            }
            return true
        }
        const releaseSeekDelta = seekDeltaForKey(event.key)
        if (releaseSeekDelta !== 0 && event.isAutoRepeat)
            return startPreviewSeekHold(event.key, releaseSeekDelta)
        if (isBackEvent(event)) return handleBack()
        if (event.key === Qt.Key_I || event.key === Qt.Key_Info) { toggleDebugStats(); return true }
        if (event.key === Qt.Key_Q) { appController.player.stopWithReason("player-q"); return true }
        if (mode === "hidden") {
            if (event.key === Qt.Key_Left) { previewSeek(-10); return true }
            if (event.key === Qt.Key_Right) { previewSeek(10); return true }
            if (isAcceptKey(event.key)) { appController.player.togglePause(); showControls("actions"); return true }
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
        id: seekHoldTimer
        interval: 200
        repeat: false
        onTriggered: {
            if (overlay.seekHoldKey === 0) {
                stop()
                return
            }
            overlay.seekHoldElapsedMs = Math.max(0, Date.now() - overlay.seekHoldStartMs)
            overlay.sendPreviewSeek(overlay.seekHoldStepSeconds())
            overlay.scheduleSeekHoldTick()
        }
    }
    Timer {
        id: seekHoldReleaseTimer
        interval: 260
        repeat: false
        onTriggered: {
            if (overlay.seekHoldKey !== 0 && overlay.seekHoldKey === overlay.seekHoldReleaseKey)
                overlay.stopPreviewSeekHold()
            overlay.seekHoldReleaseKey = 0
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
                if (appController.player.backAllowed)
                    appController.player.stopWithReason("overlay-back-button")
            }
        }
    }

    transitions: Transition {
        NumberAnimation { properties: "opacity"; duration: 140; easing.type: Easing.OutCubic }
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
                        text: appController.player.title
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
                        text: appController.player.statusText
                        color: appController.player.buffering || appController.player.seeking ? "#9DE8FF" : "#AAB7BF"
                        font.pixelSize: Math.round(24 * overlay.uiScale)
                        font.weight: Font.Medium
                        font.hintingPreference: Font.PreferNoHinting
                        renderType: Text.QtRendering
                        maximumLineCount: 1
                        elide: Text.ElideRight
                    }
                }

                Text {
                    text: appController.player.paused ? "Paused" : "Playing"
                    color: appController.player.paused ? "#FFFFFF" : "#B8C4CA"
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
                readonly property double ratio: appController.player.durationSeconds > 0 ? Math.max(0, Math.min(1, overlay.positionSeconds() / appController.player.durationSeconds)) : 0
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
                    text: overlay.formatClock(appController.player.durationSeconds)
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
                    color: appController.player.buffering ? "#80DFFF" : "#00A4DC"
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
                        overlay.scrubSeconds = overlay.clampSeconds((mouse.x / Math.max(1, width)) * appController.player.durationSeconds)
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
                visible: appController.player.errorText.length > 0
                text: appController.player.errorText
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
                        text: overlay.formatAudioDelay(appController.audioDelayMs)
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
                            width: Math.abs(appController.audioDelayMs) / 2000 * parent.width / 2
                            height: Math.round(12 * overlay.uiScale)
                            radius: height / 2
                            color: "#00A4DC"
                            visible: appController.audioDelayMs >= 0
                        }
                        Rectangle {
                            anchors.right: parent.horizontalCenter
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.abs(appController.audioDelayMs) / 2000 * parent.width / 2
                            height: Math.round(12 * overlay.uiScale)
                            radius: height / 2
                            color: "#AA5CC3"
                            visible: appController.audioDelayMs < 0
                        }
                        Rectangle {
                            x: Math.max(0, Math.min(parent.width - width, ((appController.audioDelayMs + 2000) / 4000) * parent.width - width / 2))
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
                model: overlay.mode === "subtitles" ? appController.player.subtitleTracks
                     : overlay.mode === "audio" ? appController.player.audioTracks
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
                            text: (overlay.mode === "subtitles" && appController.player.selectedSubtitleIndex === index)
                                || (overlay.mode === "audio" && appController.player.selectedAudioIndex === index) ? "✓" : ""
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
