import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../theme"
import "../primitives"
import "../pages"

FocusScope {
    id: root
    focus: true

    property string route: controllerRoute()
    property var backStack: []
    property string previousRoute: "home"
    property int lastLibraryIndex: 0
    property int lastGridIndex: 0
    property real lastGridY: 0
    property int lastSearchIndex: 0
    property bool shortcutOverlayVisible: false
    property bool diagnosticsVisible: false
    property bool mediaInfoVisible: false
    property var mediaInfoItem: ({})
    property bool textInputActive: Qt.inputMethod.visible
    property bool backPressHandled: false
    property bool playerBackPressHandled: false
    readonly property var player: appController ? appController.player : null
    readonly property bool hasPlayer: player !== null && player !== undefined
    readonly property string errorTextValue: appController ? appController.errorText : ""
    readonly property bool busyValue: appController ? appController.busy : false
    readonly property string busyTextValue: appController ? appController.busyText : ""

    function controllerRoute() {
        if (!appController)
            return "home"
        if (appController.page === "login") return "login"
        // After login, default to Home. The user can drill into Libraries explicitly.
        if (appController.page === "libraries") return "home"
        // page === "movies" — we just opened a library or backed out of playback into one.
        return "libraryGrid"
    }

    Connections {
        target: appController
        function onPageChanged() { root.route = root.controllerRoute() }
    }

    Connections {
        target: root.player
        function onVisibleChanged() {
            if (root.hasPlayer && root.player.visible) {
                root.playerBackPressHandled = false
                root.focusPlayerInput()
            } else {
                routeStack.forceActiveFocus()
            }
        }
    }

    function focusPlayerInput() {
        playerInputShield.forceActiveFocus()
    }

    function pushRoute(nextRoute) {
        if (route === nextRoute)
            return
        previousRoute = route
        backStack.push(route)
        route = nextRoute
        routeStack.forceActiveFocus()
    }

    function replaceRoute(nextRoute) {
        previousRoute = route
        route = nextRoute
        routeStack.forceActiveFocus()
    }

    function goHome() {
        backStack = []
        previousRoute = "home"
        appController.goHome()
        route = "home"
        routeStack.forceActiveFocus()
    }

    function back() {
        if (shortcutOverlayVisible) { shortcutOverlayVisible = false; return true }
        if (diagnosticsVisible) { diagnosticsVisible = false; return true }
        if (mediaInfoVisible) { mediaInfoVisible = false; return true }
        if (root.hasPlayer && root.player.visible) {
            if (root.player.backAllowed) root.player.stopWithReason("shell-back-fallback")
            return true
        }
        if (route === "playerOverlay") { replaceRoute(previousRoute.length > 0 ? previousRoute : "home"); return true }
        if (route === "settings") { appController.closeSettings(); replaceRoute(previousRoute.length > 0 ? previousRoute : "home"); return true }
        if (route === "itemDetails") { replaceRoute("libraryGrid"); return true }
        if (route === "search") { replaceRoute(previousRoute.length > 0 ? previousRoute : "home"); return true }
        if (route === "libraryGrid") { goHome(); return true }
        appController.back()
        return true
    }

    function openContextMenu() {
        openMediaInfo(currentMediaItem())
    }

    function openMediaInfo(item) {
        mediaInfoItem = item || ({})
        mediaInfoVisible = true
    }

    function currentMediaItem() {
        const count = appController.movies.rowCount()
        if (count <= 0)
            return ({})
        const idx = Math.max(0, Math.min(lastGridIndex, count - 1))
        return appController.movies.get(idx) || ({})
    }

    function focusRail() {
        if (route === "login")
            return
        sideRail.forceActiveFocus()
        sideRail.focusCurrent()
    }

    function focusContent() {
        if (root.hasPlayer && root.player.visible)
            return
        routeStack.forceActiveFocus()
    }

    function globalShortcut(event, released) {
        if (!released || textInputActive)
            return false
        if (event.modifiers & Qt.ControlModifier && event.key === Qt.Key_D) {
            diagnosticsVisible = !diagnosticsVisible
            return true
        }
        if (event.key === Qt.Key_Question) { shortcutOverlayVisible = !shortcutOverlayVisible; return true }
        if (event.key === Qt.Key_Slash) { pushRoute("search"); return true }
        if (event.key === Qt.Key_I) {
            if (mediaInfoVisible) { mediaInfoVisible = false; mediaInfoItem = ({}) }
            else openMediaInfo(currentMediaItem())
            return true
        }
        if (event.key === Qt.Key_M || event.key === Qt.Key_Menu) { openContextMenu(); return true }
        if (event.key === Qt.Key_Backspace || event.key === Qt.Key_Escape || event.key === Qt.Key_Back || event.key === Qt.Key_BrowserBack) return back()
        if (event.key === Qt.Key_H) { event.key = Qt.Key_Left; return false }
        if (event.key === Qt.Key_L) { event.key = Qt.Key_Right; return false }
        if (event.key === Qt.Key_Q && root.hasPlayer && root.player.visible) { root.player.stopWithReason("shortcut-q"); return true }
        return false
    }

    function isDirectionalKey(key) {
        return key === Qt.Key_Left || key === Qt.Key_Right || key === Qt.Key_Up || key === Qt.Key_Down
    }

    function isAcceptKey(key) {
        return key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select || key === Qt.Key_Space
    }

    function isBackEvent(event) {
        const scanCode = Number(event.nativeScanCode || 0)
        const virtualKey = Number(event.nativeVirtualKey || 0)
        const key = Number(event.key || 0)
        return event.key === Qt.Key_Back
                || event.key === Qt.Key_Escape
                || event.key === Qt.Key_Backspace
                || event.key === Qt.Key_BrowserBack
                || event.key === 0x01200003
                || key === 461
                || scanCode === 420
                || scanCode === 461
                || virtualKey === 420
                || virtualKey === 461
    }

    function isIgnoredPlayerNoise(event) {
        const scanCode = Number(event.nativeScanCode || 0)
        return event.key === 0 && (scanCode === 1206 || scanCode === 1207)
    }

    function handlePlayerPressed(event) {
        if (isBackEvent(event)) {
            playerBackPressHandled = true
            playerOverlay.handleBack()
            return true
        }
        if (playerOverlay.handlePressed(event))
            return true
        return isDirectionalKey(event.key) || isAcceptKey(event.key) || event.key === Qt.Key_Space || isIgnoredPlayerNoise(event)
    }

    function handlePlayerReleased(event) {
        if (playerBackPressHandled && isBackEvent(event)) {
            playerBackPressHandled = false
            return true
        }
        return playerOverlay.handleReleased(event) || globalShortcut(event, true)
    }

    Keys.priority: Keys.BeforeItem

    Keys.onPressed: (event) => {
        if (root.hasPlayer && root.player.visible) {
            if (handlePlayerPressed(event))
                event.accepted = true
            return
        }

        if (isBackEvent(event)) {
            backPressHandled = true
            back()
            event.accepted = true
            return
        }

        if (isDirectionalKey(event.key)) {
            if (sideRail.visible && sideRail.activeFocus) {
                if (sideRail.handleNavigationKey(event.key))
                    event.accepted = true
                return
            }
            if (routeStack.handleNavigationKey(event.key))
                event.accepted = true
        }
    }

    Keys.onReleased: (event) => {
        if (playerBackPressHandled && isBackEvent(event)) {
            playerBackPressHandled = false
            event.accepted = true
            return
        }
        if (root.hasPlayer && root.player.visible) {
            if (handlePlayerReleased(event))
                event.accepted = true
            return
        }
        if (isBackEvent(event) && backPressHandled) {
            backPressHandled = false
            event.accepted = true
            return
        }
        if (isAcceptKey(event.key)) {
            // Don't hijack Enter when the side rail (or anything else) owns focus —
            // let the focused button handle it natively.
            if (!sideRail.activeFocus && routeStack.handleNavigationKey(event.key)) {
                event.accepted = true
                return
            }
        }
        if (globalShortcut(event, true))
            event.accepted = true
    }

    Rectangle { anchors.fill: parent; color: Theme.bg; visible: !(root.hasPlayer && root.player.visible) }

    RowLayout {
        id: contentLayer
        anchors.fill: parent
        spacing: 0
        visible: !(root.hasPlayer && root.player.visible)
        enabled: !(root.hasPlayer && root.player.visible)

        SideRail {
            id: sideRail
            Layout.fillHeight: true
            Layout.preferredWidth: route === "login" ? 0 : Metrics.railWidth(root.width)
            visible: route !== "login"
            currentRoute: root.route
            onNavigate: (r) => {
                if (r === "home") root.goHome()
                else if (r === "settings") root.pushRoute("settings")
                else root.pushRoute(r)
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

        onVisibleChanged: if (visible) forceActiveFocus()
        onActiveFocusChanged: if (visible && !activeFocus) forceActiveFocus()

        Keys.priority: Keys.BeforeItem
        Keys.onPressed: (event) => {
            if (root.handlePlayerPressed(event))
                event.accepted = true
        }
        Keys.onReleased: (event) => {
            if (root.handlePlayerReleased(event))
                event.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: root.busyValue && !(root.hasPlayer && root.player.visible)
        color: "#AA0E0E0E"
        z: 40
        Surface {
            anchors.centerIn: parent
            width: Math.min(620, parent.width - 96)
            height: 104
            elevated: true
            Row {
                anchors.centerIn: parent
                spacing: 18
                BusyIndicator { running: true; width: 30; height: 30 }
                AppText { text: root.busyTextValue; font.pixelSize: Metrics.bodyPx(root.width) + 2; anchors.verticalCenter: parent.verticalCenter }
            }
        }
    }

    MediaInfoOverlay {
        anchors.fill: parent
        visible: root.mediaInfoVisible
        item: visible ? (root.mediaInfoItem && Object.keys(root.mediaInfoItem).length > 0 ? root.mediaInfoItem : root.currentMediaItem()) : ({})
        z: 59
        onClosed: { root.mediaInfoVisible = false; root.mediaInfoItem = ({}) }
    }
    ShortcutOverlay { anchors.fill: parent; visible: root.shortcutOverlayVisible; z: 60; onClosed: root.shortcutOverlayVisible = false }
    DiagnosticsOverlay { anchors.fill: parent; visible: root.diagnosticsVisible && !(root.hasPlayer && root.player.visible); route: root.route; focusedItemId: String(root.lastGridIndex); z: 61 }
    ToastLayer { id: toast; anchors.fill: parent; z: 70 }

    Surface {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 32
        width: Math.min(parent.width * 0.72, 960)
        height: root.errorTextValue.length > 0 ? errorText.implicitHeight + 28 : 0
        visible: root.errorTextValue.length > 0
        baseColor: "#2A1717"
        z: 80
        AppText {
            id: errorText
            anchors.centerIn: parent
            width: Math.max(0, parent.width - 28)
            text: root.errorTextValue
            color: "#FFD6D6"
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        MouseArea { anchors.fill: parent; onClicked: if (appController) appController.clearError() }
    }
}
