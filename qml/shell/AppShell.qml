import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"
import "RoutePolicy.js" as RoutePolicy

KeyRouter {
    id: root
    onWidthChanged: Metrics.viewportWidth = width
    onHeightChanged: Metrics.viewportHeight = height

    // The shell is the one place that knows how big the window is and what
    // the user asked the interface to be scaled to. It hands both to Metrics
    // so nothing under qml/theme has to reach for a backend singleton.
    Binding {
        target: Metrics
        property: "zoomPercent"
        value: Settings.uiScalePercent
        restoreMode: Binding.RestoreNone
    }
    focus: true
    backspaceNavigatesInTextInput: Platform.isTV
    webOsScanCodes: Platform.isTV

    readonly property string route: Router.route
    readonly property var routeArgs: Router.args || ({})
    property bool diagnosticsVisible: false
    property string switchUserReturnProfileId: ""
    property string switchUserReturnRoute: ""
    property var switchUserReturnArgs: ({})
    property bool switchUserReturnPending: false
    readonly property bool canCancelSwitchUser: switchUserReturnProfileId.length > 0
    property bool mediaInfoVisible: false
    property bool itemMenuLoaded: false
    property int uiScaleShortcutKey: 0
    property int searchShortcutKey: 0
    property int settingsShortcutKey: 0
    readonly property bool itemMenuOpen: itemContextMenuLoader.item ? itemContextMenuLoader.item.opened : false
    readonly property bool tlsTrustPending: TlsTrust.pending
    property var mediaInfoItem: ({})
    property bool managementOverlayVisible: false
    property string managementMode: ""
    property var managementItem: ({})
    property var personItem: ({})
    property var pendingPlaybackBackItem: ({})
    textInputActive: Qt.inputMethod.visible || InputKeys.isTextInputItem(root.Window.window
                                                                         ? root.Window.window.activeFocusItem : null)
    property var navigationTarget: routeStack
    activeTarget: tlsTrustPending ? tlsTrustDialog : managementOverlayVisible ? managementOverlayLoader.item :
                                                                                itemMenuOpen
                                                                                ? itemContextMenuLoader.item :
                                                                                  mediaInfoVisible
                                                                                  ? mediaInfoOverlayLoader.item :
                                                                                    hasPlayer && player.visible
                                                                                    ? videoSurface : navigationTarget
    backHandler: function () {
        return root.back()
    }
    globalHandler: function (key, phase, repeat, modifiers) {
        return root.globalShortcut(key, phase, repeat, modifiers)
    }
    readonly property var player: Player
    readonly property bool hasPlayer: true
    readonly property bool playerSessionActive: hasPlayer && player.sessionActive
    // The player owns the screen for a whole queue step, not only while a file
    // is decoding. mpv drops the surface the instant it reports end-of-file and
    // the next item takes most of a second to negotiate, so keying the shell on
    // player.visible alone flashed the details page between every two tracks.
    readonly property bool playerHoldsScreen: hasPlayer && (player.visible || App.playbackTransition)
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
        const overlap = focusBottom + Metrics.scaled(24) - keyboardTop
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
        target: NativeWindow
        function onPointerBackRequested() {
            root.back()
        }
        function onPointerForwardRequested() {
            root.forward()
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
        function onInitializedChanged() {
            if (App.initialized)
                root.applyInitializedRoute()
        }
        function onAggressiveMemoryPressure() {
            if (!root.itemMenuOpen)
                root.itemMenuLoaded = false
            routeStack.trim()
        }
        function onToastMessage(message) {
            toast.show(message)
        }
        function onRemoteUiActionRequested(action) {
            root.handleRemoteUiAction(action)
        }
        function onRemoteMessageRequested(message) {
            remoteMessageText.text = message
            remoteMessage.visible = true
            remoteMessageTimer.restart()
        }
    }
    function showToastAction(message, actionText, callback) {
        toast.showAction(message, actionText, callback)
    }

    function defaultRoute() {
        return Session.authenticated ? "home" : "login"
    }

    function restoreRecoveredRoute() {
        if (!Router.recoveryPending || !Session.authenticated)
            return false
        const args = root.routeArgs
        if (root.route === "libraryGrid") {
            const libraryId = String(args.libraryId || "")
            if (libraryId.length <= 0) {
                Router.reset("home")
            } else if (!App.openLibraryById(libraryId)) {
                if (Libraries.count > 0)
                    Router.reset("home")
                else
                    return false
            }
        } else if (root.route === "itemDetails") {
            const itemId = String(args.itemId || "")
            if (itemId.length <= 0) {
                Router.reset("home")
            } else {
                Content.prepareLinkedItem(itemId, String(args.title || "Selected item"), String(args.itemType || "Video"),
                                          String(args.seriesId || ""), String(args.title || ""), String(args.seasonId
                                                                                                        || ""))

                const restored = Object.assign({}, args)
                restored.model = Content.linkedItems
                Router.replace("itemDetails", restored)
            }
        } else if (root.route === "personDetails") {
            const personId = String(args.personId || "")
            if (personId.length <= 0) {
                Router.reset("home")
            } else {
                personItem = {
                    id: personId,
                    name: String(args.personName || "Person"),
                    role: String(args.personRole || ""),
                    type: String(args.personType || "Person")
                }
                Content.loadPersonItems(personItem.id)
            }
        }
        Router.finishRecovery()
        return true
    }

    function applyInitializedRoute() {
        if (Router.recoveryPending) {
            if (root.restoreRecoveredRoute())
                return
            if (Session.authenticated)
                return
        }
        Router.reset(root.defaultRoute())
    }

    Connections {
        target: Session
        function onAuthenticatedStateChanged() {
            if (Session.authenticated && root.switchUserReturnPending) {
                root.completeSwitchUserReturn()
                return
            }
            if (Session.authenticated)
                root.clearSwitchUserReturn()
            if (Session.authenticated && root.restoreRecoveredRoute())
                return
            if (Session.authenticated)
                Router.replace(root.defaultRoute())
            else
                Router.reset(root.defaultRoute())
        }
    }

    // Linux native rendering goes through the platform FreeType/fontconfig
    // path, including the user's antialiasing and subpixel policy. Light
    // hinting keeps baselines aligned without snapping stems to whole pixels,
    // which is what small labels were being coarsened by. The TV keeps full
    // hinting for its 1080p scene; other desktops retain Qt's scalable
    // distance-field rendering.
    Component.onCompleted: {
        if (!Platform.isTV && Qt.platform.os === "linux") {
            Theme.normalTextRenderType = Text.NativeRendering
            Typography.sansHinting = Font.PreferVerticalHinting
        } else if (!Platform.isTV) {
            Theme.normalTextRenderType = Text.QtRendering
        }
        // The TV keeps its two-step text entry: there the field taking focus
        // is what raises the on-screen keyboard, so the row stays the D-pad
        // target until Select is pressed.
        Theme.textEntryFollowsFocus = !Platform.isTV
        if (App.initialized)
            root.applyInitializedRoute()
    }

    Connections {
        target: Libraries
        function onCountChanged() {
            if (Router.recoveryPending && root.route === "libraryGrid")
                root.restoreRecoveredRoute()
        }
    }

    Connections {
        target: root.player
        function onVisibleChanged() {
            if (root.hasPlayer && root.player.visible) {
                root.preparePlaybackBackNavigation(PlayQueue.currentIndex >= 0 ? PlayQueue.get(PlayQueue.currentIndex) :
                                                                                 ({}))
                root.focusPlayerInput()
            } else if (App.playbackTransition) {
                // Stepping to the next item, not leaving playback. Navigating
                // here would load details — item plus similar items — for the
                // track that just ended, behind a player surface the shell is
                // deliberately still holding. Nobody sees it and it costs a
                // round trip per track.
            } else {
                root.finishPlaybackBackNavigation()
                root.navigationTarget = routeStack
                InputKeys.focus(routeStack)
            }
        }
    }

    function focusPlayerInput() {
        videoSurface.focusInput()
    }

    function preparePlaybackBackNavigation(item) {
        if (RoutePolicy.itemIdFor(item).length <= 0) {
            pendingPlaybackBackItem = ({})
            return
        }
        pendingPlaybackBackItem = item
        routeStack.preloadRoute("itemDetails")
    }

    function finishPlaybackBackNavigation() {
        const item = pendingPlaybackBackItem
        pendingPlaybackBackItem = ({})
        const itemId = RoutePolicy.itemIdFor(item)
        if (itemId.length <= 0)
            return false
        const returnRoute = route === "itemDetails" ? String(routeArgs.returnRoute || "home") : route
        return openDetailsRoute({
                                    "model": PlayQueue,
                                    "itemId": itemId,
                                    "itemType": RoutePolicy.itemTypeFor(item),
                                    "source": "playback",
                                    "returnRoute": returnRoute,
                                    "focusIndex": Math.max(0, PlayQueue.currentIndex)
                                })
    }

    function pushRoute(nextRoute, args) {
        Router.push(nextRoute, args || ({}))
        navigationTarget = routeStack
        InputKeys.focus(routeStack)
    }

    function commitDetailsRoute(args, source, focusIndex) {
        if (!args) {
            console.warn("details route ignored: missing item id", source || "", Math.max(0, Number(focusIndex || 0)))
            return false
        }
        if (route === "itemDetails") {
            Router.replace("itemDetails", args)
            InputKeys.focus(routeStack)
        } else {
            pushRoute("itemDetails", args)
        }
        return true
    }

    function openDetailsRoute(request) {
        return commitDetailsRoute(RoutePolicy.normalizeDetailsRoute(request, Browse.items, route), request
                                  ? request.source : "", request ? request.focusIndex : 0)
    }

    function openDetailsAt(model, index, source, returnRoute) {
        const nextModel = model || (Browse.items)
        return commitDetailsRoute(RoutePolicy.detailsRouteAt(nextModel, index, source, returnRoute, route), source,
                                  index)

    }

    function openSeriesDetails(seriesId, seriesName, returnRoute) {
        const id = String(seriesId || "")
        if (id.length <= 0)
            return false
        const title = String(seriesName || "Series")
        Content.prepareLinkedItem(id, title, "Series", "", title, "")
        return openDetailsAt(Content.linkedItems, 0, "series-link", returnRoute || route)
    }

    function openSeasonDetails(seriesId, seasonId, seasonName, seriesName, returnRoute) {
        const showId = String(seriesId || "")
        const id = String(seasonId || "")
        if (showId.length <= 0 || id.length <= 0)
            return false
        Content.prepareLinkedItem(id, String(seasonName || "Season"), "Season", showId, String(seriesName || ""), id)
        return openDetailsAt(Content.linkedItems, 0, "season-link", returnRoute || route)
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

    function exitPlaybackForRemoteNavigation(reason) {
        if (!playerSessionActive)
            return
        pendingPlaybackBackItem = ({})
        player.stopWithReason(reason)
    }

    function handleRemoteUiAction(action) {
        if (action === "toggle-osd") {
            if (player.visible)
                videoSurface.toggleOsd()
            return
        }
        if (action === "context-menu") {
            if (player.visible)
                videoSurface.openPlaybackSettings()
            else
                openContextMenu()
            return
        }
        if (action === "settings") {
            if (player.visible)
                videoSurface.openPlaybackSettings()
            else
                pushRoute("settings")
            return
        }
        if (action === "search") {
            exitPlaybackForRemoteNavigation("remote-search")
            pushRoute("search")
            return
        }
        if (action === "home") {
            exitPlaybackForRemoteNavigation("remote-home")
            goHome()
        }
    }

    function switchUser() {
        switchUserReturnProfileId = Session.activeProfileId
        switchUserReturnRoute = route
        switchUserReturnArgs = Object.assign({}, routeArgs)
        switchUserReturnPending = false
        Router.reset("login")
        App.switchUser()
        navigationTarget = routeStack
        InputKeys.focus(routeStack)
    }

    function cancelSwitchUser() {
        if (!canCancelSwitchUser)
            return false
        if (switchUserReturnPending)
            return true
        switchUserReturnPending = true
        App.useProfile(switchUserReturnProfileId)
        return true
    }

    function completeSwitchUserReturn() {
        if (!switchUserReturnPending || !Session.authenticated)
            return false
        const returnRoute = switchUserReturnRoute
        const returnArgs = switchUserReturnArgs
        clearSwitchUserReturn()
        Router.reset(returnRoute, returnArgs)
        navigationTarget = routeStack
        InputKeys.focus(routeStack)
        return true
    }

    function clearSwitchUserReturn() {
        switchUserReturnProfileId = ""
        switchUserReturnRoute = ""
        switchUserReturnArgs = ({})
        switchUserReturnPending = false
    }

    function releaseTextInput() {
        let item = root.Window.window ? root.Window.window.activeFocusItem : null
        while (item) {
            if (item.releaseTextInput && item.releaseTextInput())
                return
            item = item.parent
        }
        Qt.inputMethod.hide()
        InputKeys.focus(routeStack)
    }

    function back() {
        if (tlsTrustPending)
            return tlsTrustDialog.back()
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
            closeMediaInfo()
            return true
        }
        if (root.hasPlayer && root.player.visible) {
            if (root.player.backAllowed) {
                root.preparePlaybackBackNavigation(PlayQueue.currentIndex >= 0 ? PlayQueue.get(PlayQueue.currentIndex) :
                                                                                 ({}))
                root.player.stopWithReason("shell-back-fallback")
            }
            return true
        }
        if (routeStack.back())
            return true
        if (route === "itemDetails") {
            Router.pop(String(routeArgs.returnRoute || "libraryGrid"))
            InputKeys.focus(routeStack)
            return true
        }
        if (route === "libraryGrid") {
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

    function forward() {
        if (tlsTrustPending || textInputActive || (navBar.visible && navBar.syncPlayMenuOpen) || diagnosticsVisible
                || itemMenuOpen || managementOverlayVisible || mediaInfoVisible || playerSessionActive)
            return true
        if (!Router.canForward)
            return false
        if (!Router.forward())
            return false
        navigationTarget = routeStack
        InputKeys.focus(routeStack)
        return true
    }

    function openContextMenu() {
        if (navigationTarget !== routeStack)
            return false
        return routeStack.longPress()
    }

    function openItemMenu(item, anchorItem, context) {
        Management.loadCurrentUserPolicy()
        itemMenuLoaded = true
        return itemContextMenuLoader.item ? itemContextMenuLoader.item.openForItem(item || ({}), anchorItem || null,
                                                                                   context || ({})) : false
    }

    function finishItemMenuOpeningGesture() {
        if (itemContextMenuLoader.item)
            itemContextMenuLoader.item.finishOpeningGesture()
    }

    function restoreFocusAfterItemMenu() {
        if (managementOverlayVisible || mediaInfoVisible || diagnosticsVisible || player.visible)
            return
        navigationTarget = routeStack
        InputKeys.focus(routeStack)
    }

    function mediaInfoAvailable(item) {
        const type = String(item && item.itemType || "")
        return Boolean(item && item.movieId && type !== "Series" && type !== "Season")
    }

    function openMediaInfo(item) {
        if (!mediaInfoAvailable(item))
            return false
        mediaInfoItem = item || ({})
        if (mediaInfoItem.movieId)
            Content.loadItemDetail(mediaInfoItem.movieId)
        mediaInfoVisible = true
        return true
    }

    function closeMediaInfo() {
        mediaInfoVisible = false
        mediaInfoItem = ({})
        InputKeys.focus(routeStack)
    }

    function openManagement(mode, item) {
        managementMode = mode
        managementItem = item || ({})
        managementOverlayVisible = true
    }

    function closeManagementOverlay() {
        managementOverlayVisible = false
        managementMode = ""
        managementItem = ({})
        Qt.inputMethod.hide()
        InputKeys.focus(routeStack)
    }

    function openPerson(person) {
        personItem = person || ({})
        const personId = String(personItem.id || "")
        if (personId.length <= 0)
            return false
        if (Content)
            Content.loadPersonItems(personId)
        if (route === "personDetails") {
            InputKeys.focus(routeStack)
            return true
        }
        pushRoute("personDetails", {
                      personId: personId,
                      personName: String(personItem.name || "Person"),
                      personRole: String(personItem.role || ""),
                      personType: String(personItem.type || "Person")
                  })
        return true
    }

    function currentMediaItem() {
        const page = routeStack.activeItem
        const item = page && page.currentMediaItem ? page.currentMediaItem() : null
        return item || ({})
    }

    function focusNavBar() {
        if (route === "login")
            return
        const page = routeStack.activeItem
        if (page && page.revealHeader)
            page.revealHeader()
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

    function setUiScale(percent) {
        Settings.setUiScalePercent(Math.max(80, Math.min(180, Math.round(Number(percent || 100) / 5) * 5)))
    }

    function globalShortcut(key, phase, repeat, modifiers) {
        if (phase === "release" && key === uiScaleShortcutKey) {
            uiScaleShortcutKey = 0
            return true
        }
        if (phase === "release" && key === searchShortcutKey) {
            searchShortcutKey = 0
            return true
        }
        if (phase === "release" && key === settingsShortcutKey) {
            settingsShortcutKey = 0
            return true
        }
        if (repeat)
            return key === uiScaleShortcutKey || key === searchShortcutKey || key === settingsShortcutKey

        const control = Boolean(modifiers & Qt.ControlModifier)
        const primaryModifier = Boolean(modifiers & (Qt.ControlModifier | Qt.MetaModifier))
        const shortcutRoute = key === Qt.Key_F ? "search" : key === Qt.Key_Comma ? "settings" : ""
        if (phase === "press" && primaryModifier && shortcutRoute.length > 0) {
            if (shortcutRoute === "search")
                searchShortcutKey = key
            else
                settingsShortcutKey = key
            if (playerSessionActive)
                return true
            if (route === shortcutRoute) {
                navigationTarget = routeStack
                InputKeys.focus(routeStack)
            } else {
                pushRoute(shortcutRoute)
            }
            return true
        }
        if (phase === "press" && control) {
            let scaleDelta = 0
            if (key === Qt.Key_Plus || key === Qt.Key_Equal)
                scaleDelta = 5
            else if (key === Qt.Key_Minus || key === Qt.Key_Underscore)
                scaleDelta = -5
            if (scaleDelta !== 0 || key === Qt.Key_0) {
                uiScaleShortcutKey = key
                setUiScale(key === Qt.Key_0 ? 100 : Settings.uiScalePercent + scaleDelta)
                return true
            }
        }

        if (textInputActive)
            return false
        // Claim the physical Menu press as well as its release so focus remains
        // stable until the release-triggered context menu opens.
        if (phase === "press" && (key === Qt.Key_M || key === Qt.Key_Menu))
            return true
        if (phase !== "release")
            return false
        if (control && key === Qt.Key_D) {
            diagnosticsVisible = !diagnosticsVisible
            return true
        }
        if (key === Qt.Key_Slash) {
            pushRoute("search")
            return true
        }
        if (key === Qt.Key_I) {
            if (mediaInfoVisible)
                closeMediaInfo()
            else
                return openMediaInfo(currentMediaItem())
            return true
        }
        if (key === Qt.Key_M || key === Qt.Key_Menu) {
            openContextMenu()
            return true
        }
        if (key === Qt.Key_H || key === Qt.Key_L)
            return deliver(activeTarget, key === Qt.Key_H ? Qt.Key_Left : Qt.Key_Right, "press", false)
        if (key === Qt.Key_Q && root.playerSessionActive) {
            root.player.stopWithReason("shortcut-q")
            return true
        }
        return false
    }

    Item {
        anchors.fill: parent
        z: 1000

        PointHandler {
            acceptedButtons: Qt.LeftButton
            onActiveChanged: if (active && navBar.syncPlayMenuOpen && !navBar.containsSyncPlayPoint(root,
                                                                                                    point.position.x,
                                                                                                    point.position.y))
                                 navBar.closeSyncPlayMenu(false)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
        visible: !(root.hasPlayer && root.playerHoldsScreen)
    }

    ColumnLayout {
        id: contentLayer
        anchors.fill: parent
        anchors.topMargin: -root.keyboardAvoidance
        anchors.bottomMargin: root.keyboardAvoidance
        spacing: 0
        visible: App.initialized && !(root.hasPlayer && root.playerHoldsScreen)
        enabled: visible

        TopBar {
            id: navBar
            Layout.fillWidth: true
            Layout.preferredHeight: route === "login" ? 0 : Metrics.topBarHeightPx
            visible: route !== "login"
            z: 1
            currentRoute: root.route
            onActiveFocusChanged: if (activeFocus)
                                      root.navigationTarget = navBar
            onNavigate: r => {
                if (r === "home")
                    root.goHome()
                else if (r === "switchUser")
                    root.switchUser()
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
            startupReady: App.initialized
            focus: !(root.hasPlayer && root.playerHoldsScreen)
            onActiveFocusChanged: if (activeFocus)
                                      root.navigationTarget = routeStack
        }
    }

    Rectangle {
        readonly property bool contentReady: routeStack.activeRoute.length > 0 && routeStack.activeItem && (
                                                 typeof routeStack.activeItem.contentReady === "undefined"
                                                 || routeStack.activeItem.contentReady)
        property bool dismissed: false

        anchors.fill: parent
        color: "black"
        visible: !dismissed
        z: 70

        onContentReadyChanged: if (contentReady)
                                   dismissed = true

        Image {
            anchors.fill: parent
            source: startupSplashImageUrl
            fillMode: Image.PreserveAspectFit
            asynchronous: false
            cache: true
        }
    }

    VideoSurface {
        id: videoSurface
        anchors.fill: parent
        active: root.hasPlayer && root.player.visible
        mediaInfoVisible: root.mediaInfoVisible
        diagnosticsVisible: root.diagnosticsVisible
        onPlaybackBackRequested: item => root.preparePlaybackBackNavigation(item)
        z: 19
    }

    Loader {
        id: busyOverlayLoader
        anchors.fill: parent
        z: 40
        // Stepping to the next item is the one case where the busy state should
        // show over the player: the surface is being held deliberately and
        // would otherwise be a frozen last frame with no sign of progress.
        active: App.playbackTransition || (root.busyValue && !(root.hasPlayer && root.playerHoldsScreen))
        asynchronous: true
        source: active ? "BusyOverlay.qml" : ""

        Binding {
            target: busyOverlayLoader.item
            property: "text"
            value: App.playbackTransition ? "Loading next item…" : root.busyTextValue
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
            onDismissed: root.closeManagementOverlay()
        }
        onLoaded: item.prepare()
    }

    Loader {
        id: itemContextMenuLoader
        anchors.fill: parent
        z: 58
        active: root.itemMenuLoaded
        sourceComponent: ItemContextMenu {
            shell: root
            onClosed: Qt.callLater(root.restoreFocusAfterItemMenu)
        }
    }

    Loader {
        id: mediaInfoOverlayLoader
        anchors.fill: parent
        z: 59
        active: root.mediaInfoVisible
        sourceComponent: MediaInfoOverlay {
            visible: root.mediaInfoVisible
            item: visible ? (root.mediaInfoItem && Object.keys(root.mediaInfoItem).length > 0 ? root.mediaInfoItem :
                                                                                                root.currentMediaItem(
                                                                                                    )) : ({})
            shell: root
            onClosed: root.closeMediaInfo()
        }
    }

    Loader {
        anchors.fill: parent
        z: 61
        active: root.diagnosticsVisible && !(root.hasPlayer && root.playerHoldsScreen)
        sourceComponent: DiagnosticsOverlay {
            route: root.route
            focusedItemId: RoutePolicy.itemIdFor(root.currentMediaItem())
        }
    }
    Rectangle {
        id: remoteMessage
        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.round(parent.height * 0.75 - height / 2)
        width: Math.min(parent.width * 0.72, Metrics.scaled(960))
        height: remoteMessageText.implicitHeight + Metrics.scaled(30)
        visible: false
        radius: Theme.radiusMedium
        color: Theme.bgRaised
        z: 69

        AppText {
            id: remoteMessageText
            anchors.centerIn: parent
            width: Math.max(0, parent.width - Metrics.scaled(32))
            color: Theme.textPrimary
            font.pixelSize: Metrics.bodySizePx + Metrics.scaled(1)
            font.weight: Font.Normal
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        Timer {
            id: remoteMessageTimer
            interval: 10000
            onTriggered: remoteMessage.visible = false
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
        anchors.bottomMargin: Metrics.scaled(32)
        width: Math.min(parent.width * 0.72, Metrics.scaled(960))
        height: root.errorTextValue.length > 0 ? errorText.implicitHeight + Metrics.scaled(28) : 0
        visible: root.errorTextValue.length > 0
        baseColor: Theme.errorPanel
        z: 80
        AppText {
            id: errorText
            anchors.centerIn: parent
            width: Math.max(0, parent.width - Metrics.scaled(28))
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
    TlsTrustDialog {
        id: tlsTrustDialog
        visible: root.tlsTrustPending
        trustController: TlsTrust
        inputKeys: InputKeys
        z: 250
    }
    InputLatencyWarning {
        anchors.fill: parent
        z: 90
        monitor: InputLatency
    }
}
