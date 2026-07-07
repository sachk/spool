import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../theme"
import "../primitives"
import "../pages"
import "RoutePolicy.js" as RoutePolicy

FocusScope {
    id: root
    focus: true

    readonly property string route: router ? router.route : controllerRoute()
    property int lastLibraryIndex: 0
    property int lastGridIndex: 0
    property int lastSearchIndex: 0
    property var detailsModel: appController ? appController.movies : null
    property int detailsIndex: 0
    property string detailsSource: "movies"
    property string detailsReturnRoute: "libraryGrid"
    property var detailsRoute: ({
            itemId: "",
            itemType: "",
            source: "movies",
            returnRoute: "libraryGrid",
            focusIndex: 0
        })
    property bool shortcutOverlayVisible: false
    property bool shortcutOverlayLoaded: false
    property bool diagnosticsVisible: false
    property bool diagnosticsLoaded: false
    property bool mediaInfoVisible: false
    property bool mediaInfoLoaded: false
    property bool itemMenuLoaded: false
    readonly property bool itemMenuOpen: itemContextMenuLoader.item ? itemContextMenuLoader.item.opened : false
    property var mediaInfoItem: ({})
    property var personItem: ({})
    property bool textInputActive: Qt.inputMethod.visible
            || InputKeys.isTextInputItem(root.Window.window ? root.Window.window.activeFocusItem : null)
    property bool backPressHandled: false
    property bool playerBackPressHandled: false
    readonly property var player: appController ? appController.player : null
    readonly property bool hasPlayer: player !== null && player !== undefined
    readonly property string errorTextValue: appController ? appController.errorText : ""
    readonly property bool busyValue: appController ? appController.busy : false
    readonly property string busyTextValue: appController ? appController.busyText : ""
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
        const keyboardTop = keyboardRect && keyboardRect.height > 0
                ? keyboardRect.y : root.height
        const focusPos = focusItem.mapToItem(root, 0, 0)
        const focusBottom = focusPos.y + focusItem.height + keyboardAvoidance
        const overlap = focusBottom + 24 - keyboardTop
        keyboardAvoidance = Math.max(0, Math.min(overlap, root.height * 0.45))
    }

    Behavior on keyboardAvoidance {
        enabled: !Theme.reducedMotion
        NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
    }

    Connections {
        target: Qt.inputMethod
        function onVisibleChanged() { root.refreshKeyboardAvoidance() }
        function onKeyboardRectangleChanged() { root.refreshKeyboardAvoidance() }
        function onAnchorRectangleChanged() { root.refreshKeyboardAvoidance() }
    }

    Connections {
        target: root.Window.window
        function onActiveFocusItemChanged() { root.refreshKeyboardAvoidance() }
    }

    Connections {
        target: appController
        function onAggressiveMemoryPressure() {
            if (!root.shortcutOverlayVisible)
                root.shortcutOverlayLoaded = false
            if (!root.diagnosticsVisible)
                root.diagnosticsLoaded = false
            if (!root.mediaInfoVisible)
                root.mediaInfoLoaded = false
            if (!root.itemMenuOpen)
                root.itemMenuLoaded = false
        }
    }
    function controllerRoute() {
        if (!appController)
            return "home";
        if (appController.page === "login")
            return "login";
        // After login, default to Home. The user can drill into Libraries explicitly.
        if (appController.page === "libraries")
            return "home";
        // page === "movies" — we just opened a library or backed out of playback into one.
        return "libraryGrid";
    }

    Connections {
        target: appController
        function onPageChanged() {
            if (router)
                router.replace(root.controllerRoute());
        }
    }

    Component.onCompleted: if (router)
        router.reset(root.controllerRoute())

    Connections {
        target: root.player
        function onVisibleChanged() {
            if (root.hasPlayer && root.player.visible) {
                root.playerBackPressHandled = false;
                root.focusPlayerInput();
            } else {
                routeStack.forceActiveFocus();
            }
        }
    }

    function focusPlayerInput() {
        playerInputShield.forceActiveFocus();
    }

    function pushRoute(nextRoute) {
        if (router)
            router.push(nextRoute);
        routeStack.forceActiveFocus();
    }

    function itemIdFor(item) {
        return RoutePolicy.itemIdFor(item);
    }

    function itemTypeFor(item) {
        return RoutePolicy.itemTypeFor(item);
    }

    function modelCount(model) {
        return RoutePolicy.modelCount(model);
    }

    function modelItem(model, index) {
        return RoutePolicy.modelItem(model, index);
    }

    function modelIndexForItemId(model, itemId, fallbackIndex) {
        return RoutePolicy.modelIndexForItemId(model, itemId, fallbackIndex);
    }

    function detailsIndexForModel(model) {
        return modelIndexForItemId(model, detailsRoute ? detailsRoute.itemId : "", detailsIndex);
    }

    function openDetailsRoute(request) {
        const normalized = RoutePolicy.normalizeDetailsRoute(request, appController ? appController.movies : null, route);
        if (!normalized) {
            const focusIndex = Math.max(0, Number(request && request.focusIndex !== undefined ? request.focusIndex : 0));
            console.warn("details route ignored: missing item id", request ? request.source : "", focusIndex);
            return false;
        }

        detailsModel = normalized.model;
        detailsIndex = normalized.focusIndex;
        detailsSource = normalized.source;
        detailsReturnRoute = normalized.returnRoute;
        detailsRoute = {
            itemId: normalized.itemId,
            itemType: normalized.itemType,
            source: detailsSource,
            returnRoute: detailsReturnRoute,
            focusIndex: detailsIndex
        };
        if (detailsSource === "movies")
            lastGridIndex = detailsIndex;
        else if (detailsSource === "search" || detailsSource === "suggestion")
            lastSearchIndex = detailsIndex;

        if (route === "itemDetails") {
            routeStack.forceActiveFocus();
            return true;
        }
        pushRoute("itemDetails");
        return true;
    }

    function openDetailsAt(model, index, source, returnRoute) {
        const nextModel = model || (appController ? appController.movies : null);
        const request = RoutePolicy.detailsRouteAt(nextModel, index, source, returnRoute, route);
        return request ? openDetailsRoute(request) : false;
    }

    function replaceRoute(nextRoute) {
        if (router)
            router.replace(nextRoute);
        routeStack.forceActiveFocus();
    }

    function goHome() {
        if (router)
            router.reset("home");
        appController.goHome();
        routeStack.forceActiveFocus();
    }

    function switchUser() {
        if (router)
            router.reset("login");
        if (appController)
            appController.switchUser();
        routeStack.forceActiveFocus();
    }

    function releaseTextInput() {
        // Forcing focus onto the route stack causes the focused TextField to
        // lose activeFocus, which closes the virtual keyboard cleanly.
        routeStack.forceActiveFocus();
        Qt.inputMethod.hide();
    }

    function back() {
        if (textInputActive) {
            releaseTextInput();
            return true;
        }
        if (navBar.visible && navBar.syncPlayMenuOpen) {
            navBar.closeSyncPlayMenu();
            return true;
        }
        if (shortcutOverlayVisible) {
            shortcutOverlayVisible = false;
            return true;
        }
        if (diagnosticsVisible) {
            diagnosticsVisible = false;
            return true;
        }
        if (itemMenuOpen && itemContextMenuLoader.item) {
            itemContextMenuLoader.item.closeMenu();
            return true;
        }
        if (mediaInfoVisible) {
            mediaInfoVisible = false;
            return true;
        }
        if (root.hasPlayer && root.player.visible) {
            if (root.player.backAllowed)
                root.player.stopWithReason("shell-back-fallback");
            return true;
        }
        if (routeStack.handleBack())
            return true;
        if (route === "itemDetails") {
            replaceRoute(detailsRoute && detailsRoute.returnRoute ? detailsRoute.returnRoute : (detailsReturnRoute.length > 0 ? detailsReturnRoute : "libraryGrid"));
            return true;
        }
        if (route === "libraries" || route === "libraryGrid") {
            goHome();
            return true;
        }
        if (route === "home") {
            switchUser();
            return true;
        }
        if (router) {
            router.pop(route === "personDetails" ? "itemDetails" : "home");
            routeStack.forceActiveFocus();
            return true;
        }
        return true;
    }

    function openContextMenu() {
        openItemMenu(currentMediaItem(), null);
    }

    function openItemMenu(item, anchorItem) {
        itemMenuLoaded = true;
        return itemContextMenuLoader.item
                ? itemContextMenuLoader.item.openForItem(item || ({}), anchorItem || null)
                : false;
    }

    function openMediaInfo(item) {
        mediaInfoItem = item || ({});
        if (appController && mediaInfoItem.movieId)
            appController.loadItemDetail(mediaInfoItem.movieId);
        mediaInfoLoaded = true;
        mediaInfoVisible = true;
    }

    function openPerson(person) {
        personItem = person || ({});
        if (appController && personItem.personId)
            appController.loadPersonItems(personItem.personId);
        if (route === "personDetails") {
            routeStack.forceActiveFocus();
            return;
        }
        pushRoute("personDetails");
    }

    function currentMediaItem() {
        if (route === "itemDetails" && detailsModel && detailsModel.rowCount) {
            const detailsCount = detailsModel.rowCount();
            if (detailsCount > 0) {
                const detailsIdx = detailsIndexForModel(detailsModel);
                return detailsModel.get(detailsIdx) || ({});
            }
        }
        if (route === "search" && appController && appController.searchController && appController.searchController.results) {
            const searchCount = appController.searchController.results.rowCount();
            if (searchCount > 0) {
                const searchIdx = Math.max(0, Math.min(lastSearchIndex, searchCount - 1));
                return appController.searchController.results.get(searchIdx) || ({});
            }
        }
        const count = appController.movies.rowCount();
        if (count <= 0)
            return ({});
        const idx = Math.max(0, Math.min(lastGridIndex, count - 1));
        return appController.movies.get(idx) || ({});
    }

    function focusNavBar() {
        if (route === "login")
            return;
        navBar.forceActiveFocus();
        navBar.focusCurrent();
    }

    function focusContent() {
        if (root.hasPlayer && root.player.visible)
            return;
        routeStack.forceActiveFocus();
    }

    function dispatchNavigationKey(key) {
        if (navBar.visible && navBar.activeFocus)
            return navBar.handleNavigationKey(key);
        return routeStack.handleNavigationKey(key);
    }

    function setUiScale(value) {
        const step = 0.05;
        const rounded = Math.round(Number(value || 1.0) / step) * step;
        Metrics.userUiScale = Math.max(0.75, Math.min(1.5, rounded));
    }

    function handleZoomShortcut(event) {
        if (!(event.modifiers & Qt.ControlModifier))
            return false;
        if (event.key === Qt.Key_Plus || event.key === Qt.Key_Equal) {
            setUiScale(Metrics.userUiScale + 0.05);
            return true;
        }
        if (event.key === Qt.Key_Minus || event.key === Qt.Key_Underscore) {
            setUiScale(Metrics.userUiScale - 0.05);
            return true;
        }
        if (event.key === Qt.Key_0) {
            setUiScale(1.0);
            return true;
        }
        return false;
    }

    function globalShortcut(event, released) {
        if (released && handleZoomShortcut(event))
            return true;
        if (!released || textInputActive)
            return false;
        if (event.modifiers & Qt.ControlModifier && event.key === Qt.Key_D) {
            diagnosticsLoaded = true;
            diagnosticsVisible = !diagnosticsVisible;
            return true;
        }
        if (event.key === Qt.Key_Question) {
            shortcutOverlayLoaded = true;
            shortcutOverlayVisible = !shortcutOverlayVisible;
            return true;
        }
        if (event.key === Qt.Key_Slash) {
            pushRoute("search");
            return true;
        }
        if (event.key === Qt.Key_I) {
            if (mediaInfoVisible) {
                mediaInfoVisible = false;
                mediaInfoItem = ({});
            } else
                openMediaInfo(currentMediaItem());
            return true;
        }
        if (event.key === Qt.Key_M || event.key === Qt.Key_Menu) {
            openContextMenu();
            return true;
        }
        if (InputKeys.isBack(event.key))
            return back();
        if (event.key === Qt.Key_H)
            return dispatchNavigationKey(Qt.Key_Left);
        if (event.key === Qt.Key_L)
            return dispatchNavigationKey(Qt.Key_Right);
        if (event.key === Qt.Key_Q && root.hasPlayer && root.player.visible) {
            root.player.stopWithReason("shortcut-q");
            return true;
        }
        return false;
    }

    function handlePlayerPressed(event) {
        if (InputKeys.isBackEvent(event, !textInputActive)) {
            playerBackPressHandled = true;
            playerOverlay.handleBack(true);
            return true;
        }
        if (playerOverlay.handlePressed(event))
            return true;
        return InputKeys.isDirection(event.key) || InputKeys.isAccept(event.key) || InputKeys.isIgnoredPlayerNoise(event);
    }

    function handlePlayerReleased(event) {
        if (playerBackPressHandled && InputKeys.isBackEvent(event, !textInputActive)) {
            playerBackPressHandled = false;
            return true;
        }
        return playerOverlay.handleReleased(event) || globalShortcut(event, true);
    }

    Keys.priority: Keys.BeforeItem

    Keys.onPressed: event => {
        if (itemMenuOpen && itemContextMenuLoader.item) {
            itemContextMenuLoader.item.handlePressed(event);
            event.accepted = true;
            return;
        }
        if (mediaInfoVisible && mediaInfoOverlayLoader.item) {
            mediaInfoOverlayLoader.item.handlePressed(event);
            event.accepted = true;
            return;
        }
        if (root.hasPlayer && root.player.visible) {
            if (handlePlayerPressed(event))
                event.accepted = true;
            return;
        }

        if (InputKeys.isBackEvent(event, !textInputActive)) {
            backPressHandled = true;
            back();
            event.accepted = true;
            return;
        }

        if (textInputActive && Qt.inputMethod.visible && InputKeys.isDirection(event.key))
            return;

        if (InputKeys.isDirection(event.key)) {
            if (dispatchNavigationKey(event.key))
                event.accepted = true;
        }
        if (InputKeys.isAccept(event.key) && !navBar.activeFocus) {
            if (routeStack.handlePressedKey(event.key))
                event.accepted = true;
        }
    }

    Keys.onReleased: event => {
        if (playerBackPressHandled && InputKeys.isBackEvent(event, !textInputActive)) {
            playerBackPressHandled = false;
            event.accepted = true;
            return;
        }
        if (itemMenuOpen && itemContextMenuLoader.item) {
            itemContextMenuLoader.item.handleReleased(event);
            event.accepted = true;
            return;
        }
        if (mediaInfoVisible && mediaInfoOverlayLoader.item) {
            mediaInfoOverlayLoader.item.handleReleased(event);
            event.accepted = true;
            return;
        }
        if (root.hasPlayer && root.player.visible) {
            if (handlePlayerReleased(event))
                event.accepted = true;
            return;
        }
        if (InputKeys.isBackEvent(event, !textInputActive) && backPressHandled) {
            backPressHandled = false;
            event.accepted = true;
            return;
        }
        if (InputKeys.isAccept(event.key)) {
            // Don't hijack Enter when the nav bar (or anything else) owns focus —
            // let the focused button handle it natively.
            if (!navBar.activeFocus && routeStack.handleNavigationKey(event.key)) {
                event.accepted = true;
                return;
            }
        }
        if (globalShortcut(event, true))
            event.accepted = true;
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
        visible: !(root.hasPlayer && root.player.visible)
    }

    ColumnLayout {
        id: contentLayer
        anchors.fill: parent
        transform: Translate { y: -root.keyboardAvoidance }
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
            onNavigate: r => {
                if (r === "home")
                    root.goHome();
                else if (r === "settings")
                    root.pushRoute("settings");
                else
                    root.pushRoute(r);
            }
            onContentRequested: root.focusContent()
        }

        RouteStack {
            id: routeStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            route: root.route
            shell: root
            args: router ? router.args : ({})
            focus: !(root.hasPlayer && root.player.visible)
        }
    }

    Image {
        anchors.fill: parent
        visible: root.hasPlayer && root.player.visible
        source: visible ? "image://mpv-overlay/live?rev=" + nativeWindow.overlayRevision : ""
        cache: false
        fillMode: Image.Stretch
        z: 19
    }

    PlayerOverlayPage {
        id: playerOverlay
        anchors.fill: parent
        visible: root.hasPlayer && root.player.visible
        mediaInfoVisible: root.mediaInfoVisible
        diagnosticsVisible: root.diagnosticsVisible
        z: 20
    }

    FocusScope {
        id: playerInputShield
        anchors.fill: parent
        visible: root.hasPlayer && root.player.visible
        enabled: visible
        focus: visible
        z: 21

        onVisibleChanged: if (visible)
            forceActiveFocus()
        onActiveFocusChanged: if (visible && !activeFocus)
            forceActiveFocus()

        Keys.priority: Keys.BeforeItem
        Keys.onPressed: event => {
            if (root.handlePlayerPressed(event))
                event.accepted = true;
        }
        Keys.onReleased: event => {
            if (root.handlePlayerReleased(event))
                event.accepted = true;
        }
    }

    Rectangle {
        id: busyOverlay
        anchors.fill: parent
        visible: root.busyValue && !(root.hasPlayer && root.player.visible)
        color: Theme.busyScrim
        z: 40
        Surface {
            anchors.centerIn: parent
            visible: busyOverlay.visible
            width: Math.min(620, parent.width - 96)
            height: 104
            elevated: true
            Row {
                anchors.centerIn: parent
                spacing: 18
                BusyIndicator {
                    running: true
                    width: 30
                    height: 30
                }
                AppText {
                    text: root.busyTextValue
                    font.pixelSize: Metrics.bodyPx(root.width) + 2
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }

    Loader {
        id: itemContextMenuLoader
        anchors.fill: parent
        z: 58
        active: root.itemMenuLoaded
        sourceComponent: ItemContextMenu {
            onPlayedToggled: (itemId, played) => {
                if (appController)
                    appController.setPlayed(itemId, played)
            }
            onFavoriteToggled: (itemId, favorite) => {
                if (appController)
                    appController.setFavorite(itemId, favorite)
            }
            onClearProgressRequested: (itemId) => {
                if (appController)
                    appController.clearProgress(itemId)
            }
            onOpenSeriesRequested: (seriesId, seriesName) => {
                if (!appController || seriesId.length <= 0)
                    return
                root.replaceRoute("libraryGrid")
                appController.openSeriesById(seriesId, seriesName)
            }
            onOpenSeasonRequested: (seriesId, seasonId, seasonName) => {
                if (!appController || seriesId.length <= 0)
                    return
                root.replaceRoute("libraryGrid")
                appController.openSeasonById(seriesId, seasonId, seasonName)
            }
            onMediaInfoRequested: (snapshot) => root.openMediaInfo(snapshot)
        }
    }

    Loader {
        id: mediaInfoOverlayLoader
        anchors.fill: parent
        z: 59
        active: root.mediaInfoLoaded
        sourceComponent: MediaInfoOverlay {
            visible: root.mediaInfoVisible
            item: visible ? (root.mediaInfoItem && Object.keys(root.mediaInfoItem).length > 0 ? root.mediaInfoItem : root.currentMediaItem()) : ({})
            shell: root
            onClosed: {
                root.mediaInfoVisible = false;
                root.mediaInfoItem = ({});
            }
        }
    }

    Loader {
        anchors.fill: parent
        z: 60
        active: root.shortcutOverlayLoaded
        sourceComponent: ShortcutOverlay {
            visible: root.shortcutOverlayVisible
            onClosed: root.shortcutOverlayVisible = false
        }
    }

    Loader {
        anchors.fill: parent
        z: 61
        active: root.diagnosticsLoaded
        sourceComponent: DiagnosticsOverlay {
            visible: root.diagnosticsVisible && !(root.hasPlayer && root.player.visible)
            route: root.route
            focusedItemId: root.itemIdFor(root.currentMediaItem())
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
            onClicked: if (appController)
                appController.clearError()
        }
    }
}
