import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"
import "RoutePolicy.js" as RoutePolicy

KeyRouter {
    id: root
    focus: true

    readonly property string route: Router.route
    readonly property var routeArgs: Router.args || ({})
    property int lastLibraryIndex: 0
    property int lastGridIndex: 0
    property int lastSearchIndex: 0
    property bool diagnosticsVisible: false
    property bool diagnosticsLoaded: false
    property bool mediaInfoVisible: false
    property bool mediaInfoLoaded: false
    property bool itemMenuLoaded: false
    readonly property bool itemMenuOpen: itemContextMenuLoader.item ? itemContextMenuLoader.item.opened : false
    property var mediaInfoItem: ({})
    property bool managementOverlayVisible: false
    property string managementMode: ""
    property var managementItem: ({})
    property string managementInitialName: ""
    property bool managementFocusNamePending: false
    readonly property var managementTargets: managementMode === "collection" ? Management.collectionTargets :
                                                                               Management.playlistTargets
    property var personItem: ({})
    textInputActive: Qt.inputMethod.visible || InputKeys.isTextInputItem(root.Window.window
                                                                         ? root.Window.window.activeFocusItem : null)
    property var navigationTarget: routeStack
    activeTarget: managementOverlayVisible ? managementOverlayLoader.item : itemMenuOpen ? itemContextMenuLoader.item :
                                                                                           mediaInfoVisible
                                                                                           ? mediaInfoOverlayLoader.item :
                                                                                             hasPlayer
                                                                                             && player.visible
                                                                                             ? videoSurface :
                                                                                               navigationTarget
    backHandler: function () {
        return root.back()
    }
    globalHandler: function (key, phase, repeat, modifiers) {
        return root.globalShortcut(key, phase, repeat, modifiers)
    }
    readonly property var player: Player
    readonly property bool hasPlayer: true
    readonly property bool playerSessionActive: hasPlayer && player.sessionActive
    readonly property string errorTextValue: App.errorText
    readonly property bool busyValue: App.busy
    readonly property string busyTextValue: App.busyText
    property real keyboardAvoidance: 0

    function refreshKeyboardAvoidance() {
        if (!Qt.inputMethod.visible) {
            keyboardAvoidance = 0
            return
        }
        const window = root.Window.window
        const focusItem = window ? window.activeFocusItem : null
        if (!InputKeys.isTextInputItem(focusItem) || !focusItem.mapToItem) {
            keyboardAvoidance = 0
            return
        }
        const keyboardRect = Qt.inputMethod.keyboardRectangle
        const keyboardTop = keyboardRect && keyboardRect.height > 0 ? keyboardRect.y : root.height
        const focusPos = focusItem.mapToItem(root, 0, 0)
        const focusBottom = focusPos.y + focusItem.height + keyboardAvoidance
        const overlap = focusBottom + 24 - keyboardTop
        keyboardAvoidance = Math.max(0, Math.min(overlap, root.height * 0.45))
    }

    Behavior on keyboardAvoidance {
        enabled: !Theme.reducedMotion
        NumberAnimation {
            duration: 120
            easing.type: Easing.OutCubic
        }
    }

    Connections {
        target: Qt.inputMethod
        function onVisibleChanged() {
            root.refreshKeyboardAvoidance()
        }
        function onKeyboardRectangleChanged() {
            root.refreshKeyboardAvoidance()
        }
        function onAnchorRectangleChanged() {
            root.refreshKeyboardAvoidance()
        }
    }

    Connections {
        target: root.Window.window
        function onActiveFocusItemChanged() {
            root.refreshKeyboardAvoidance()
        }
    }

    Connections {
        target: App
        function onAggressiveMemoryPressure() {
            if (!root.diagnosticsVisible)
                root.diagnosticsLoaded = false
            if (!root.mediaInfoVisible)
                root.mediaInfoLoaded = false
            if (!root.itemMenuOpen)
                root.itemMenuLoaded = false
        }
        function onToastMessage(message) {
            toast.show(message)
        }
    }
    function defaultRoute() {
        return Session.authenticated ? "home" : "login"
    }

    Connections {
        target: Session
        function onAuthenticatedStateChanged() {
            if (Session.authenticated)
                Router.replace("home")
            else
                Router.reset("login")
        }
    }

    Component.onCompleted: Router.reset(root.defaultRoute())

    Connections {
        target: root.player
        function onVisibleChanged() {
            if (root.hasPlayer && root.player.visible) {
                root.focusPlayerInput()
            } else {
                root.navigationTarget = routeStack
                InputKeys.focus(routeStack)
            }
        }
    }

    function focusPlayerInput() {
        videoSurface.focusInput()
    }

    function pushRoute(nextRoute, args) {
        Router.push(nextRoute, args || ({}))
        navigationTarget = routeStack
        InputKeys.focus(routeStack)
    }

    function openDetailsRoute(request) {
        const normalized = RoutePolicy.normalizeDetailsRoute(request, Browse.items, route)
        if (!normalized) {
            const focusIndex = Math.max(0, Number(request && request.focusIndex !== undefined ? request.focusIndex : 0))
            console.warn("details route ignored: missing item id", request ? request.source : "", focusIndex)
            return false
        }

        if (normalized.source === "movies")
            lastGridIndex = normalized.focusIndex
        else if (normalized.source === "search" || normalized.source === "suggestion")
            lastSearchIndex = normalized.focusIndex

        if (route === "itemDetails") {
            Router.replace("itemDetails", normalized)
            InputKeys.focus(routeStack)
            return true
        }
        pushRoute("itemDetails", normalized)
        return true
    }

    function openDetailsAt(model, index, source, returnRoute) {
        const nextModel = model || (Browse.items)
        const request = RoutePolicy.detailsRouteAt(nextModel, index, source, returnRoute, route)
        return request ? openDetailsRoute(request) : false
    }

    function replaceRoute(nextRoute, args) {
        Router.replace(nextRoute, args || ({}))
        navigationTarget = routeStack
        InputKeys.focus(routeStack)
    }

    function goHome() {
        Router.reset("home")
        App.goHome()
        navigationTarget = routeStack
        InputKeys.focus(routeStack)
    }

    function switchUser() {
        Router.reset("login")
        App.switchUser()
        navigationTarget = routeStack
        InputKeys.focus(routeStack)
    }

    function releaseTextInput() {
        InputKeys.focus(routeStack)
        Qt.inputMethod.hide()
    }

    function back() {
        if (textInputActive) {
            releaseTextInput()
            return true
        }
        if (navBar.visible && navBar.syncPlayMenuOpen) {
            navBar.closeSyncPlayMenu()
            return true
        }
        if (diagnosticsVisible) {
            diagnosticsVisible = false
            return true
        }
        if (itemMenuOpen && itemContextMenuLoader.item) {
            itemContextMenuLoader.item.closeMenu()
            return true
        }
        if (managementOverlayVisible) {
            closeManagementOverlay()
            return true
        }
        if (mediaInfoVisible) {
            mediaInfoVisible = false
            return true
        }
        if (root.hasPlayer && root.player.visible) {
            if (root.player.backAllowed)
                root.player.stopWithReason("shell-back-fallback")
            return true
        }
        if (routeStack.back())
            return true
        if (route === "itemDetails") {
            Router.pop(String(routeArgs.returnRoute || "libraryGrid"))
            InputKeys.focus(routeStack)
            return true
        }
        if (route === "libraries" || route === "libraryGrid") {
            goHome()
            return true
        }
        if (route === "home" || route === "login")
            return false
        if (Router.canPop) {
            Router.pop(route === "personDetails" ? "itemDetails" : "home")
            InputKeys.focus(routeStack)
            return true
        }
        return false
    }

    function openContextMenu() {
        openItemMenu(currentMediaItem(), null)
    }

    function openItemMenu(item, anchorItem) {
        itemMenuLoaded = true
        return itemContextMenuLoader.item ? itemContextMenuLoader.item.openForItem(item || ({}), anchorItem || null) :
                                            false

    }

    function openMediaInfo(item) {
        mediaInfoItem = item || ({})
        if (mediaInfoItem.movieId)
            Content.loadItemDetail(mediaInfoItem.movieId)
        mediaInfoLoaded = true
        mediaInfoVisible = true
    }

    function itemTitle(item) {
        return String(item && (item.displayTitle || item.title || item.seriesName || item.name) || "Selected item")
    }

    function focusManagementOverlay() {
        const overlay = managementOverlayLoader.item
        if (overlay)
            InputKeys.focus(overlay)
    }

    function focusManagementNameField() {
        const overlay = managementOverlayLoader.item
        if (!overlay) {
            managementFocusNamePending = true
            return
        }
        managementFocusNamePending = false
        overlay.focusNameField()
    }

    function openManagementPicker(mode, item) {
        managementMode = mode
        managementItem = item || ({})
        managementInitialName = ""
        managementOverlayVisible = true
        Management.refreshTargets(mode)
        Qt.callLater(focusManagementOverlay)
    }

    function openRenamePrompt(item) {
        managementMode = "rename"
        managementItem = item || ({})
        managementInitialName = itemTitle(managementItem)
        managementOverlayVisible = true
        managementFocusNamePending = true
        Qt.callLater(focusManagementNameField)
    }

    function openDeleteConfirm(item) {
        managementMode = "delete"
        managementItem = item || ({})
        managementInitialName = ""
        managementOverlayVisible = true
        Qt.callLater(focusManagementOverlay)
    }

    function openRemoveConfirm(item) {
        managementMode = "remove"
        managementItem = item || ({})
        managementInitialName = ""
        managementOverlayVisible = true
        Qt.callLater(focusManagementOverlay)
    }

    function closeManagementOverlay() {
        managementOverlayVisible = false
        managementMode = ""
        managementItem = ({})
        managementInitialName = ""
        managementFocusNamePending = false
        Qt.inputMethod.hide()
        InputKeys.focus(routeStack)
    }

    function submitManagementCreate(name) {
        const trimmed = String(name || "").trim()
        if (trimmed.length <= 0)
            return
        if (managementMode === "playlist")
            Management.createPlaylistForItem(trimmed, managementItem)
        else if (managementMode === "collection")
            Management.createCollectionForItem(trimmed, managementItem)
        else if (managementMode === "rename")
            Management.renameItem(managementItem, trimmed)
        closeManagementOverlay()
    }

    function submitManagementTarget(index) {
        if (index < 0)
            return
        if (index === 0) {
            focusManagementNameField()
            return
        }
        const target = managementTargets[index - 1] || ({})
        const targetId = String(target.movieId || target.id || "")
        if (targetId.length <= 0)
            return
        if (managementMode === "playlist")
            Management.addItemToPlaylist(targetId, managementItem)
        else if (managementMode === "collection")
            Management.addItemToCollection(targetId, managementItem)
        closeManagementOverlay()
    }

    function confirmManagementAction() {
        if (managementMode === "delete")
            Management.deleteItem(managementItem)
        else if (managementMode === "remove")
            Management.removeItemFromCurrentParent(managementItem)
        closeManagementOverlay()
    }

    function openPerson(person) {
        personItem = person || ({})
        if (Content && personItem.personId)
            Content.loadPersonItems(personItem.personId)
        if (route === "personDetails") {
            InputKeys.focus(routeStack)
            return
        }
        pushRoute("personDetails")
    }

    function currentMediaItem() {
        if (route === "itemDetails") {
            const details = RoutePolicy.detailsContext(routeArgs, Browse.items)
            if (details.index >= 0)
                return details.item
        }
        if (route === "search" && Search.results) {
            const searchCount = Search.results.rowCount()
            if (searchCount > 0) {
                const searchIdx = Math.max(0, Math.min(lastSearchIndex, searchCount - 1))
                return Search.results.get(searchIdx) || ({})
            }
        }
        const count = Browse.items.rowCount()
        if (count <= 0)
            return ({})
        const idx = Math.max(0, Math.min(lastGridIndex, count - 1))
        return Browse.items.get(idx) || ({})
    }

    function focusNavBar() {
        if (route === "login")
            return
        navigationTarget = navBar
        InputKeys.focus(navBar)
        navBar.focusCurrent()
    }

    function focusContent() {
        if (root.hasPlayer && root.player.visible)
            return
        navigationTarget = routeStack
        InputKeys.focus(routeStack)
    }

    function setUiScale(value) {
        const step = 0.05
        const rounded = Math.round(Number(value || 1.0) / step) * step
        Metrics.userUiScale = Math.max(0.75, Math.min(1.5, rounded))
    }

    function globalShortcut(key, phase, repeat, modifiers) {
        if (phase !== "release" || repeat || textInputActive)
            return false
        if (modifiers & Qt.ControlModifier) {
            if (key === Qt.Key_Plus || key === Qt.Key_Equal) {
                setUiScale(Metrics.userUiScale + 0.05)
                return true
            }
            if (key === Qt.Key_Minus || key === Qt.Key_Underscore) {
                setUiScale(Metrics.userUiScale - 0.05)
                return true
            }
            if (key === Qt.Key_0) {
                setUiScale(1.0)
                return true
            }
            if (key === Qt.Key_D) {
                diagnosticsLoaded = true
                diagnosticsVisible = !diagnosticsVisible
                return true
            }
        }
        if (key === Qt.Key_Slash) {
            pushRoute("search")
            return true
        }
        if (key === Qt.Key_I) {
            if (mediaInfoVisible) {
                mediaInfoVisible = false
                mediaInfoItem = ({})
            } else {
                openMediaInfo(currentMediaItem())
            }
            return true
        }
        if (key === Qt.Key_M || key === Qt.Key_Menu) {
            openContextMenu()
            return true
        }
        if (key === Qt.Key_H || key === Qt.Key_L)
            return route(activeTarget, key === Qt.Key_H ? Qt.Key_Left : Qt.Key_Right, "press", false)
        if (key === Qt.Key_Q && root.playerSessionActive) {
            root.player.stopWithReason("shortcut-q")
            return true
        }
        return false
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
        visible: !(root.hasPlayer && root.player.visible)
    }

    ColumnLayout {
        id: contentLayer
        anchors.fill: parent
        transform: Translate {
            y: -root.keyboardAvoidance
        }
        spacing: 0
        visible: !(root.hasPlayer && root.player.visible)
        enabled: !(root.hasPlayer && root.player.visible)

        TopBar {
            id: navBar
            Layout.fillWidth: true
            Layout.preferredHeight: route === "login" ? 0 : Metrics.topBarHeight(root.width)
            visible: route !== "login"
            z: 1
            currentRoute: root.route
            onActiveFocusChanged: if (activeFocus)
                                      root.navigationTarget = navBar
            onNavigate: r => {
                            if (r === "home")
                            root.goHome()
                            else if (r === "settings")
                            root.pushRoute("settings")
                            else
                            root.pushRoute(r)
                        }
            onContentRequested: root.focusContent()
        }

        RouteStack {
            id: routeStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            route: root.route
            shell: root
            focus: !(root.hasPlayer && root.player.visible)
            onActiveFocusChanged: if (activeFocus)
                                      root.navigationTarget = routeStack
        }
    }

    VideoSurface {
        id: videoSurface
        anchors.fill: parent
        active: root.hasPlayer && root.player.visible
        mediaInfoVisible: root.mediaInfoVisible
        diagnosticsVisible: root.diagnosticsVisible
        z: 19
    }

    Loader {
        id: busyOverlayLoader
        anchors.fill: parent
        z: 40
        active: root.busyValue && !(root.hasPlayer && root.player.visible)
        asynchronous: true
        source: active ? "BusyOverlay.qml" : ""

        Binding {
            target: busyOverlayLoader.item
            property: "text"
            value: root.busyTextValue
            when: busyOverlayLoader.item
        }
    }

    Loader {
        id: managementOverlayLoader
        anchors.fill: parent
        z: 57
        active: root.managementOverlayVisible
        asynchronous: true
        sourceComponent: ManagementDialog {
            mode: root.managementMode
            item: root.managementItem
            targets: root.managementTargets
            currentViewKind: App.currentViewKind
            nameDraft: root.managementInitialName
            targetIndex: 0
            onDismissed: root.closeManagementOverlay()
            onCreateRequested: name => root.submitManagementCreate(name)
            onTargetRequested: index => root.submitManagementTarget(index)
            onConfirmRequested: root.confirmManagementAction()
        }
        onLoaded: {
            if (!item)
                return
            if (root.managementFocusNamePending) {
                root.managementFocusNamePending = false
                item.focusNameField()
            } else {
                InputKeys.focus(item)
            }
        }
    }

    Loader {
        id: itemContextMenuLoader
        anchors.fill: parent
        z: 58
        active: root.itemMenuLoaded
        sourceComponent: ItemContextMenu {
            onPlayedToggled: (itemId, played) => App.setPlayed(itemId, played)
            onFavoriteToggled: (itemId, favorite) => App.setFavorite(itemId, favorite)
            onClearProgressRequested: itemId => App.clearProgress(itemId)
            onOpenSeriesRequested: (seriesId, seriesName) => {
                                       if (seriesId.length <= 0)
                                       return
                                       root.replaceRoute("libraryGrid")
                                       App.openSeriesById(seriesId, seriesName)
                                   }
            onOpenSeasonRequested: (seriesId, seasonId, seasonName) => {
                                       if (seriesId.length <= 0)
                                       return
                                       root.replaceRoute("libraryGrid")
                                       App.openSeasonById(seriesId, seasonId, seasonName)
                                   }
            onMediaInfoRequested: item => root.openMediaInfo(item)
            onPlayNextRequested: item => App.playNextFromItem(item)
            onAddToQueueRequested: item => App.addToQueueFromItem(item)
            onAddToPlaylistRequested: item => root.openManagementPicker("playlist", item)
            onAddToCollectionRequested: item => root.openManagementPicker("collection", item)
            onRemoveFromParentRequested: item => root.openRemoveConfirm(item)
            onMovePlaylistItemRequested: (item, delta) => Management.movePlaylistItemInCurrent(item, delta)
            onRenameRequested: item => root.openRenamePrompt(item)
            onDeleteRequested: item => root.openDeleteConfirm(item)
        }
    }

    Loader {
        id: mediaInfoOverlayLoader
        anchors.fill: parent
        z: 59
        active: root.mediaInfoLoaded
        sourceComponent: MediaInfoOverlay {
            visible: root.mediaInfoVisible
            item: visible ? (root.mediaInfoItem && Object.keys(root.mediaInfoItem).length > 0 ? root.mediaInfoItem :
                                                                                                root.currentMediaItem(
                                                                                                    )) : ({})
            shell: root
            onClosed: {
                root.mediaInfoVisible = false
                root.mediaInfoItem = ({})
            }
        }
    }

    Loader {
        anchors.fill: parent
        z: 61
        active: root.diagnosticsLoaded
        sourceComponent: DiagnosticsOverlay {
            visible: root.diagnosticsVisible && !(root.hasPlayer && root.player.visible)
            route: root.route
            focusedItemId: RoutePolicy.itemIdFor(root.currentMediaItem())
        }
    }
    ToastLayer {
        id: toast
        anchors.fill: parent
        z: 70
    }

    Surface {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 32
        width: Math.min(parent.width * 0.72, 960)
        height: root.errorTextValue.length > 0 ? errorText.implicitHeight + 28 : 0
        visible: root.errorTextValue.length > 0
        baseColor: Theme.errorPanel
        z: 80
        AppText {
            id: errorText
            anchors.centerIn: parent
            width: Math.max(0, parent.width - 28)
            text: root.errorTextValue
            color: Theme.errorText
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        MouseArea {
            anchors.fill: parent
            onClicked: App.clearError()
        }
    }
}
