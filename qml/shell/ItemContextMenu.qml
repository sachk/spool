import QtQuick
import QtQuick.Controls.Basic as QQC
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root

    property var item: ({})
    property var anchorItem: null
    property int menuIndex: 0
    property var menuOptions: []
    property bool favoriteState: Boolean(item && item.favorite)
    property bool playedState: Boolean(item && item.played)
    readonly property bool opened: menuPopup.opened
    readonly property int windowWidth: root.Window.window ? root.Window.window.width : 1920
    readonly property int menuEdgeMargin: Math.max(12, Metrics.gap(windowWidth))
    readonly property int menuRowHeight: Math.max(46, Metrics.controlHeight(windowWidth))
    readonly property int menuPanelWidth: Math.min(windowWidth - menuEdgeMargin * 2,
                                                   Math.max(320, Math.min(392, Math.round(windowWidth * 0.22))))
    readonly property int menuPanelHeight: menuOptions.length <= 0 ? 0
                                                                  : menuOptions.length * menuRowHeight
                                                                    + (menuOptions.length - 1) * 4 + 16
    readonly property string itemId: item && item.movieId ? String(item.movieId) : ""
    readonly property string itemType: item && item.itemType ? String(item.itemType) : ""
    readonly property bool episodeOrSeason: itemType === "Episode" || itemType === "Season"
    readonly property bool hasProgress: Number(item && item.resumeTicks ? item.resumeTicks : 0) > 0
    readonly property bool partialEpisode: itemType === "Episode" && hasProgress && !playedState
    readonly property bool actionable: itemId.length > 0
    readonly property bool queueable: actionable && item && item.playable !== false
    readonly property string currentViewKind: appController ? String(appController.currentViewKind || "") : ""
    readonly property bool inPlaylist: currentViewKind === "playlist"
    readonly property bool inCollection: currentViewKind === "boxset" || currentViewKind === "collection"
    readonly property bool collectionEligible: actionable
                                               && (itemType === "Movie" || itemType === "Series" || itemType === "Episode")
    readonly property bool canManagePlaylists: appController ? appController.currentUserCanManagePlaylists : false
    readonly property bool canManageCollections: appController ? appController.currentUserCanManageCollections : false
    readonly property bool canRenameItem: appController ? appController.currentUserCanRenameItems : false
    readonly property bool canDeleteItem: appController ? appController.currentUserCanDeleteItems : false

    signal playedToggled(string itemId, bool played)
    signal favoriteToggled(string itemId, bool favorite)
    signal clearProgressRequested(string itemId)
    signal openSeriesRequested(string seriesId, string seriesName)
    signal openSeasonRequested(string seriesId, string seasonId, string seasonName)
    signal mediaInfoRequested(var snapshot)
    signal playNextRequested(var snapshot)
    signal addToQueueRequested(var snapshot)
    signal addToPlaylistRequested(var snapshot)
    signal addToCollectionRequested(var snapshot)
    signal removeFromParentRequested(var snapshot)
    signal movePlaylistItemRequested(var snapshot, int delta)
    signal renameRequested(var snapshot)
    signal deleteRequested(var snapshot)

    visible: opened
    focus: opened

    Connections {
        target: appController
        enabled: root.opened
        function onItemFavoriteChanged(changedItemId, favorite) {
            if (root.itemId === changedItemId)
                root.favoriteState = favorite
        }
        function onItemPlayedChanged(changedItemId, played) {
            if (root.itemId === changedItemId)
                root.playedState = played
        }
    }

    function menuParentItem() {
        return root.Window.window ? root.Window.window.contentItem : root
    }

    function clamp(value, minimum, maximum) {
        return Math.max(minimum, Math.min(maximum, value))
    }

    function syncItemState() {
        favoriteState = Boolean(item && item.favorite)
        playedState = Boolean(item && item.played)
    }

    function seasonTitle() {
        return item && item.seasonNumber > 0 ? "Season " + item.seasonNumber : "Season"
    }

    function rebuildMenu() {
        const options = []
        if (episodeOrSeason && item.seriesId)
            options.push({ action: "series", icon: "live_tv", label: "Go to series", checked: false })
        if ((itemType === "Episode" && item.seriesId && item.seasonId)
                || (itemType === "Season" && item.seriesId && itemId))
            options.push({ action: "season", icon: "video_library", label: "Go to season", checked: false })
        if (actionable) {
            if (queueable) {
                options.push({ action: "playNext", icon: "playlist_play", label: "Play next", checked: false })
                options.push({ action: "addQueue", icon: "queue_music", label: "Add to queue", checked: false })
            }
            options.push({
                action: "played",
                icon: "check_circle",
                label: playedState ? "Mark unwatched" : "Mark watched",
                checked: playedState
            })
            if (partialEpisode)
                options.push({ action: "clear", icon: "replay", label: "Clear progress", checked: false })
            options.push({
                action: "favorite",
                icon: favoriteState ? "favorite" : "favorite_border",
                label: favoriteState ? "Remove favourite" : "Add favourite",
                checked: favoriteState
            })
            if (canManagePlaylists && queueable)
                options.push({ action: "playlist", icon: "playlist_add", label: "Add to playlist", checked: false })
            if (canManageCollections && collectionEligible)
                options.push({ action: "collection", icon: "library_add", label: "Add to collection", checked: false })
            if (inPlaylist && item.playlistItemId) {
                options.push({ action: "moveUp", icon: "keyboard_arrow_up", label: "Move up", checked: false })
                options.push({ action: "moveDown", icon: "keyboard_arrow_down", label: "Move down", checked: false })
                options.push({ action: "removeParent", icon: "remove_circle", label: "Remove from playlist", checked: false })
            } else if (inCollection && canManageCollections) {
                options.push({ action: "removeParent", icon: "remove_circle", label: "Remove from collection", checked: false })
            }
            if ((itemType === "Playlist" && canManagePlaylists) || canRenameItem)
                options.push({ action: "rename", icon: "drive_file_rename_outline", label: "Rename", checked: false })
            if (canDeleteItem)
                options.push({ action: "delete", icon: "delete", label: "Delete", checked: false })
        }
        if (item && (item.movieId || item.id || item.title || item.displayTitle || item.seriesName))
            options.push({ action: "info", icon: "info", label: "Media info", checked: false })
        menuOptions = options
        return menuOptions.length > 0
    }

    function positionMenu() {
        const target = menuParentItem()
        const edge = root.menuEdgeMargin
        const maxX = Math.max(edge, target.width - menuPopup.width - edge)
        const maxY = Math.max(edge, target.height - menuPopup.height - edge)
        let desiredX = Math.round((target.width - menuPopup.width) / 2)
        let desiredY = Math.round((target.height - menuPopup.height) / 2)
        if (anchorItem) {
            const anchor = anchorItem.mapToItem(target, 0, 0)
            desiredX = anchor.x + anchorItem.width - menuPopup.width
            const belowY = anchor.y + Math.min(anchorItem.height, root.menuRowHeight) + 8
            const aboveY = anchor.y - menuPopup.height - 8
            desiredY = belowY + menuPopup.height <= target.height - edge ? belowY : aboveY
        }
        menuPopup.x = clamp(desiredX, edge, maxX)
        menuPopup.y = clamp(desiredY, edge, maxY)
    }

    function openForItem(snapshot, anchor) {
        item = snapshot || ({})
        anchorItem = anchor || null
        syncItemState()
        if (!rebuildMenu())
            return false
        menuIndex = 0
        InputKeys.focus(root)
        menuPopup.open()
        positionMenu()
        Qt.callLater(positionMenu)
        return true
    }

    function closeMenu() {
        if (menuPopup.opened) {
            menuPopup.close()
            return
        }
        item = ({})
        anchorItem = null
        menuOptions = []
    }

    function activateMenuIndex(index) {
        if (index < 0 || index >= menuOptions.length)
            return
        const action = menuOptions[index].action
        if (action === "series") {
            openSeriesRequested(String(item.seriesId || ""), String(item.seriesName || ""))
        } else if (action === "season") {
            openSeasonRequested(String(item.seriesId || ""),
                                itemType === "Season" ? itemId : String(item.seasonId || ""),
                                itemType === "Season" ? String(item.title || seasonTitle()) : seasonTitle())
        } else if (action === "playNext") {
            playNextRequested(item)
        } else if (action === "addQueue") {
            addToQueueRequested(item)
        } else if (action === "played") {
            playedState = !playedState
            playedToggled(itemId, playedState)
        } else if (action === "clear") {
            playedState = false
            clearProgressRequested(itemId)
        } else if (action === "favorite") {
            favoriteState = !favoriteState
            favoriteToggled(itemId, favoriteState)
        } else if (action === "playlist") {
            addToPlaylistRequested(item)
        } else if (action === "collection") {
            addToCollectionRequested(item)
        } else if (action === "removeParent") {
            removeFromParentRequested(item)
        } else if (action === "moveUp") {
            movePlaylistItemRequested(item, -1)
        } else if (action === "moveDown") {
            movePlaylistItemRequested(item, 1)
        } else if (action === "rename") {
            renameRequested(item)
        } else if (action === "delete") {
            deleteRequested(item)
        } else if (action === "info") {
            mediaInfoRequested(item)
        }
        closeMenu()
    }

    function handlePressed(event) {
        return opened
    }

    function handleReleased(event) {
        if (!opened)
            return false
        if (InputKeys.isBackEvent(event, true) || InputKeys.isHorizontal(event.key)) {
            closeMenu()
            return true
        }
        if (event.key === Qt.Key_Up) {
            menuIndex = Math.max(0, menuIndex - 1)
            return true
        }
        if (event.key === Qt.Key_Down) {
            menuIndex = Math.min(menuOptions.length - 1, menuIndex + 1)
            return true
        }
        if (InputKeys.isAccept(event.key)) {
            activateMenuIndex(menuIndex)
            return true
        }
        return true
    }

    component MenuRow: Item {
        id: rowRoot
        property int optionIndex: 0
        property string iconName: "info"
        property string label: ""
        property bool checked: false

        width: root.menuPanelWidth - 16
        height: root.menuRowHeight

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusMedium
            color: root.menuIndex === rowRoot.optionIndex ? Theme.focusedFill
                                                          : rowRoot.checked ? Theme.accentPanel : "transparent"
            border.width: root.menuIndex === rowRoot.optionIndex ? 1 : 0
            border.color: Theme.accent
            antialiasing: true
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 12

            MaterialIcon {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                name: rowRoot.iconName
                iconSize: 22
                iconColor: rowRoot.checked ? Theme.accent : Theme.textSecondary
            }

            AppText {
                Layout.fillWidth: true
                text: rowRoot.label
                color: Theme.textPrimary
                font.pixelSize: Metrics.bodyPx(root.windowWidth)
                font.weight: root.menuIndex === rowRoot.optionIndex ? Font.DemiBold : Font.Medium
                elide: Text.ElideRight
                maximumLineCount: 1
                verticalAlignment: Text.AlignVCenter
            }

            MaterialIcon {
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                visible: rowRoot.checked
                name: "done"
                iconSize: 21
                iconColor: Theme.accent
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onEntered: root.menuIndex = rowRoot.optionIndex
            onClicked: root.activateMenuIndex(rowRoot.optionIndex)
        }
    }

    QQC.Popup {
        id: menuPopup

        parent: root.Window.window ? root.Window.window.contentItem : root
        width: root.menuPanelWidth
        height: root.menuPanelHeight
        padding: 0
        modal: false
        dim: false
        focus: true
        closePolicy: QQC.Popup.CloseOnEscape | QQC.Popup.CloseOnPressOutside
        onClosed: root.closeMenu()

        background: Rectangle {
            radius: Theme.radiusPanel
            color: Theme.floatingPanel
            border.width: 1
            border.color: Theme.borderStrong
            antialiasing: true
        }

        contentItem: Item {
            clip: true
            focus: true

            Keys.onPressed: (event) => {
                if (root.handlePressed(event))
                    event.accepted = true
            }
            Keys.onReleased: (event) => {
                if (root.handleReleased(event))
                    event.accepted = true
            }

            Column {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                Repeater {
                    model: root.menuOptions.length
                    delegate: MenuRow {
                        required property int index
                        readonly property var option: root.menuOptions[index] || ({})
                        optionIndex: index
                        iconName: option.icon || "more_horiz"
                        label: option.label || ""
                        checked: Boolean(option.checked)
                    }
                }
            }
        }
    }
}
