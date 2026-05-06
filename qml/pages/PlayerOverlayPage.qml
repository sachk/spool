import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import JellyfinWebOS

FocusScope {
    id: overlay
    focus: visible
    onActiveFocusChanged: if (visible && !activeFocus) forceActiveFocus()

    property var shell
    property bool mediaInfoVisible: false
    property bool diagnosticsVisible: false

    property string mode: "hidden"       // hidden, controls, subtitles, audio, debug
    property string row: "timeline"      // back, timeline, actions
    property int actionIndex: 1
    property int menuIndex: 0
    property bool scrubbing: false
    property double scrubSeconds: 0
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
    readonly property var debugOptions: [
        appController.player.debugOsdVisible ? "Hide debug stats" : "Show debug stats",
        appController.player.nightModeEnabled ? "Disable night mode" : "Enable night mode",
        "Stop playback"
    ]
    readonly property bool menuOpen: mode === "subtitles" || mode === "audio" || mode === "debug"
    readonly property bool pinned: appController.player.paused || scrubbing || menuOpen

    function formatClock(seconds) {
        const total = Math.max(0, Math.floor(seconds || 0))
        const hours = Math.floor(total / 3600)
        const minutes = Math.floor((total % 3600) / 60)
        const secs = total % 60
        return hours > 0 ? hours + ":" + String(minutes).padStart(2, "0") + ":" + String(secs).padStart(2, "0") : minutes + ":" + String(secs).padStart(2, "0")
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
        return Math.max(0, Math.min(appController.player.durationSeconds || 0, seconds || 0))
    }

    function positionSeconds() {
        if (scrubbing) return scrubSeconds
        return appController.player.positionSeconds
    }

    function showControls(preferredRow) {
        if (mode === "hidden")
            row = preferredRow || "timeline"
        if (!menuOpen)
            mode = "controls"
        if (pinned) autohideTimer.stop()
        else autohideTimer.restart()
    }

    function hideControls() {
        if (pinned)
            return false
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

    function commitScrub() {
        if (!scrubbing)
            return false
        seekTo(scrubSeconds)
        scrubbing = false
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
        if (menuIndex === 0) appController.player.toggleDebugOsd()
        else if (menuIndex === 1) appController.player.setNightModeEnabled(!appController.player.nightModeEnabled)
        else if (menuIndex === 2) appController.player.stopWithReason("debug-menu-stop")
        if (mode === "debug") closeMenu()
    }

    function handleBack() {
        if (scrubbing) { scrubbing = false; showControls("timeline"); return true }
        if (menuOpen) { closeMenu(); return true }
        if (mode !== "hidden" && !pinned) { hideControls(); return true }
        // Hidden, paused, buffering, or any pinned state: back exits playback.
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
            row = row === "back" ? "timeline" : "actions"
            showControls(row)
            return true
        }
        if (key === Qt.Key_Left) {
            if (row === "timeline") appController.player.seekBack()
            else if (row === "actions") actionIndex = Math.max(0, actionIndex - 1)
            showControls(row)
            return true
        }
        if (key === Qt.Key_Right) {
            if (row === "timeline") appController.player.seekForward()
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
        if (isBackEvent(event)) return handleBack()
        if (mode === "hidden") {
            if (event.key === Qt.Key_Left) { appController.player.seekBack(); showControls("timeline"); return true }
            if (event.key === Qt.Key_Right) { appController.player.seekForward(); showControls("timeline"); return true }
            if (isAcceptKey(event.key)) { appController.player.togglePause(); showControls("actions"); return true }
            showControls("timeline")
            return true
        }
        if (menuOpen && handleMenuKey(event.key)) return true
        if (handleControlsKey(event.key)) return true
        if (event.key === Qt.Key_I || event.key === Qt.Key_Info) { appController.player.toggleDebugOsd(); showControls(row); return true }
        if (event.key === Qt.Key_S) { openSubtitles(); return true }
        if (event.key === Qt.Key_A) { openAudio(); return true }
        if (event.key === Qt.Key_Q) { appController.player.stopWithReason("player-q"); return true }
        return false
    }

    Keys.onReleased: (event) => {
        if (handleReleased(event))
            event.accepted = true
    }

    onVisibleChanged: {
        if (visible) {
            mode = "controls"
            row = "timeline"
            actionIndex = 0
            forceActiveFocus()
            showControls("timeline")
        } else {
            autohideTimer.stop()
            scrubTimer.stop()
            mode = "hidden"
            row = "timeline"
            scrubbing = false
        }
    }

    onPinnedChanged: if (visible && mode !== "hidden") showControls(row)

    Timer { id: autohideTimer; interval: 5000; onTriggered: overlay.hideControls() }
    Timer { id: scrubTimer; interval: 650; onTriggered: overlay.commitScrub() }

    // Embedded video surface (desktop / non-Starfish builds). On Starfish the
    // video lives on a separate exported surface so this item is harmless —
    // MpvVideoItem just sits unused. z=-1 keeps it behind the HUD.
    MpvVideoItem {
        anchors.fill: parent
        z: -1
        rotation: 180
    }

    TapHandler { onTapped: overlay.showControls("timeline") }
    HoverHandler { onPointChanged: if (overlay.mode !== "hidden") overlay.showControls(overlay.row) }

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
        readonly property bool focused: overlay.mode === "controls" && overlay.row === "back"
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
                overlay.mode = "controls"
                overlay.row = "back"
                overlay.handleBack()
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
                readonly property bool focused: overlay.mode === "controls" && overlay.row === "timeline"

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
                        readonly property bool focused: overlay.mode === "controls" && overlay.row === "actions" && overlay.actionIndex === index
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
        id: menuPanel
        readonly property real edgeMargin: Math.round(20 * overlay.uiScale)
        x: Math.max(edgeMargin, Math.min(parent.width - width - edgeMargin, overlay.menuAnchorX - width / 2))
        y: Math.max(edgeMargin, Math.min(parent.height - height - edgeMargin, overlay.menuAnchorY - height - Math.round(12 * overlay.uiScale)))
        width: Math.round(Math.min(parent.width - edgeMargin * 2, 360 * overlay.uiScale))
        height: Math.min(Math.round(parent.height * 0.48), Math.round(menuHeader.implicitHeight + menuList.contentHeight + 36 * overlay.uiScale))
        visible: overlay.menuOpen
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
