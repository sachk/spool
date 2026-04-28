import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

FocusScope {
    id: overlay
    focus: visible

    property string osdState: "shown" // shown, closing, hidden
    readonly property bool sheetOpened: subtitleSheet.opened || menuPopup.opened
    property bool scrubbing: false
    property double scrubSeconds: 0
    property double optimisticSeekSeconds: -1
    property string focusZone: "overlay"
    property int subtitleFocusIndex: 0
    property int menuFocusIndex: 0

    readonly property bool forceVisible: scrubbing
                                         || appController.player.paused
                                         || appController.player.buffering
                                         || sheetOpened

    property double controlsOpacity: 1.0

    function formatClock(seconds) {
        const total = Math.max(0, Math.floor(seconds || 0))
        const hours = Math.floor(total / 3600)
        const minutes = Math.floor((total % 3600) / 60)
        const secs = total % 60
        return hours > 0
                ? hours + ":" + String(minutes).padStart(2, "0") + ":" + String(secs).padStart(2, "0")
                : minutes + ":" + String(secs).padStart(2, "0")
    }

    function displaySeconds() {
        if (scrubbing)
            return scrubSeconds
        if (optimisticSeekSeconds >= 0)
            return optimisticSeekSeconds
        return appController.player.positionSeconds
    }

    function clampSeconds(seconds) {
        return Math.max(0, Math.min(appController.player.durationSeconds || 0, seconds || 0))
    }

    function setOptimisticSeek(seconds) {
        optimisticSeekSeconds = clampSeconds(seconds)
        seekCatchupTimer.restart()
    }

    function seekTo(seconds) {
        const target = clampSeconds(seconds)
        setOptimisticSeek(target)
        appController.player.seek(target)
    }

    function seekRelative(delta) {
        seekTo(displaySeconds() + delta)
    }

    onOptimisticSeekSecondsChanged: {
        if (optimisticSeekSeconds >= 0 && Math.abs(appController.player.positionSeconds - optimisticSeekSeconds) < 1.2) {
            optimisticSeekSeconds = -1
            seekCatchupTimer.stop()
        }
    }

    Connections {
        target: appController.player
        function onPositionSecondsChanged() {
            if (overlay.optimisticSeekSeconds >= 0 &&
                    Math.abs(appController.player.positionSeconds - overlay.optimisticSeekSeconds) < 1.2) {
                overlay.optimisticSeekSeconds = -1
                seekCatchupTimer.stop()
            }
        }
    }

    Timer {
        id: seekCatchupTimer
        interval: 1800
        onTriggered: overlay.optimisticSeekSeconds = -1
    }

    function isBackEvent(event) {
        const scanCode = Number(event.nativeScanCode || 0)
        return event.key === Qt.Key_Back
                || event.key === Qt.Key_Escape
                || event.key === Qt.Key_BrowserBack
                || event.key === 0x01200003
                || (event.key === 0 && scanCode === 420)
    }

    function isIgnoredPlayerNoise(event) {
        const scanCode = Number(event.nativeScanCode || 0)
        return event.key === 0 && (scanCode === 1206 || scanCode === 1207)
    }

    function isAcceptKey(key) {
        return key === Qt.Key_Return || key === Qt.Key_Enter ||
               key === Qt.Key_Space || key === Qt.Key_Select
    }

    NumberAnimation {
        id: osdAnim
        target: overlay
        property: "controlsOpacity"
        easing.type: Easing.OutCubic
        onStopped: {
            if (overlay.osdState === "closing" && overlay.controlsOpacity <= 0.01) {
                overlay.controlsOpacity = 0
                overlay.osdState = "hidden"
            }
        }
    }

    function canHide() {
        return !forceVisible
    }

    function showControls(reason) {
        if (reason === "pointer-move" && osdState !== "shown")
            return
        osdAnim.stop()
        osdState = "shown"
        osdAnim.to = 1.0
        osdAnim.duration = 160
        osdAnim.start()
        if (!forceVisible)
            autohideTimer.restart()
        else
            autohideTimer.stop()
    }

    function startHide(reason) {
        if (!canHide())
            return false
        autohideTimer.stop()
        osdAnim.stop()
        osdState = "closing"
        osdAnim.to = 0.0
        osdAnim.duration = reason === "back" ? 180 : 420
        osdAnim.start()
        return true
    }

    function accelerateHide() {
        if (!canHide())
            return false
        autohideTimer.stop()
        osdAnim.stop()
        osdState = "closing"
        osdAnim.to = 0.0
        osdAnim.duration = 70
        osdAnim.start()
        return true
    }

    function hide(quick) {
        return quick ? accelerateHide() : startHide("manual")
    }

    function reveal() {
        showControls("input")
    }

    function handleBack() {
        if (menuPopup.opened) { menuPopup.close(); return true }
        if (subtitleSheet.opened) { subtitleSheet.close(); return true }
        if (scrubbing) { scrubbing = false; return true }
        if (osdState === "shown")
            return startHide("back")
        if (osdState === "closing")
            return accelerateHide()
        return false
    }

    function handleAccept() {
        if (ensureShownForKey())
            return true
        if (subtitleSheet.opened) {
            appController.player.selectSubtitle(subtitleFocusIndex)
            subtitleSheet.close()
            return true
        }
        if (menuPopup.opened) {
            activateMenuAction(menuFocusIndex)
            return true
        }
        if (timelineFocus.activeFocus) {
            focusControls()
            return true
        }
        if (pauseButton.activeFocus) {
            appController.player.togglePause()
            return true
        }
        if (subtitlesButton.activeFocus) {
            openSubtitleSheet()
            return true
        }
        if (menuButton.activeFocus) {
            menuPopup.opened ? menuPopup.close() : openMenu()
            return true
        }
        if (overlay.activeFocus) {
            appController.player.togglePause()
            return true
        }
        return true
    }

    function openSubtitleSheet() {
        subtitleFocusIndex = Math.max(0, appController.player.selectedSubtitleIndex)
        subtitleSheet.open()
    }

    function handleShortcutKey(key) {
        if (key === Qt.Key_S) {
            showControls("key")
            openSubtitleSheet()
            return true
        }
        if (key === Qt.Key_I) {
            showControls("key")
            appController.player.toggleDebugOsd()
            return true
        }
        if (key === Qt.Key_Info || key === Qt.Key_Menu) {
            showControls("key")
            openMenu()
            return true
        }
        return false
    }

    function openMenu() {
        menuFocusIndex = 0
        menuPopup.open()
    }

    function activateMenuAction(index) {
        if (index === 0) {
            appController.player.toggleDebugOsd()
            menuPopup.close()
        } else if (index === 1) {
            menuPopup.close()
            openSubtitleSheet()
        } else if (index === 2) {
            menuPopup.close()
            overlay.hide(false)
        } else if (index === 3) {
            menuPopup.close()
            if (appController.player.backAllowed)
                appController.player.stopWithReason("overlay-menu-stop")
        }
    }

    function controlsButtonFocused() {
        return pauseButton.activeFocus || subtitlesButton.activeFocus || menuButton.activeFocus
    }

    function ensureShownForKey() {
        if (osdState !== "shown") {
            showControls("key")
            overlay.forceActiveFocus()
            return true
        }
        return false
    }

    function focusTimeline() {
        showControls("key")
        focusZone = "timeline"
        timelineFocus.forceActiveFocus()
    }

    function focusControls() {
        showControls("key")
        focusZone = "controls"
        pauseButton.forceActiveFocus()
    }

    function focusOverlay() {
        showControls("key")
        focusZone = "overlay"
        overlay.forceActiveFocus()
    }

    function handleDirectionalKey(key) {
        if (ensureShownForKey()) {
            return true
        }

        if (subtitleSheet.opened) {
            if (key === Qt.Key_Down) {
                subtitleFocusIndex = Math.min(appController.player.subtitleTracks.length - 1,
                                              subtitleFocusIndex + 1)
                return true
            }
            if (key === Qt.Key_Up) {
                subtitleFocusIndex = Math.max(0, subtitleFocusIndex - 1)
                return true
            }
            return key === Qt.Key_Left || key === Qt.Key_Right
        }

        if (menuPopup.opened) {
            if (key === Qt.Key_Down) {
                menuFocusIndex = Math.min(3, menuFocusIndex + 1)
                return true
            }
            if (key === Qt.Key_Up) {
                menuFocusIndex = Math.max(0, menuFocusIndex - 1)
                return true
            }
            return key === Qt.Key_Left || key === Qt.Key_Right
        }

        if (key === Qt.Key_Down) {
            if (timelineFocus.activeFocus)
                focusControls()
            else
                focusTimeline()
            return true
        }
        if (key === Qt.Key_Up) {
            if (controlsButtonFocused())
                focusTimeline()
            else
                focusOverlay()
            return true
        }
        if (key === Qt.Key_Left || key === Qt.Key_Right) {
            if (timelineFocus.activeFocus) {
                if (key === Qt.Key_Left)
                    seekRelative(-10)
                else
                    seekRelative(30)
                return true
            }
            if (!controlsButtonFocused()) {
                focusTimeline()
                return true
            }
        }
        return false
    }

    function handleDirectional(event) {
        event.accepted = handleDirectionalKey(event.key)
        return event.accepted
    }

    onForceVisibleChanged: {
        if (forceVisible) {
            showControls("force")
            autohideTimer.stop()
        } else if (osdState === "shown") {
            autohideTimer.restart()
        }
    }

    Timer {
        id: autohideTimer
        interval: 6000
        onTriggered: overlay.startHide("autohide")
    }

    onVisibleChanged: {
        if (visible) {
            osdAnim.stop()
            controlsOpacity = 0.0
            showControls("visible")
            forceActiveFocus()
        } else {
            osdAnim.stop()
            autohideTimer.stop()
            osdState = "hidden"
            controlsOpacity = 0.0
            optimisticSeekSeconds = -1
            subtitleSheet.close()
            menuPopup.close()
        }
    }

    TapHandler { onTapped: overlay.showControls("tap") }
    HoverHandler { onPointChanged: overlay.showControls("pointer-move") }

    Keys.onPressed: (event) => {
        if (overlay.isIgnoredPlayerNoise(event)) {
            event.accepted = true
            return
        }
        if (overlay.isBackEvent(event)) {
            event.accepted = true
            return
        }
        if (handleDirectional(event))
            return
    }

    Keys.onReleased: (event) => {
        if (overlay.isIgnoredPlayerNoise(event)) {
            event.accepted = true
            return
        }

        if (overlay.isBackEvent(event)) {
            if (overlay.handleBack())
                event.accepted = true
            return
        }

        if (isAcceptKey(event.key)) {
            event.accepted = handleAccept()
        } else if (handleShortcutKey(event.key)) {
            event.accepted = true
        }
    }

    component PillButton: Button {
        id: pill
        property color accent: "#e8f7ff"
        property color baseColor: "#1c2832dd"
        property color hoverColor: "#2f4658ee"
        implicitHeight: 52
        padding: 18
        focusPolicy: Qt.StrongFocus
        font.pixelSize: 21
        font.weight: Font.DemiBold
        contentItem: Label {
            text: pill.text
            color: pill.accent
            font: pill.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 26
            color: pill.down || pill.hovered || pill.activeFocus ? pill.hoverColor : pill.baseColor
            border.width: 1
            border.color: pill.down || pill.hovered || pill.activeFocus ? Qt.lighter(pill.accent, 1.2) : "#3d566a"
            Behavior on color { ColorAnimation { duration: 120 } }
            Behavior on border.color { ColorAnimation { duration: 120 } }
        }
    }

    component IconPill: Button {
        id: ip
        property color accent: "#e8f7ff"
        implicitWidth: 52
        implicitHeight: 52
        padding: 0
        focusPolicy: Qt.StrongFocus
        font.pixelSize: 26
        font.weight: Font.DemiBold
        contentItem: Label {
            text: ip.text
            color: ip.accent
            font: ip.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 26
            color: ip.down || ip.hovered || ip.activeFocus ? "#2f4658ee" : "#1c2832dd"
            border.width: 1
            border.color: ip.down || ip.hovered || ip.activeFocus ? Qt.lighter(ip.accent, 1.2) : "#3d566a"
            Behavior on color { ColorAnimation { duration: 120 } }
            Behavior on border.color { ColorAnimation { duration: 120 } }
        }
    }

    // Subtle scrim — kept transparent at the top so video is unobstructed.
    Rectangle {
        anchors.fill: parent
        visible: overlay.controlsOpacity > 0 || subtitleSheet.opened || menuPopup.opened
        opacity: overlay.controlsOpacity
        color: "transparent"
        gradient: Gradient {
            GradientStop { position: 0.00; color: "#55000000" }
            GradientStop { position: 0.35; color: "#00000000" }
            GradientStop { position: 0.70; color: "#00000000" }
            GradientStop { position: 1.00; color: "#cc000000" }
        }
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 44
        spacing: 18
        visible: overlay.controlsOpacity > 0 && !subtitleSheet.opened
        opacity: overlay.controlsOpacity

        RowLayout {
            Layout.fillWidth: true
            spacing: 22

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    Layout.fillWidth: true
                    text: appController.player.title
                    color: "#f8fbff"
                    font.pixelSize: 38
                    font.weight: Font.DemiBold
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: appController.player.statusText
                    color: appController.player.seeking ? "#9ff0ff" : "#b3c7d6"
                    font.pixelSize: 22
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }
            }

            Label {
                text: overlay.formatClock(overlay.displaySeconds())
                      + "  /  " + overlay.formatClock(appController.player.durationSeconds)
                color: "#d8edf7"
                font.pixelSize: 24
                font.family: "monospace"
            }
        }

        // Refined scrubber: thinner track, glowing thumb on hover/scrub.
        Item {
            id: timelineFocus
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            focus: false
            KeyNavigation.down: pauseButton

            readonly property double ratio: appController.player.durationSeconds > 0
                                        ? Math.max(0, Math.min(1, overlay.displaySeconds() / appController.player.durationSeconds))
                                        : 0
            readonly property bool hot: activeFocus || overlay.scrubbing || scrubMouse.containsMouse

            Rectangle {
                id: track
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                height: timelineFocus.hot ? 10 : 7
                radius: height / 2
                color: "#26323f"
                border.width: 1
                border.color: "#33485a"
                Behavior on height { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
            }

            Rectangle {
                anchors.left: track.left
                anchors.verticalCenter: track.verticalCenter
                width: Math.max(track.height, track.width * timelineFocus.ratio)
                height: track.height
                radius: track.height / 2
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "#5cdcff" }
                    GradientStop { position: 1.0; color: overlay.scrubbing || timelineFocus.activeFocus ? "#ffffff" : "#74ffd7" }
                }
            }

            Rectangle {
                id: thumb
                x: Math.max(-width / 2, Math.min(track.width - width / 2, track.width * timelineFocus.ratio - width / 2))
                anchors.verticalCenter: track.verticalCenter
                width: timelineFocus.hot ? 26 : 18
                height: width
                radius: width / 2
                color: "#ffffff"
                border.width: 3
                border.color: overlay.scrubbing || timelineFocus.activeFocus ? "#ffffff" : "#74ffd7"
                Behavior on width { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                // Soft glow while scrubbing/hovering.
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width + 14
                    height: parent.height + 14
                    radius: width / 2
                    color: "transparent"
                    border.width: 2
                    border.color: "#5574ffd7"
                    visible: timelineFocus.hot
                }
            }

            MouseArea {
                id: scrubMouse
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                hoverEnabled: true

                function update(mouse) {
                    const ratio = Math.max(0, Math.min(1, mouse.x / Math.max(1, width)))
                    overlay.scrubSeconds = ratio * appController.player.durationSeconds
                }

                onPressed: (mouse) => {
                    overlay.showControls("tap")
                    overlay.scrubbing = true
                    update(mouse)
                }
                onPositionChanged: (mouse) => {
                    overlay.showControls(overlay.scrubbing ? "key" : "pointer-move")
                    if (overlay.scrubbing)
                        update(mouse)
                }
                onReleased: (mouse) => {
                    if (overlay.scrubbing) {
                        update(mouse)
                        overlay.seekTo(overlay.scrubSeconds)
                        overlay.scrubbing = false
                    }
                }
                onCanceled: overlay.scrubbing = false
            }
        }

        RowLayout {
            id: controlsRow
            Layout.fillWidth: true
            spacing: 12

            PillButton {
                id: pauseButton
                text: appController.player.paused ? "Play" : "Pause"
                accent: "#74ffd7"
                KeyNavigation.right: subtitlesButton
                KeyNavigation.up: timelineFocus
                onActiveFocusChanged: if (activeFocus) overlay.showControls("key")
                onClicked: {
                    overlay.showControls("tap")
                    appController.player.togglePause()
                }
            }
            PillButton {
                id: subtitlesButton
                text: "Subtitles"
                KeyNavigation.left: pauseButton
                KeyNavigation.right: menuButton
                KeyNavigation.up: timelineFocus
                onActiveFocusChanged: if (activeFocus) overlay.showControls("key")
                onClicked: {
                    overlay.showControls("tap")
                    overlay.openSubtitleSheet()
                }
            }

            Item { Layout.fillWidth: true }

            Label {
                text: "↑ timeline  ←/→ seek  Back hides"
                color: "#8da6b8"
                font.pixelSize: 18
            }

            IconPill {
                id: menuButton
                text: "⋯"
                accent: "#e8f7ff"
                KeyNavigation.left: subtitlesButton
                KeyNavigation.up: timelineFocus
                onActiveFocusChanged: if (activeFocus) overlay.showControls("key")
                onClicked: {
                    overlay.showControls("tap")
                    menuPopup.opened ? menuPopup.close() : overlay.openMenu()
                }
            }
        }
    }

    // Hamburger / overflow menu for secondary actions.
    Popup {
        id: menuPopup
        parent: overlay
        modal: false
        focus: opened
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent | Popup.CloseOnPressOutside
        width: 280
        x: overlay.width - width - 44
        y: overlay.height - height - 120
        padding: 10

        enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 140 } }
        exit:  Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 } }

        Keys.onReleased: (event) => {
            if (overlay.isBackEvent(event)) {
                event.accepted = overlay.handleBack()
            } else if (overlay.isAcceptKey(event.key)) {
                event.accepted = overlay.handleAccept()
            } else if (event.key === Qt.Key_Up || event.key === Qt.Key_Down ||
                       event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
                event.accepted = overlay.handleDirectionalKey(event.key)
            }
        }

        background: Rectangle {
            radius: 22
            color: "#ee131d27"
            border.width: 1
            border.color: "#3d566a"
        }

        contentItem: ColumnLayout {
            spacing: 6

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.topMargin: 6
                text: "Options"
                color: "#7e96a8"
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }

            component MenuRow: Button {
                id: row
                property string accentText: ""
                property int actionIndex: 0
                Layout.fillWidth: true
                implicitHeight: 50
                padding: 12
                contentItem: RowLayout {
                    spacing: 12
                    Label {
                        Layout.fillWidth: true
                        text: row.text
                        color: row.actionIndex === overlay.menuFocusIndex ? "#07111a" : "#f2f8ff"
                        font.pixelSize: 20
                        font.weight: row.actionIndex === overlay.menuFocusIndex ? Font.DemiBold : Font.Normal
                    }
                    Label {
                        text: row.accentText
                        color: row.actionIndex === overlay.menuFocusIndex ? "#21404c" : "#74ffd7"
                        font.pixelSize: 18
                        visible: text.length > 0
                    }
                }
                background: Rectangle {
                    radius: 14
                    color: row.actionIndex === overlay.menuFocusIndex
                           ? "#d7f9ff"
                           : (row.hovered || row.down ? "#2a3b4c" : "transparent")
                    border.width: row.actionIndex === overlay.menuFocusIndex ? 1 : 0
                    border.color: "#ffffff"
                    Behavior on color { ColorAnimation { duration: 100 } }
                }
                onClicked: overlay.activateMenuAction(actionIndex)
            }

            MenuRow {
                actionIndex: 0
                text: "Toggle debug OSD"
                accentText: "I"
            }
            MenuRow {
                actionIndex: 1
                text: "Subtitles"
                accentText: "S"
            }
            MenuRow {
                actionIndex: 2
                text: "Hide controls"
            }
            MenuRow {
                actionIndex: 3
                text: "Stop playback"
            }
        }
    }

    Popup {
        id: subtitleSheet
        parent: overlay
        modal: false
        focus: opened
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(520, overlay.width - 96)
        height: Math.min(420, subtitleColumn.implicitHeight + 32)
        x: overlay.width - width - 44
        y: overlay.height - height - 44
        padding: 16

        enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 160 } }
        exit:  Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 } }

        Keys.onReleased: (event) => {
            if (overlay.isBackEvent(event)) {
                event.accepted = overlay.handleBack()
            } else if (overlay.isAcceptKey(event.key)) {
                event.accepted = overlay.handleAccept()
            } else if (event.key === Qt.Key_Up || event.key === Qt.Key_Down ||
                       event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
                event.accepted = overlay.handleDirectionalKey(event.key)
            }
        }

        background: Rectangle {
            radius: 22
            color: "#ee131d27"
            border.width: 1
            border.color: "#3d566a"
        }

        contentItem: ColumnLayout {
            id: subtitleColumn
            spacing: 16

            Label {
                Layout.fillWidth: true
                text: "Subtitles"
                color: "#f7fbff"
                font.pixelSize: 26
                font.weight: Font.DemiBold
            }

            Label {
                Layout.fillWidth: true
                text: appController.player.subtitleTracks.length > 1
                      ? "Select a subtitle track."
                      : "No subtitle tracks."
                color: "#a9bdca"
                font.pixelSize: 16
                wrapMode: Text.Wrap
            }

            Repeater {
                model: appController.player.subtitleTracks
                delegate: Button {
                    required property int index
                    required property string modelData
                    Layout.fillWidth: true
                    implicitHeight: 46
                    text: modelData
                    onClicked: {
                        appController.player.selectSubtitle(index)
                        subtitleSheet.close()
                    }
                    contentItem: Label {
                        text: parent.text
                        color: index === overlay.subtitleFocusIndex ||
                               index === appController.player.selectedSubtitleIndex ? "#07111a" : "#f2f8ff"
                        font.pixelSize: 18
                        font.weight: index === appController.player.selectedSubtitleIndex ? Font.DemiBold : Font.Normal
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                    background: Rectangle {
                        radius: 13
                        color: index === overlay.subtitleFocusIndex
                               ? "#d7f9ff"
                               : index === appController.player.selectedSubtitleIndex
                               ? "#74ffd7"
                               : (parent.hovered || parent.down ? "#2a3b4c" : "#1b2632")
                        border.width: 1
                        border.color: index === overlay.subtitleFocusIndex
                                      ? "#ffffff"
                                      : index === appController.player.selectedSubtitleIndex ? "#74ffd7" : "#3d566a"
                        Behavior on color { ColorAnimation { duration: 100 } }
                    }
                }
            }
        }
    }
}
