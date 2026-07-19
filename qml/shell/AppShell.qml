import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"
import "RoutePolicy.js" as RoutePolicy

KeyRouter {
    id: root
    onWidthChanged: Metrics.refWidth = width
    focus: true
    backspaceNavigatesInTextInput: Platform.isTV
    webOsScanCodes: Platform.isTV

    readonly property string route: Router.route
    readonly property var routeArgs: Router.args || ({})
    property bool diagnosticsVisible: false
    property bool mediaInfoVisible: false
    property bool itemMenuLoaded: false
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
    }
    function showToastAction(message, actionText, callback) {
        toast.showAction(message, actionText, callback)
    }

    function defaultRoute() {
        return !Settings.uiScaleSetupComplete ? "scaleSetup" : Session.authenticated ? "home" : "login"
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
            if (Session.authenticated && root.restoreRecoveredRoute())
                return
            if (Session.authenticated)
                Router.replace(root.defaultRoute())
            else
                Router.reset(root.defaultRoute())
        }
    }

    Component.onCompleted: if (App.initialized)
                               root.applyInitializedRoute()

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

    function openDetailsRoute(request) {
        const normalized = RoutePolicy.normalizeDetailsRoute(request, Browse.items, route)
        if (!normalized) {
            const focusIndex = Math.max(0, Number(request && request.focusIndex !== undefined ? request.focusIndex : 0))
            console.warn("details route ignored: missing item id", request ? request.source : "", focusIndex)
            return false
        }

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
        Settings.setUiScalePercent(Math.max(80, Math.min(125, Math.round(Number(percent || 100) / 5) * 5)))
    }

    function globalShortcut(key, phase, repeat, modifiers) {
        if (repeat || textInputActive)
            return false
        // Claim the physical Menu press as well as its release so focus remains
        // stable until the release-triggered context menu opens.
        if (phase === "press" && (key === Qt.Key_M || key === Qt.Key_Menu))
            return true
        if (phase !== "release")
            return false
        if (modifiers & Qt.ControlModifier) {
            if (key === Qt.Key_Plus || key === Qt.Key_Equal) {
                setUiScale(Settings.uiScalePercent + 5)
                return true
            }
            if (key === Qt.Key_Minus || key === Qt.Key_Underscore) {
                setUiScale(Settings.uiScalePercent - 5)
                return true
            }
            if (key === Qt.Key_0) {
                setUiScale(100)
                return true
            }
            if (key === Qt.Key_D) {
                diagnosticsVisible = !diagnosticsVisible
                return true
            }
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

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
        visible: !(root.hasPlayer && root.player.visible)
    }

    Column {
        anchors.centerIn: parent
        spacing: Metrics.scaled(12)
        visible: !App.initialized && !(root.hasPlayer && root.player.visible)

        AppText {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Jellyfin Native"
            color: Theme.textPrimary
            font.pixelSize: Metrics.scaled(34)
            font.weight: Font.DemiBold
        }

        AppText {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Starting…")
            color: Theme.textSecondary
            font.pixelSize: Metrics.bodySizePx
        }
    }

    ColumnLayout {
        id: contentLayer
        anchors.fill: parent
        anchors.topMargin: -root.keyboardAvoidance
        anchors.bottomMargin: root.keyboardAvoidance
        spacing: 0
        visible: App.initialized && !(root.hasPlayer && root.player.visible)
        enabled: visible

        TopBar {
            id: navBar
            Layout.fillWidth: true
            Layout.preferredHeight: route === "login" || route === "scaleSetup" ? 0 : Metrics.topBarHeightPx
            visible: route !== "login" && route !== "scaleSetup"
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
            focus: !(root.hasPlayer && root.player.visible)
            onActiveFocusChanged: if (activeFocus)
                                      root.navigationTarget = routeStack

            TapHandler {
                acceptedButtons: Qt.LeftButton
                onTapped: if (navBar.syncPlayMenuOpen)
                              navBar.closeSyncPlayMenu()
            }
        }
    }

    VideoSurface {
        id: videoSurface
        anchors.fill: parent
        active: root.hasPlayer && root.player.visible
        mediaInfoVisible: root.mediaInfoVisible
        diagnosticsVisible: root.diagnosticsVisible
        onDiagnosticsVisibilityRequested: visible => root.diagnosticsVisible = visible
        onPlaybackBackRequested: item => root.preparePlaybackBackNavigation(item)
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
        active: root.diagnosticsVisible && !(root.hasPlayer && root.player.visible)
        sourceComponent: DiagnosticsOverlay {
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
