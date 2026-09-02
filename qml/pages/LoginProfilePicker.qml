pragma ComponentBehavior: Bound

import QtQuick
import "../theme"
import "../primitives"
import "ProfileNavigation.js" as ProfileNavigation

// The accounts already saved on this device, plus the way to add another.
//
// One grid holds both: the add tile is the cell after the last account, so it
// wraps with everything else and a phone showing two to a row navigates by the
// same rules as a television showing six.
FocusScope {
    id: root

    property bool dense: false

    readonly property var profiles: Session.accountProfiles
    readonly property int profileCount: profiles.length
    // Tiles take the room a lane offers, but never more room than there is:
    // a pane narrower than one tile shrinks the tile rather than clipping it.
    readonly property int tileSize: {
        const preferred = Metrics.scaled(Metrics.laneAtLeast(width, "wide") ? 168 : Metrics.laneAtLeast(width,
                                                                                                        "regular")
                                                                              ? 148 : 124)
        return Math.max(Metrics.scaled(72), Math.min(preferred, width - cellPadding * 2))
    }
    readonly property int cellPadding: Metrics.scaled(14)

    signal profileChosen(string profileId)
    signal addRequested
    signal contextRequested(string profileId, Item anchor, string serverName, string serverUrl)

    function controls() {
        return [grid]
    }

    function focusControl(item) {
        InputKeys.focus(item)
    }

    function focusDefault() {
        if (grid.currentIndex < 0 && grid.count > 0)
            grid.currentIndex = 0
        InputKeys.focus(grid)
    }

    function moveInside(control, key) {
        if (control !== grid || grid.count <= 0)
            return false
        const direction = key === Qt.Key_Left ? "left" : key === Qt.Key_Right ? "right" : key === Qt.Key_Up ? "up" :
                                                                                                              "down"
        const next = ProfileNavigation.move(grid.currentIndex, root.profileCount, grid.columnCount(), direction)
        if (next < 0 || next === grid.currentIndex)
            return false
        grid.currentIndex = next
        grid.positionViewAtIndex(next, GridView.Contain)
        return true
    }

    function activateControl(control) {
        if (control === grid)
            grid.activateCurrent()
    }

    function openContextMenu() {
        return grid.requestContextMenu()
    }

    Column {
        id: heading
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Metrics.scaled(5)
        visible: !root.dense

        AppText {
            width: parent.width
            text: "Choose an account"
            font.pixelSize: Metrics.titleSizePx
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        SecondaryText {
            width: parent.width
            text: root.profileCount === 1 ? "One account is signed in on this device." : root.profileCount
                                            + " accounts are signed in on this device."
            color: Theme.textSecondary
            font.pixelSize: Metrics.bodySizePx
            elide: Text.ElideRight
        }
    }

    GridView {
        id: grid

        readonly property int cellSpan: root.tileSize + root.cellPadding * 2

        anchors.top: parent.top
        anchors.topMargin: heading.visible ? heading.height + Metrics.scaled(26) : 0
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.max(cellSpan, Math.floor(parent.width / cellSpan) * cellSpan)
        cellWidth: cellSpan
        cellHeight: root.tileSize + Metrics.scaled(root.dense ? 52 : 84) + root.cellPadding
        clip: true
        focus: false
        keyNavigationEnabled: false
        boundsBehavior: Flickable.StopAtBounds
        model: root.profileCount + 1
        currentIndex: 0
        onCountChanged: if (currentIndex >= count)
        currentIndex = Math.max(0, count - 1)

        function columnCount() {
            return Math.max(1, Math.floor(width / Math.max(1, cellWidth)))
        }

        function activateCurrent() {
            if (currentItem)
                currentItem.accepted()
        }

        function requestContextMenu() {
            if (!currentItem || currentIndex >= root.profileCount)
                return false
            currentItem.contextRequested()
            return true
        }

        FastWheelHandler {
            flickable: grid
        }

        delegate: Item {
            id: cell

            required property int index

            readonly property var profile: cell.index < root.profileCount ? root.profiles[cell.index] : null

            width: grid.cellWidth
            height: grid.cellHeight

            function accepted() {
                tile.accepted()
            }

            function contextRequested() {
                tile.contextRequested()
            }

            ProfileTile {
                id: tile
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: root.cellPadding
                tileSize: root.tileSize
                addTile: cell.profile === null
                username: cell.profile ? String(cell.profile.userName || "Saved account") : "Add account"
                serverName: cell.profile ? String(cell.profile.serverName || "Jellyfin Server") : ""
                serverAddress: cell.profile ? String(cell.profile.serverHost || cell.profile.serverUrl || "") : ""
                needsSignIn: cell.profile ? Boolean(cell.profile.needsAuthentication) : false
                focused: cell.GridView.isCurrentItem && grid.activeFocus
                onAccepted: {
                    grid.currentIndex = cell.index
                    InputKeys.focus(grid)
                    if (cell.profile)
                    root.profileChosen(String(cell.profile.profileId || ""))
                    else
                    root.addRequested()
                }
                onContextRequested: {
                    if (!cell.profile)
                    return
                    grid.currentIndex = cell.index
                    InputKeys.focus(grid)
                    root.contextRequested(String(cell.profile.profileId || ""), tile, tile.serverName, String(
                                              cell.profile.serverUrl || ""))
                }
            }
        }
    }
}
