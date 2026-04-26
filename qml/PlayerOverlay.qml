import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

FocusScope {
    id: overlay
    focus: visible

    property bool controlsVisible: true
    property bool isScrubbing: false
    property double scrubSeconds: 0

    readonly property bool pinnedControls: appController.player.paused
                                          || appController.player.buffering
                                          || isScrubbing
                                          || overflowMenu.opened

    function formatClock(seconds) {
        const total = Math.max(0, Math.floor(seconds || 0))
        const hours = Math.floor(total / 3600)
        const minutes = Math.floor((total % 3600) / 60)
        const secs = total % 60
        if (hours > 0)
            return hours + ":" + String(minutes).padStart(2, "0") + ":" + String(secs).padStart(2, "0")
        return minutes + ":" + String(secs).padStart(2, "0")
    }

    function shouldAutoHide() {
        return overlay.visible && !overlay.pinnedControls
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

    function revealControls() {
        controlsVisible = true
        if (shouldAutoHide())
            autohideTimer.restart()
        else
            autohideTimer.stop()
    }

    Timer {
        id: autohideTimer
        interval: 3500
        onTriggered: {
            if (overlay.shouldAutoHide())
                overlay.controlsVisible = false
        }
    }

    onVisibleChanged: {
        if (visible) {
            controlsVisible = true
            revealControls()
        } else {
            autohideTimer.stop()
            overflowMenu.close()
            controlsVisible = false
        }
    }

    onPinnedControlsChanged: revealControls()

    TapHandler {
        onTapped: overlay.revealControls()
    }

    HoverHandler {
        onPointChanged: overlay.revealControls()
    }

    Keys.onPressed: (event) => {
        overlay.revealControls()
        if (overlay.isIgnoredPlayerNoise(event)) {
            event.accepted = true
            return
        }
        if (overlay.isBackEvent(event) && overflowMenu.opened) {
            overflowMenu.close()
            event.accepted = true
        } else if (event.key === Qt.Key_Left) {
            appController.player.seekBack()
            event.accepted = true
        } else if (event.key === Qt.Key_Right) {
            appController.player.seekForward()
            event.accepted = true
        }
    }

    Keys.onReleased: (event) => {
        overlay.revealControls()

        if (overlay.isIgnoredPlayerNoise(event)) {
            event.accepted = true
            return
        }

        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            appController.player.togglePause()
            event.accepted = true
        } else if (event.key === Qt.Key_I || event.key === Qt.Key_Info || event.key === Qt.Key_Menu) {
            appController.player.toggleDebugOsd()
            event.accepted = true
        } else if (overlay.isBackEvent(event)) {
            if (overflowMenu.opened)
                overflowMenu.close()
            else if (appController.player.backAllowed)
                appController.player.stopWithReason("overlay-back-key")
            event.accepted = true
        }
    }

    Popup {
        id: overflowMenu
        parent: overlay
        width: 260
        height: menuColumn.implicitHeight + 20
        x: Math.max(24, Math.min(overlay.width - width - 24, overflowButton.x + overflowButton.width - width))
        y: Math.max(24, dock.y - height - 14)
        focus: opened
        modal: false
        padding: 10
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onOpened: {
            overlay.controlsVisible = true
            menuAction.forceActiveFocus()
        }

        background: Rectangle {
            radius: 24
            color: "#f0131d26"
            border.width: 1
            border.color: "#4f7d97"
        }

        contentItem: ColumnLayout {
            id: menuColumn
            spacing: 8

            Button {
                id: menuAction
                Layout.fillWidth: true
                text: appController.player.debugOsdVisible ? "Hide Stats" : "Show Stats"
                onClicked: {
                    appController.player.toggleDebugOsd()
                    overflowMenu.close()
                }
            }

            Label {
                Layout.fillWidth: true
                text: "Shortcut: Info / Menu / I"
                color: "#8fb7cb"
                font.pixelSize: 18
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    Rectangle {
        id: dock
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 28
        height: errorLabel.visible ? 236 : 204
        radius: 30
        visible: overlay.controlsVisible || overflowMenu.opened
        enabled: visible
        color: "#ec081119"
        border.width: 1
        border.color: "#396480"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: appController.player.title
                        color: "#f2fbff"
                        font.pixelSize: 30
                        font.weight: Font.DemiBold
                        maximumLineCount: 1
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: appController.player.statusText
                        color: appController.player.buffering || appController.player.seeking ? "#9de8ff" : "#7fc6de"
                        font.pixelSize: 19
                        maximumLineCount: 1
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    radius: 16
                    color: "#2f17232c"
                    border.width: 1
                    border.color: "#315168"
                    implicitWidth: statusRow.implicitWidth + 20
                    implicitHeight: 34

                    Row {
                        id: statusRow
                        anchors.centerIn: parent
                        spacing: 12

                        Label {
                            text: appController.player.paused ? "Paused" : "Playing"
                            color: "#d9f5ff"
                            font.pixelSize: 18
                        }

                        Label {
                            text: overlay.formatClock(appController.player.positionSeconds)
                                  + " / " + overlay.formatClock(appController.player.durationSeconds)
                            color: "#9ecbdd"
                            font.pixelSize: 18
                        }
                    }
                }

                Button {
                    id: overflowButton
                    implicitWidth: 60
                    implicitHeight: 60
                    onClicked: {
                        overlay.revealControls()
                        if (overflowMenu.opened)
                            overflowMenu.close()
                        else
                            overflowMenu.open()
                    }

                    contentItem: Item {
                        implicitWidth: 24
                        implicitHeight: 24

                        Row {
                            anchors.centerIn: parent
                            spacing: 4

                            Repeater {
                                model: 3
                                delegate: Rectangle {
                                    width: 6
                                    height: 6
                                    radius: 3
                                    color: "#e5f7ff"
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                id: progressBarContainer
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                color: "transparent"

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 12
                    radius: 6
                    color: "#16313f"
                    
                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: Math.max(12, parent.width * (appController.player.durationSeconds > 0
                                                            ? (overlay.isScrubbing ? overlay.scrubSeconds : appController.player.positionSeconds) / appController.player.durationSeconds
                                                            : 0))
                        radius: 6
                        color: overlay.isScrubbing ? "#ffffff" : (appController.player.buffering ? "#62d2f1" : "#84f0d2")
                        
                        Behavior on width {
                            enabled: !overlay.isScrubbing
                            NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton

                    function updateScrub(mouse) {
                        const pos = Math.max(0, Math.min(width, mouse.x))
                        const ratio = pos / width
                        overlay.scrubSeconds = ratio * appController.player.durationSeconds
                    }

                    onPressed: (mouse) => {
                        overlay.revealControls()
                        overlay.isScrubbing = true
                        updateScrub(mouse)
                    }

                    onPositionChanged: (mouse) => {
                        overlay.revealControls()
                        if (overlay.isScrubbing) {
                            updateScrub(mouse)
                        }
                    }

                    onReleased: (mouse) => {
                        overlay.revealControls()
                        if (overlay.isScrubbing) {
                            updateScrub(mouse)
                            appController.player.seek(overlay.scrubSeconds)
                            overlay.isScrubbing = false
                        }
                    }
                    
                    onCanceled: {
                        overlay.isScrubbing = false
                    }
                }

                Rectangle {
                    visible: overlay.isScrubbing
                    x: Math.max(0, Math.min(progressBarContainer.width - width, 
                                            (overlay.scrubSeconds / appController.player.durationSeconds) * progressBarContainer.width - width/2))
                    y: -40
                    width: scrubLabel.implicitWidth + 16
                    height: 32
                    radius: 8
                    color: "#f0131d26"
                    border.width: 1
                    border.color: "#84f0d2"

                    Label {
                        id: scrubLabel
                        anchors.centerIn: parent
                        text: overlay.formatClock(overlay.scrubSeconds)
                        color: "white"
                        font.pixelSize: 18
                        font.bold: true
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Button {
                    text: "10s"
                    onClicked: {
                        overlay.revealControls()
                        appController.player.seekBack()
                    }
                }

                Button {
                    text: appController.player.paused ? "Play" : "Pause"
                    onClicked: {
                        overlay.revealControls()
                        appController.player.togglePause()
                    }
                }

                Button {
                    text: "30s"
                    onClicked: {
                        overlay.revealControls()
                        appController.player.seekForward()
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                Label {
                    text: "Back exit  Info stats  Left/Right seek"
                    color: "#82afc0"
                    font.pixelSize: 18
                }
            }

            Label {
                id: errorLabel
                Layout.fillWidth: true
                visible: appController.player.errorText.length > 0
                text: appController.player.errorText
                color: "#ffb8bd"
                font.pixelSize: 18
                wrapMode: Text.Wrap
            }
        }
    }
}
