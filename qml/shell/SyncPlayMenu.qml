import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

// Dropdown menu for the top-bar SyncPlay button. Mirrors jellyfin-web's
// SyncPlayMenu: when in a group it shows the active group and a Leave action;
// otherwise it lists joinable groups and an option to create a new one.
FocusScope {
    id: menu

    property bool menuOpen: false
    readonly property var syncPlay: appController ? appController.syncPlay : null
    property var entries: []
    property int currentIndex: 0
    signal requestClose()

    visible: menuOpen
    implicitHeight: panel.implicitHeight
    height: implicitHeight

    function defaultGroupName() {
        const name = appController && appController.session.username ? String(appController.session.username) : ""
        return (name.length > 0 ? name : "My") + "'s group"
    }

    function isActionable(entry) {
        return entry && (entry.kind === "join" || entry.kind === "create" || entry.kind === "leave")
    }

    function firstActionable(list) {
        for (let i = 0; i < list.length; ++i)
            if (isActionable(list[i]))
                return i
        return 0
    }

    function buildEntries() {
        const out = []
        if (!syncPlay)
            return out
        if (syncPlay.enabled) {
            const memberCount = syncPlay.participantCount || 0
            const statusParts = [
                memberCount + " member" + (memberCount === 1 ? "" : "s")
            ]
            if ((syncPlay.groupState || "").length > 0)
                statusParts.push(syncPlay.groupState)
            statusParts.push(syncPlay.socketConnected
                             ? Math.round(syncPlay.pingMs) + " ms"
                             : "Reconnecting")
            out.push({ kind: "current", label: syncPlay.currentGroupName || "SyncPlay group",
                       sub: statusParts.join(" / "), icon: "groups" })
            out.push({ kind: "leave", label: "Leave group", sub: "", icon: "logout" })
        } else {
            const groups = syncPlay.groups || []
            for (let i = 0; i < groups.length; ++i) {
                const group = groups[i] || ({})
                const count = group.Participants ? group.Participants.length : 0
                out.push({ kind: "join", groupId: group.GroupId || "",
                           label: group.GroupName || "Group",
                           sub: count + " member" + (count === 1 ? "" : "s"), icon: "login" })
            }
            if (groups.length === 0)
                out.push({ kind: "empty", label: "No groups available", sub: "", icon: "block" })
            out.push({ kind: "create", label: "Create a new group", sub: "", icon: "add" })
        }
        return out
    }

    function openMenu() {
        entries = buildEntries()
        currentIndex = firstActionable(entries)
        menuOpen = true
        InputKeys.focus(list)
    }

    function closeMenu() {
        menuOpen = false
    }

    function activate(index) {
        const entry = entries[index]
        if (!entry || !syncPlay)
            return
        if (entry.kind === "join")
            syncPlay.joinGroup(entry.groupId)
        else if (entry.kind === "create")
            syncPlay.createGroup(defaultGroupName())
        else if (entry.kind === "leave")
            syncPlay.leaveGroup()
        else
            return
        requestClose()
    }

    function handleKey(key) {
        if (key === Qt.Key_Up) {
            currentIndex = Math.max(0, currentIndex - 1)
            return true
        }
        if (key === Qt.Key_Down) {
            currentIndex = Math.min(entries.length - 1, currentIndex + 1)
            return true
        }
        if (key === Qt.Key_Left || key === Qt.Key_Right) {
            requestClose()
            return true
        }
        return true
    }

    Connections {
        target: menu.syncPlay
        enabled: menu.menuOpen
        function onGroupsChanged() {
            menu.entries = menu.buildEntries()
            menu.currentIndex = Math.min(menu.currentIndex, Math.max(0, menu.entries.length - 1))
        }
        function onGroupChanged() {
            menu.entries = menu.buildEntries()
            menu.currentIndex = menu.firstActionable(menu.entries)
        }
        function onConnectionChanged() {
            menu.entries = menu.buildEntries()
        }
    }

    Surface {
        id: panel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        implicitHeight: contentColumn.implicitHeight + 20
        height: implicitHeight
        elevated: true
        baseColor: Theme.bgRaised
        clip: true

        ColumnLayout {
            id: contentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 10
            spacing: 6

            AppText {
                Layout.fillWidth: true
                Layout.leftMargin: 4
                text: "SyncPlay"
                color: Theme.textMuted
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }

            ListView {
                id: list
                Layout.fillWidth: true
                Layout.preferredHeight: contentHeight
                interactive: false
                keyNavigationEnabled: false
                model: menu.entries
                currentIndex: menu.currentIndex
                spacing: 2

                delegate: Rectangle {
                    id: row
                    required property int index
                    required property var modelData
                    readonly property bool actionable: menu.isActionable(modelData)
                    readonly property bool current: index === menu.currentIndex && list.activeFocus
                    width: ListView.view.width
                    height: 52
                    radius: Theme.radiusSmall
                    color: current ? Theme.accentPanel
                          : (hover.hovered && actionable) ? Theme.bgHover
                          : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 12

                        MaterialIcon {
                            visible: (row.modelData.icon || "").length > 0
                            name: row.modelData.icon || ""
                            iconSize: 20
                            iconColor: row.current ? Theme.textPrimary
                                      : row.actionable ? Theme.textSecondary
                                      : Theme.textMuted
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            AppText {
                                Layout.fillWidth: true
                                text: row.modelData.label || ""
                                color: row.actionable || row.current ? Theme.textPrimary : Theme.textSecondary
                                font.weight: Font.Medium
                                maximumLineCount: 1
                                elide: Text.ElideRight
                            }
                            AppText {
                                Layout.fillWidth: true
                                visible: (row.modelData.sub || "").length > 0
                                text: row.modelData.sub || ""
                                color: Theme.textMuted
                                font.pixelSize: 12
                                maximumLineCount: 1
                                elide: Text.ElideRight
                            }
                        }
                    }

                    HoverHandler { id: hover }
                    TapHandler {
                        onTapped: {
                            menu.currentIndex = row.index
                            menu.activate(row.index)
                        }
                    }
                }

                Keys.onReleased: (event) => {
                    if (InputKeys.isAccept(event.key)) {
                        menu.activate(menu.currentIndex)
                        event.accepted = true
                    } else if (InputKeys.isBack(event.key, true, false)) {
                        menu.requestClose()
                        event.accepted = true
                    }
                }
            }
        }
    }
}
