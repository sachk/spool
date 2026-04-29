import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

FocusScope {
    id: overlay
    focus: visible

    property var shell
    property bool mediaInfoVisible: false
    property bool diagnosticsVisible: false

    property string mode: "hidden"       // hidden, controls, subtitles, debug
    property string row: "timeline"      // timeline, actions
    property int actionIndex: 1
    property int menuIndex: 0
    property bool scrubbing: false
    property double scrubSeconds: 0
    property double optimisticSeekSeconds: -1
    readonly property real uiScale: Math.max(0.78, Math.min(1.0, height / 1440))

    readonly property var actions: [
        { label: appController.player.paused ? "Play" : "Pause", value: "pause" },
        { label: "Subtitles", value: "subtitles" },
        { label: "...", value: "debug" }
    ]
    readonly property var debugOptions: [
        appController.player.debugOsdVisible ? "Hide debug stats" : "Show debug stats",
        appController.player.nightModeEnabled ? "Disable night mode" : "Enable night mode",
        "Stop playback"
    ]
    readonly property bool menuOpen: mode === "subtitles" || mode === "debug"
    readonly property bool pinned: appController.player.paused || appController.player.buffering || scrubbing || menuOpen

    function formatClock(seconds) {
        const total = Math.max(0, Math.floor(seconds || 0))
        const hours = Math.floor(total / 3600)
        const minutes = Math.floor((total % 3600) / 60)
        const secs = total % 60
        return hours > 0 ? hours + ":" + String(minutes).padStart(2, "0") + ":" + String(secs).padStart(2, "0") : minutes + ":" + String(secs).padStart(2, "0")
    }

    function clampSeconds(seconds) {
        return Math.max(0, Math.min(appController.player.durationSeconds || 0, seconds || 0))
    }

    function positionSeconds() {
        if (scrubbing) return scrubSeconds
        if (optimisticSeekSeconds >= 0) return optimisticSeekSeconds
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
        optimisticSeekSeconds = clampSeconds(seconds)
        scrubSeconds = optimisticSeekSeconds
        seekCatchupTimer.restart()
        appController.player.seek(optimisticSeekSeconds)
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
        mode = "subtitles"
        menuIndex = 0
        menuList.positionViewAtBeginning()
        autohideTimer.stop()
    }

    function openDebugMenu() {
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
        else if (action === "debug") openDebugMenu()
    }

    function activateMenuItem() {
        if (mode === "subtitles") {
            appController.player.selectSubtitle(menuIndex)
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
        if (mode !== "hidden") return hideControls()
        if (appController.player.backAllowed) appController.player.stopWithReason("overlay-back-key")
        return true
    }

    function handleControlsKey(key) {
        if (key === Qt.Key_Up) { row = "timeline"; showControls(row); return true }
        if (key === Qt.Key_Down) { row = "actions"; showControls(row); return true }
        if (key === Qt.Key_Left) {
            if (row === "timeline") adjustTimeline(-10)
            else actionIndex = Math.max(0, actionIndex - 1)
            showControls(row)
            return true
        }
        if (key === Qt.Key_Right) {
            if (row === "timeline") adjustTimeline(30)
            else actionIndex = Math.min(actions.length - 1, actionIndex + 1)
            showControls(row)
            return true
        }
        if (isAcceptKey(key)) {
            if (row === "timeline") {
                if (!commitScrub()) appController.player.togglePause()
            } else {
                activateAction()
            }
            showControls(row)
            return true
        }
        return false
    }

    function handleMenuKey(key) {
        const count = mode === "subtitles" ? appController.player.subtitleTracks.length : debugOptions.length
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
            seekCatchupTimer.stop()
            mode = "hidden"
            row = "timeline"
            scrubbing = false
            optimisticSeekSeconds = -1
        }
    }

    onPinnedChanged: if (visible && mode !== "hidden") showControls(row)

    Connections {
        target: appController.player
        function onPositionSecondsChanged() {
            if (overlay.optimisticSeekSeconds >= 0 && Math.abs(appController.player.positionSeconds - overlay.optimisticSeekSeconds) < 1.2) {
                overlay.optimisticSeekSeconds = -1
                seekCatchupTimer.stop()
            }
        }
    }

    Timer { id: autohideTimer; interval: 4200; onTriggered: overlay.hideControls() }
    Timer { id: scrubTimer; interval: 650; onTriggered: overlay.commitScrub() }
    Timer { id: seekCatchupTimer; interval: 1800; onTriggered: overlay.optimisticSeekSeconds = -1 }

    TapHandler { onTapped: overlay.showControls("timeline") }
    HoverHandler { onPointChanged: if (overlay.mode !== "hidden") overlay.showControls(overlay.row) }

    states: [
        State { name: "hidden"; when: overlay.mode === "hidden"; PropertyChanges { target: hud; opacity: 0 } },
        State { name: "controls"; when: overlay.mode === "controls"; PropertyChanges { target: hud; opacity: 1 } },
        State {
            name: "subtitles"
            when: overlay.mode === "subtitles"
            PropertyChanges { target: hud; opacity: 1 }
            PropertyChanges { target: menuPanel; opacity: 1 }
        },
        State {
            name: "debug"
            when: overlay.mode === "debug"
            PropertyChanges { target: hud; opacity: 1 }
            PropertyChanges { target: menuPanel; opacity: 1 }
        }
    ]

    transitions: Transition {
        NumberAnimation { properties: "opacity"; duration: 140; easing.type: Easing.OutCubic }
    }

    Item {
        id: hud
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Math.round(36 * overlay.uiScale)
        height: Math.round(136 * overlay.uiScale)
        visible: opacity > 0.01
        opacity: 0

        ColumnLayout {
            anchors.fill: parent
            spacing: Math.round(9 * overlay.uiScale)

            RowLayout {
                Layout.fillWidth: true
                spacing: Math.round(14 * overlay.uiScale)

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3
                    Text {
                        Layout.fillWidth: true
                        text: appController.player.title
                        color: "#F4F8FA"
                        font.pixelSize: Math.round(24 * overlay.uiScale)
                        font.weight: Font.DemiBold
                        maximumLineCount: 1
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: appController.player.statusText
                        color: appController.player.buffering || appController.player.seeking ? "#9DE8FF" : "#AAB7BF"
                        font.pixelSize: Math.round(15 * overlay.uiScale)
                        maximumLineCount: 1
                        elide: Text.ElideRight
                    }
                }

                Text {
                    text: appController.player.paused ? "Paused" : "Playing"
                    color: appController.player.paused ? "#FFFFFF" : "#B8C4CA"
                    font.pixelSize: Math.round(15 * overlay.uiScale)
                    font.weight: Font.Medium
                }
            }

            Item {
                id: timeline
                Layout.fillWidth: true
                Layout.preferredHeight: Math.round(42 * overlay.uiScale)
                readonly property double ratio: appController.player.durationSeconds > 0 ? Math.max(0, Math.min(1, overlay.positionSeconds() / appController.player.durationSeconds)) : 0
                readonly property bool focused: overlay.mode === "controls" && overlay.row === "timeline"

                Text {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    text: overlay.formatClock(overlay.positionSeconds())
                    color: timeline.focused ? "#FFFFFF" : "#B8C4CA"
                    font.pixelSize: Math.round(14 * overlay.uiScale)
                }
                Text {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    text: overlay.formatClock(appController.player.durationSeconds)
                    color: "#B8C4CA"
                    font.pixelSize: Math.round(14 * overlay.uiScale)
                }
                Rectangle {
                    id: track
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: timeline.focused ? Math.round(7 * overlay.uiScale) : Math.round(4 * overlay.uiScale)
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
                    width: timeline.focused ? Math.round(15 * overlay.uiScale) : Math.round(9 * overlay.uiScale)
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
                        Layout.preferredWidth: Math.round(42 * overlay.uiScale)
                        Layout.preferredHeight: Math.round(34 * overlay.uiScale)
                        radius: 6
                        color: focused ? "#2400A4DC" : "transparent"
                        border.width: focused ? 2 : 1
                        border.color: focused ? "#EAF8FF" : "#66717A82"

                        Item {
                            anchors.centerIn: parent
                            width: Math.round(18 * overlay.uiScale)
                            height: Math.round(18 * overlay.uiScale)

                            readonly property color iconColor: parent.focused ? "#FFFFFF" : "#C9D0D4"

                            Text {
                                anchors.centerIn: parent
                                visible: parent.parent.actionValue === "pause" && appController.player.paused
                                text: "▶"
                                color: parent.iconColor
                                font.pixelSize: Math.round(16 * overlay.uiScale)
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            Row {
                                anchors.centerIn: parent
                                visible: parent.parent.actionValue === "pause" && !appController.player.paused
                                spacing: Math.round(4 * overlay.uiScale)
                                Repeater {
                                    model: 2
                                    Rectangle {
                                        width: Math.round(4 * overlay.uiScale)
                                        height: Math.round(13 * overlay.uiScale)
                                        radius: 1
                                        color: parent.parent.iconColor
                                    }
                                }
                            }
                            Rectangle {
                                visible: parent.parent.actionValue === "subtitles"
                                anchors.centerIn: parent
                                width: Math.round(16 * overlay.uiScale)
                                height: Math.round(12 * overlay.uiScale)
                                radius: 1
                                color: "transparent"
                                border.width: 1
                                border.color: parent.iconColor
                                Column {
                                    anchors.centerIn: parent
                                    spacing: Math.round(2 * overlay.uiScale)
                                    Rectangle { width: Math.round(9 * overlay.uiScale); height: 1; color: parent.parent.parent.iconColor }
                                    Rectangle { width: Math.round(12 * overlay.uiScale); height: 1; color: parent.parent.parent.iconColor }
                                }
                            }
                            Row {
                                anchors.centerIn: parent
                                visible: parent.parent.actionValue === "debug"
                                spacing: Math.round(3 * overlay.uiScale)
                                Repeater {
                                    model: 3
                                    Rectangle {
                                        width: Math.round(4 * overlay.uiScale)
                                        height: width
                                        radius: width / 2
                                        color: parent.parent.iconColor
                                    }
                                }
                            }
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

                Text {
                    text: "↑/↓ rows   ←/→ seek/select   OK apply   Back hide"
                    color: "#94A0A7"
                    font.pixelSize: Math.round(12 * overlay.uiScale)
                }
            }

            Text {
                Layout.fillWidth: true
                visible: appController.player.errorText.length > 0
                text: appController.player.errorText
                color: "#FFB8BD"
                font.pixelSize: Math.round(14 * overlay.uiScale)
                wrapMode: Text.Wrap
            }
        }
    }

    Rectangle {
        id: menuPanel
        anchors.right: parent.right
        anchors.bottom: hud.top
        anchors.rightMargin: Math.round(36 * overlay.uiScale)
        anchors.bottomMargin: Math.round(16 * overlay.uiScale)
        width: Math.round(360 * overlay.uiScale)
        height: Math.min(Math.round(parent.height * 0.56), Math.round((menuHeader.implicitHeight + menuList.contentHeight + 38) * overlay.uiScale))
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
                text: overlay.mode === "subtitles" ? "Subtitles" : "Playback Debug"
                color: "#F4F8FA"
                font.pixelSize: Math.round(16 * overlay.uiScale)
                font.weight: Font.DemiBold
            }

            ListView {
                id: menuList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: overlay.mode === "subtitles" ? appController.player.subtitleTracks : overlay.debugOptions
                currentIndex: overlay.menuIndex
                boundsBehavior: Flickable.StopAtBounds
                highlightMoveDuration: 90
                onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    width: menuList.width
                    height: Math.round(34 * overlay.uiScale)
                    radius: 5
                    color: overlay.menuIndex === index ? "#2600A4DC" : "transparent"
                    border.width: overlay.menuIndex === index ? 2 : 0
                    border.color: "#EAF8FF"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10
                        Text {
                            text: overlay.mode === "subtitles" && appController.player.selectedSubtitleIndex === index ? "✓" : ""
                            color: "#80DFFF"
                            font.pixelSize: Math.round(14 * overlay.uiScale)
                            Layout.preferredWidth: Math.round(20 * overlay.uiScale)
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Text {
                            Layout.fillWidth: true
                            text: String(modelData)
                            color: overlay.menuIndex === index ? "#FFFFFF" : "#C9D0D4"
                            font.pixelSize: Math.round(14 * overlay.uiScale)
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
