pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"
import "../shell" as Shell

Item {
    id: root

    required property var overlay
    readonly property bool syncPlayMenuOpen: syncPlayMenu.menuOpen

    function dp(value) {
        return overlay ? overlay.dp(value) : Math.round(value)
    }

    function artworkSource(url) {
        if (!url)
            return ""
        return url.indexOf("http://") === 0 || url.indexOf("https://") === 0 ? "image://artwork/" + encodeURIComponent(
                                                                                   url) : url
    }

    function resetMenu(index) {
        Qt.callLater(function () {
            menuList.currentIndex = menuList.count > 0 ? Math.min(index, menuList.count - 1) : -1
            menuList.clampEnabled()
            if (menuList.currentIndex >= 0)
                menuList.positionViewAtIndex(menuList.currentIndex, ListView.Contain)
        })
    }

    function openSyncPlayMenu() {
        SyncPlay.refreshGroups()
        syncPlayMenu.openMenu()
    }

    function closeSyncPlayMenu() {
        syncPlayMenu.closeMenu()
    }

    function routeSyncPlayMenuKey(key) {
        return syncPlayMenu.routeKey(key, "release", false)
    }

    function activateSyncPlayMenu() {
        syncPlayMenu.activate()
    }

    function routeMenuKey(key) {
        if (overlay.menuKind === "debug" && menuList.currentIndex === 0 && InputKeys.isHorizontal(key)) {
            overlay.adjustPlaybackSpeed(key === Qt.Key_Left ? -1 : 1)
            return true
        }
        return menuList.routeKey(key, "release", false)
    }

    function activateMenu() {
        menuList.activate()
    }

    Item {
        visible: false
        Repeater {
            model: root.overlay.visible && root.overlay.hasPlayer ? root.overlay.player.trickplaySheetUrls : []
            delegate: Image {
                required property string modelData
                source: root.artworkSource(modelData)
                asynchronous: true
                cache: true
            }
        }
    }

    TapHandler {
        onTapped: eventPoint => {
            if (root.syncPlayMenuOpen) {
                const local = syncPlayMenu.mapFromItem(root, eventPoint.position.x, eventPoint.position.y)
                const syncTarget = transportBar.actionTarget("syncplay")
                if (syncTarget) {
                    const syncLocal = syncTarget.mapFromItem(root, eventPoint.position.x, eventPoint.position.y)
                    if (syncTarget.contains(syncLocal))
                    return
                }
                if (!syncPlayMenu.contains(local))
                root.overlay.closeMenu()
                return
            }
            root.overlay.showControls("timeline")
        }
    }
    HoverHandler {
        onHoveredChanged: if (hovered && root.overlay.controlsVisible)
        root.overlay.showControls(root.overlay.focusZone)
    }
    WheelHandler {
        target: null
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: event => root.overlay.adjustVolumeFromWheel(event)
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.dp(150)
        visible: root.overlay.controlsVisible && !root.overlay.audioSyncVisible
        gradient: Gradient {
            GradientStop {
                position: 0
                color: "#99000000"
            }
            GradientStop {
                position: 1
                color: "transparent"
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: root.dp(360)
        visible: root.overlay.controlsVisible
        opacity: root.overlay.audioSyncVisible ? 0.35 : 1
        gradient: Gradient {
            GradientStop {
                position: 0
                color: "transparent"
            }
            GradientStop {
                position: 1
                color: Theme.overlayScrimStrong
            }
        }
    }

    Rectangle {
        id: backButton
        readonly property bool focused: root.overlay.isControlsActive() && root.overlay.focusZone === "back"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: root.dp(40)
        width: root.dp(64)
        height: width
        radius: width / 2
        visible: root.overlay.controlsVisible
        color: focused ? Qt.alpha(Theme.accent, 0.2) : "transparent"
        border.width: focused ? Theme.focusBorderWidth : 0
        border.color: Theme.accent

        MaterialIcon {
            anchors.centerIn: parent
            name: "arrow_back"
            iconColor: backButton.focused ? Theme.textPrimary : Theme.textSecondary
            iconSize: root.dp(34)
        }

        TapHandler {
            onTapped: root.overlay.stopPlayback("overlay-back")
        }
    }

    PlayerTrickplayPreview {
        overlay: root.overlay
    }

    PlayerSkipSegmentCard {
        overlay: root.overlay
    }

    Item {
        id: syncPlayWaitingIcon

        anchors.centerIn: parent
        width: root.dp(116)
        height: width
        visible: SyncPlay.enabled && SyncPlay.waitingForPlayback
        z: 20

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "#C9161616"
            border.width: 1
            border.color: Theme.borderStrong
        }

        MaterialIcon {
            anchors.centerIn: parent
            name: "schedule"
            iconColor: Theme.textPrimary
            iconSize: root.dp(82)
        }

        MaterialIcon {
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: root.dp(20)
            anchors.verticalCenterOffset: root.dp(15)
            name: "play_arrow"
            iconColor: Theme.accent
            iconSize: root.dp(38)
        }

        SequentialAnimation on opacity {
            running: syncPlayWaitingIcon.visible && !Theme.reducedMotion
            loops: Animation.Infinite
            NumberAnimation {
                from: 1
                to: 0.58
                duration: 650
            }
            NumberAnimation {
                from: 0.58
                to: 1
                duration: 650
            }
        }
    }

    Item {
        id: hud
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: root.dp(52)
        height: root.dp(276)
        visible: root.overlay.controlsVisible

        ColumnLayout {
            anchors.fill: parent
            spacing: root.dp(16)

            RowLayout {
                Layout.fillWidth: true
                spacing: root.dp(20)

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: root.dp(6)
                    AppText {
                        Layout.fillWidth: true
                        Layout.preferredHeight: visible ? implicitHeight : 0
                        visible: root.overlay.episodeContextText.length > 0
                        text: root.overlay.episodeContextText
                        color: Theme.textSecondary
                        font.pixelSize: root.dp(22)
                        font.weight: Font.DemiBold
                        maximumLineCount: 1
                        elide: Text.ElideRight
                    }

                    AppText {
                        Layout.fillWidth: true
                        Layout.preferredHeight: visible ? implicitHeight : 0
                        visible: !root.overlay.episodeQueue || root.overlay.showEpisodeTitle
                        text: root.overlay.overlayTitle
                        color: Theme.textPrimary
                        font.pixelSize: root.dp(40)
                        font.weight: Font.Bold
                        maximumLineCount: 1
                        elide: Text.ElideRight
                    }
                    AppText {
                        Layout.fillWidth: true
                        text: root.overlay.hasPlayer ? root.overlay.player.statusText : ""
                        color: root.overlay.hasPlayer && (root.overlay.player.buffering || root.overlay.player.seeking)
                               ? Theme.accent : Theme.textSecondary
                        font.pixelSize: root.dp(24)
                        font.weight: Font.Medium
                        maximumLineCount: 1
                        elide: Text.ElideRight
                    }
                }

                AppText {
                    text: root.overlay.hasPlayer && root.overlay.player.paused ? "Paused" : "Playing"
                    color: root.overlay.hasPlayer && root.overlay.player.paused ? Theme.textPrimary :
                                                                                  Theme.textSecondary

                    font.pixelSize: root.dp(23)
                    font.weight: Font.DemiBold
                }
            }

            PlayerSeekBar {
                overlay: root.overlay
            }

            PlayerTransportBar {
                id: actionRow
                overlay: root.overlay
            }

            AppText {
                Layout.fillWidth: true
                visible: root.overlay.hasPlayer && root.overlay.player.errorText.length > 0
                text: root.overlay.hasPlayer ? root.overlay.player.errorText : ""
                color: Theme.errorText
                font.pixelSize: root.dp(18)
                wrapMode: Text.Wrap
            }
        }
    }

    Shell.SyncPlayMenu {
        id: syncPlayMenu
        anchors.right: parent.right
        anchors.bottom: hud.top
        anchors.rightMargin: root.dp(52)
        anchors.bottomMargin: root.dp(18)
        width: root.dp(420)
        z: 55
        onRequestClose: root.overlay.closeMenu()
    }

    PlayerAudioSyncPanel {
        overlay: root.overlay
    }

    OverlayDialog {
        id: menuDialog
        visible: root.overlay.menuKind.length > 0
        preferredWidth: 620
        z: 50
        onDismissed: root.overlay.closeMenu()

        AppText {
            Layout.fillWidth: true
            text: root.overlay.menuTitle()
            color: Theme.textPrimary
            font.pixelSize: Metrics.titleSizePx
            font.weight: Font.DemiBold
        }

        AppText {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? implicitHeight : 0
            visible: menuList.count === 0
            text: root.overlay.menuPlaceholder()
            color: Theme.textMuted
            font.pixelSize: Metrics.bodySizePx
        }

        MenuListView {
            id: menuList
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? Math.min(contentHeight, root.dp(360)) : 0
            visible: count > 0
            model: root.overlay.menuOptions
            onDismissed: root.overlay.closeMenu()
            onAccepted: index => root.overlay.activateMenuItem(index)

            delegate: MenuRow {
                required property int index
                required property var modelData
                label: root.overlay.menuLabel(modelData)
                detail: root.overlay.menuKind === "debug" && index === 0 && SyncPlay.enabled ? "Managed by SyncPlay while grouped" :
                                                                                               ""
                checked: root.overlay.menuItemSelected(index)
                highlighted: menuList.currentIndex === index
                metricsWidth: root.width
                stepperVisible: root.overlay.menuKind === "debug" && index === 0
                stepperEnabled: !SyncPlay.enabled
                stepperText: root.overlay.formatPlaybackSpeed(root.overlay.player.effectivePlaybackSpeed)
                onDecreaseRequested: {
                    menuList.currentIndex = index
                    root.overlay.adjustPlaybackSpeed(-1)
                }
                onIncreaseRequested: {
                    menuList.currentIndex = index
                    root.overlay.adjustPlaybackSpeed(1)
                }
                onHovered: menuList.currentIndex = index
                onActivated: {
                    menuList.currentIndex = index
                    root.overlay.activateMenuItem(index)
                }
            }
        }
    }
}
