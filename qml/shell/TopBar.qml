pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

// Horizontal top navigation bar. Hosts the primary routes on the left and the
// SyncPlay control on the right (mirroring jellyfin-web's AppToolbar). D-pad:
// Left/Right move between bar items, Down enters the content area, and pages
// return focus here by pressing Up at their top edge.
FocusScope {
    id: root
    property string currentRoute: "home"
    signal navigate(string route)
    signal contentRequested

    readonly property bool syncPlayMenuOpen: syncMenuLoader.item ? syncMenuLoader.item.menuOpen : false
    property bool syncPlayMenuLoaded: false
    readonly property var syncPlay: SyncPlay
    readonly property bool syncActive: syncPlay ? syncPlay.enabled : false
    readonly property var syncGroups: syncPlay ? syncPlay.groups : []
    readonly property bool syncAvailable: syncGroups && syncGroups.length > 0
    readonly property string selectedRoute: currentRoute === "libraryGrid" ? "home" : currentRoute

    // Index space: [0 .. railRepeater.count-1] are nav buttons, the trailing
    // index is the SyncPlay button.
    function focusedIndex() {
        for (let i = 0; i < railRepeater.count; ++i) {
            const item = railRepeater.itemAt(i)
            if (item && item.hasButtonFocus())
                return i
        }
        if (syncButton.activeFocus)
            return railRepeater.count
        return 0
    }

    function focusIndex(index) {
        const clamped = Math.max(0, Math.min(railRepeater.count, index))
        if (clamped >= railRepeater.count) {
            InputKeys.focus(syncButton)
            return
        }
        const item = railRepeater.itemAt(clamped)
        if (item)
            item.forceButtonFocus()
    }

    function focusCurrent() {
        for (let i = 0; i < railRepeater.count; ++i) {
            const item = railRepeater.itemAt(i)
            if (item && item.route === selectedRoute) {
                item.forceButtonFocus()
                return
            }
        }
        const first = railRepeater.itemAt(0)
        if (first)
            first.forceButtonFocus()
    }

    function syncMenu() {
        syncPlayMenuLoaded = true
        return syncMenuLoader.item
    }

    function openSyncMenu() {
        if (syncPlay)
            syncPlay.refreshGroups()
        const menu = syncMenu()
        if (menu)
            menu.openMenu()
    }

    function closeSyncPlayMenu(restoreFocus) {
        const menu = syncMenuLoader.item
        if (menu)
            menu.closeMenu()
        if (restoreFocus !== false)
            InputKeys.focus(syncButton)
    }

    function containsSyncPlayPoint(item, x, y) {
        const buttonPoint = syncButton.mapFromItem(item, x, y)
        if (syncButton.contains(buttonPoint))
            return true
        const menu = syncMenuLoader.item
        if (!menu || !menu.menuOpen)
            return false
        const menuPoint = menu.mapFromItem(item, x, y)
        return menu.contains(menuPoint)
    }

    function activate() {
        const menu = syncMenuLoader.item
        if (menu && menu.menuOpen) {
            menu.activate()
            return
        }
        const index = focusedIndex()
        if (index >= railRepeater.count) {
            openSyncMenu()
        } else {
            const item = railRepeater.itemAt(index)
            if (item)
                navigate(item.route)
        }
    }

    function back() {
        if (!syncPlayMenuOpen)
            return false
        closeSyncPlayMenu()
        return true
    }

    onActiveFocusChanged: if (!activeFocus && syncPlayMenuOpen)
    closeSyncPlayMenu(false)

    function routeKey(key, phase, repeat) {
        const menu = syncMenuLoader.item
        if (menu && menu.menuOpen)
            return menu.routeKey(key, phase, repeat)
        if (key === Qt.Key_Down) {
            contentRequested()
            return true
        }
        if (key === Qt.Key_Right) {
            focusIndex(focusedIndex() + 1)
            return true
        }
        if (key === Qt.Key_Left) {
            focusIndex(focusedIndex() - 1)
            return true
        }
        return key === Qt.Key_Up
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: eventPoint => {
            const local = syncButton.mapFromItem(root, eventPoint.position.x, eventPoint.position.y)
            if (!syncButton.contains(local))
                root.closeSyncPlayMenu(false)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.bgRaised
    }
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: Theme.hoverBorderWidth
        color: Theme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Metrics.scaled(14)
        anchors.rightMargin: Metrics.scaled(14)
        spacing: Metrics.scaled(4)

        Repeater {
            id: railRepeater
            model: [
                {
                    label: "My Media",
                    route: "home",
                    icon: "home"
                },
                {
                    label: "Search",
                    route: "search",
                    icon: "search"
                },
                {
                    label: "Switch user",
                    route: "switchUser",
                    icon: "person"
                },
                {
                    label: "Settings",
                    route: "settings",
                    icon: "settings"
                }
            ]

            delegate: Item {
                id: railDelegate
                required property var modelData
                readonly property string route: modelData.route
                function forceButtonFocus() {
                    InputKeys.focus(button)
                }
                function hasButtonFocus() {
                    return button.activeFocus
                }
                Layout.preferredWidth: Metrics.scaled(50)
                Layout.fillHeight: true

                IconButton {
                    id: button
                    anchors.centerIn: parent
                    iconName: modelData.icon
                    accessibleName: modelData.route === "switchUser" && Session.activeProfileLabel.length > 0
                                    ? "Switch user — " + Session.activeProfileLabel : modelData.label
                    railStyle: true
                    selected: root.selectedRoute === modelData.route
                    onClicked: {
                        root.closeSyncPlayMenu(false)
                        root.navigate(modelData.route)
                    }
                }

                Rectangle {
                    anchors.top: button.bottom
                    anchors.topMargin: Metrics.scaled(5)
                    anchors.horizontalCenter: button.horizontalCenter
                    width: switchTooltip.implicitWidth + Metrics.scaled(16)
                    height: switchTooltip.implicitHeight + Metrics.scaled(10)
                    radius: Theme.radiusSmall
                    color: Theme.floatingPanel
                    border.width: Theme.hoverBorderWidth
                    border.color: Theme.borderStrong
                    visible: Platform.hasDesktopPointer && modelData.route === "switchUser" && button.pointerHovered
                    z: 100

                    AppText {
                        id: switchTooltip
                        anchors.centerIn: parent
                        text: "Switch user — " + Session.activeProfileLabel
                        color: Theme.textPrimary
                        font.pixelSize: Metrics.metaSizePx
                        maximumLineCount: 1
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
        }

        Item {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: Metrics.scaled(50)
            Layout.fillHeight: true

            IconButton {
                id: syncButton
                anchors.centerIn: parent
                iconName: "groups"
                accessibleName: "SyncPlay"
                railStyle: true
                selected: root.syncPlayMenuOpen
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: {
                    if (root.syncPlayMenuOpen)
                    root.closeSyncPlayMenu(false)
                    else
                    root.openSyncMenu()
                }

                // Status dot: accent when in a group, green when groups exist to join.
                Rectangle {
                    visible: root.syncActive || root.syncAvailable
                    width: Metrics.scaled(9)
                    height: width
                    radius: width / 2
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.rightMargin: Metrics.scaled(7)
                    anchors.topMargin: Metrics.scaled(7)
                    color: root.syncActive ? Theme.accent : Theme.success
                    border.width: Theme.focusBorderWidth
                    border.color: Theme.bgRaised
                }
            }
        }
    }

    Loader {
        id: syncMenuLoader
        width: Metrics.scaled(320)
        anchors.top: parent.bottom
        anchors.right: parent.right
        anchors.topMargin: Metrics.scaled(6)
        anchors.rightMargin: Metrics.scaled(14)
        z: 50
        active: root.syncPlayMenuLoaded
        sourceComponent: SyncPlayMenu {
            onRequestClose: root.closeSyncPlayMenu()
        }
    }
}
