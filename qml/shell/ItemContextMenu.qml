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
    readonly property int menuPanelWidth: Math.min(windowWidth - menuEdgeMargin * 2, Math.max(320, Math.min(392,
                                                                                                            Math.round(
                                                                                                                windowWidth
                                                                                                                * 0.22))))
    readonly property int menuPanelHeight: menuOptions.length <= 0 ? 0 : menuOptions.length * menuRowHeight + (
                                                                         menuOptions.length - 1) * 4 + 16
    readonly property string itemId: item && item.movieId ? String(item.movieId) : ""
    readonly property string itemType: item && item.itemType ? String(item.itemType) : ""
    readonly property bool episodeOrSeason: itemType === "Episode" || itemType === "Season"
    readonly property bool hasProgress: Number(item && item.resumeTicks ? item.resumeTicks : 0) > 0
    readonly property bool partialEpisode: itemType === "Episode" && hasProgress && !playedState
    readonly property bool actionable: itemId.length > 0
    readonly property bool queueable: actionable && item && item.playable !== false
    readonly property string currentViewKind: String(App.currentViewKind || "")
    readonly property bool inPlaylist: currentViewKind === "playlist"
    readonly property bool inCollection: currentViewKind === "boxset" || currentViewKind === "collection"
    readonly property bool collectionEligible: actionable && (itemType === "Movie" || itemType === "Series" || itemType
                                                              === "Episode")
    readonly property bool canManagePlaylists: Management.currentUserCanManagePlaylists
    readonly property bool canManageCollections: Management.currentUserCanManageCollections
    readonly property bool canRenameItem: Management.currentUserCanRenameItems
    readonly property bool canDeleteItem: Management.currentUserCanDeleteItems

    signal playedToggled(string itemId, bool played)
    signal favoriteToggled(string itemId, bool favorite)
    signal clearProgressRequested(string itemId)
    signal openSeriesRequested(string seriesId, string seriesName)
    signal openSeasonRequested(string seriesId, string seasonId, string seasonName)
    signal mediaInfoRequested(var item)
    signal playNextRequested(var item)
    signal addToQueueRequested(var item)
    signal addToPlaylistRequested(var item)
    signal addToCollectionRequested(var item)
    signal removeFromParentRequested(var item)
    signal movePlaylistItemRequested(var item, int delta)
    signal renameRequested(var item)
    signal deleteRequested(var item)

    visible: opened
    focus: opened

    Connections {
        target: App
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
            options.push({
                             action: "series",
                             icon: "live_tv",
                             label: "Go to series",
                             checked: false
                         })
        if ((itemType === "Episode" && item.seriesId && item.seasonId) || (itemType === "Season" && item.seriesId
                                                                           && itemId))
            options.push({
                             action: "season",
                             icon: "video_library",
                             label: "Go to season",
                             checked: false
                         })
        if (actionable) {
            if (queueable) {
                options.push({
                                 action: "playNext",
                                 icon: "playlist_play",
                                 label: "Play next",
                                 checked: false
                             })
                options.push({
                                 action: "addQueue",
                                 icon: "queue_music",
                                 label: "Add to queue",
                                 checked: false
                             })
            }
            options.push({
                             action: "played",
                             icon: "check_circle",
                             label: playedState ? "Mark unwatched" : "Mark watched",
                             checked: playedState
                         })
            if (partialEpisode)
                options.push({
                                 action: "clear",
                                 icon: "replay",
                                 label: "Clear progress",
                                 checked: false
                             })
            options.push({
                             action: "favorite",
                             icon: favoriteState ? "favorite" : "favorite_border",
                             label: favoriteState ? "Remove favourite" : "Add favourite",
                             checked: favoriteState
                         })
            if (canManagePlaylists && queueable)
                options.push({
                                 action: "playlist",
                                 icon: "playlist_add",
                                 label: "Add to playlist",
                                 checked: false
                             })
            if (canManageCollections && collectionEligible)
                options.push({
                                 action: "collection",
                                 icon: "library_add",
                                 label: "Add to collection",
                                 checked: false
                             })
            if (inPlaylist && item.playlistItemId) {
                options.push({
                                 action: "moveUp",
                                 icon: "keyboard_arrow_up",
                                 label: "Move up",
                                 checked: false
                             })
                options.push({
                                 action: "moveDown",
                                 icon: "keyboard_arrow_down",
                                 label: "Move down",
                                 checked: false
                             })
                options.push({
                                 action: "removeParent",
                                 icon: "remove_circle",
                                 label: "Remove from playlist",
                                 checked: false
                             })
            } else if (inCollection && canManageCollections) {
                options.push({
                                 action: "removeParent",
                                 icon: "remove_circle",
                                 label: "Remove from collection",
                                 checked: false
                             })
            }
            if ((itemType === "Playlist" && canManagePlaylists) || canRenameItem)
                options.push({
                                 action: "rename",
                                 icon: "drive_file_rename_outline",
                                 label: "Rename",
                                 checked: false
                             })
            if (canDeleteItem)
                options.push({
                                 action: "delete",
                                 icon: "delete",
                                 label: "Delete",
                                 checked: false
                             })
        }
        if (item && (item.movieId || item.id || item.title || item.displayTitle || item.seriesName))
            options.push({
                             action: "info",
                             icon: "info",
                             label: "Media info",
                             checked: false
                         })
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

    function openForItem(item, anchor) {
        item = item || ({})
        anchorItem = anchor || null
        syncItemState()
        if (!rebuildMenu())
            return false
        menuIndex = 0
        menuPopup.open()
        InputKeys.focus(menuList)
        positionMenu()
        Qt.callLater(function () {
            InputKeys.focus(menuList)
            positionMenu()
        })
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
            openSeasonRequested(String(item.seriesId || ""), itemType === "Season" ? itemId : String(item.seasonId
                                                                                                     || ""), itemType
                                === "Season" ? String(item.title || seasonTitle()) : seasonTitle())
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
        if (menuList.handleKey(event.key))
            return true
        return true
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

            Keys.onPressed: event => {
                                if (root.handlePressed(event))
                                event.accepted = true
                            }
            Keys.onReleased: event => {
                                 if (root.handleReleased(event))
                                 event.accepted = true
                             }

            MenuListView {
                id: menuList
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4
                model: root.menuOptions
                currentIndex: root.menuIndex
                onCurrentIndexChanged: root.menuIndex = currentIndex
                onDismissed: root.closeMenu()
                onAccepted: index => root.activateMenuIndex(index)

                delegate: MenuRow {
                    required property int index
                    required property var modelData
                    width: menuList.width
                    label: modelData.label || ""
                    iconName: modelData.icon || "more_horiz"
                    checked: Boolean(modelData.checked)
                    highlighted: ListView.isCurrentItem
                    metricsWidth: root.windowWidth
                    rowHeight: root.menuRowHeight
                    onHovered: menuList.currentIndex = index
                    onActivated: root.activateMenuIndex(index)
                }
            }
        }
    }
}
