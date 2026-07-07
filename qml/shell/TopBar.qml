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
    signal contentRequested()

    readonly property bool syncPlayMenuOpen: syncMenuLoader.item ? syncMenuLoader.item.menuOpen : false
    property bool syncPlayMenuLoaded: false
    readonly property var syncPlay: appController ? appController.syncPlay : null
    readonly property bool syncActive: syncPlay ? syncPlay.enabled : false
    readonly property var syncGroups: syncPlay ? syncPlay.groups : []
    readonly property bool syncAvailable: syncGroups && syncGroups.length > 0
    readonly property string selectedRoute: currentRoute === "libraryGrid" ? "libraries" : currentRoute

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
        if (item) item.forceButtonFocus()
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
        if (first) first.forceButtonFocus()
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

    function closeSyncPlayMenu() {
        const menu = syncMenuLoader.item
        if (menu)
            menu.closeMenu()
        InputKeys.focus(syncButton)
    }

    function handleKey(key) {
        const menu = syncMenuLoader.item
        if (menu && menu.menuOpen)
            return menu.handleKey(key)
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
        if (key === Qt.Key_Up)
            return true
        if (InputKeys.isAccept(key)) {
            const idx = focusedIndex()
            if (idx >= railRepeater.count) {
                openSyncMenu()
            } else {
                const item = railRepeater.itemAt(idx)
                if (item) navigate(item.route)
            }
            return true
        }
        return false
    }

    Rectangle { anchors.fill: parent; color: Theme.bgRaised }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 4

        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            Layout.rightMargin: 10
            radius: 6
            gradient: Gradient {
                GradientStop { position: 0; color: Theme.jellyfinBlue }
                GradientStop { position: 1; color: Theme.jellyfinPurple }
            }
        }

        Repeater {
            id: railRepeater
            model: [
                { label: "My Media", route: "home", icon: "home" },
                { label: "Libraries", route: "libraries", icon: "video_library" },
                { label: "Search", route: "search", icon: "search" },
                { label: "Settings", route: "settings", icon: "settings" }
            ]

            delegate: Item {
                id: railDelegate
                required property var modelData
                readonly property string route: modelData.route
                function forceButtonFocus() { InputKeys.focus(button) }
                function hasButtonFocus() { return button.activeFocus }
                Layout.preferredWidth: 50
                Layout.fillHeight: true

                IconButton {
                    id: button
                    anchors.centerIn: parent
                    iconName: modelData.icon
                    accessibleName: modelData.label
                    railStyle: true
                    selected: root.selectedRoute === modelData.route
                    onClicked: root.navigate(modelData.route)
                    Keys.onReleased: (event) => {
                        if (InputKeys.isAccept(event.key)) {
                            root.navigate(modelData.route)
                            event.accepted = true
                        }
                    }
                }
                Rectangle {
                    y: Math.min(parent.height - height - 4, button.y + button.height + 4)
                    anchors.horizontalCenter: button.horizontalCenter
                    width: button.activeFocus ? 30 : (root.selectedRoute === modelData.route ? 22 : 0)
                    height: 3
                    radius: 1.5
                    color: Theme.accent
                    opacity: button.activeFocus ? 1.0 : (root.selectedRoute === modelData.route ? 0.85 : 0.0)
                    visible: opacity > 0
                    Behavior on width { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
                    Behavior on opacity { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
                }
            }
        }

        Item { Layout.fillWidth: true }

        Item {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 50
            Layout.fillHeight: true

            IconButton {
                id: syncButton
                anchors.centerIn: parent
                iconName: "groups"
                accessibleName: "SyncPlay"
                railStyle: true
                selected: root.syncPlayMenuOpen
                onClicked: root.openSyncMenu()
                Keys.onReleased: (event) => {
                    if (InputKeys.isAccept(event.key)) {
                        root.openSyncMenu()
                        event.accepted = true
                    }
                }

                // Status dot: accent when in a group, green when groups exist to join.
                Rectangle {
                    visible: root.syncActive || root.syncAvailable
                    width: 9
                    height: 9
                    radius: 4.5
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.rightMargin: 7
                    anchors.topMargin: 7
                    color: root.syncActive ? Theme.accent : Theme.success
                    border.width: 2
                    border.color: Theme.bgRaised
                }
            }
        }
    }

    Loader {
        id: syncMenuLoader
        width: 320
        anchors.top: parent.bottom
        anchors.right: parent.right
        anchors.topMargin: 6
        anchors.rightMargin: 14
        z: 50
        active: root.syncPlayMenuLoaded
        sourceComponent: SyncPlayMenu {
            onRequestClose: root.closeSyncPlayMenu()
        }
    }
}
