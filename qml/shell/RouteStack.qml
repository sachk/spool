import QtQuick
import "../primitives"

FocusScope {
    id: root

    property string route: "login"
    property var shell
    focus: true
    property bool ready: false
    property var uiTransitionToken: 0

    // Resident-page host: pages are created once and route changes switch
    // visibility + focus only. login/scaleSetup are destroyed on leave;
    // itemDetails/personDetails/search are evicted under memory pressure.
    property var pages: ({})
    property var activeLoader: null
    property var pendingLoader: null
    readonly property Item activeItem: activeLoader ? activeLoader.item : null

    function pageKey(nextRoute) {
        switch (nextRoute) {
        case "login":
            return "login"
        case "scaleSetup":
            return "scaleSetup"
        case "libraryGrid":
            return "libraryGrid"
        case "itemDetails":
            return "itemDetails"
        case "personDetails":
            return "personDetails"
        case "search":
            return "search"
        case "settings":
        case "subtitleSettings":
            return "settings"
        default:
            // home and libraries share one HomePage instance; it reads
            // Router.route directly for its libraries-only mode.
            return "home"
        }
    }

    function pageSource(key) {
        switch (key) {
        case "login":
            return Qt.resolvedUrl("../pages/LoginPage.qml")
        case "scaleSetup":
            return Qt.resolvedUrl("../pages/ScaleSetupPage.qml")
        case "libraryGrid":
            return Qt.resolvedUrl("../pages/LibraryGridPage.qml")
        case "itemDetails":
            return Qt.resolvedUrl("../pages/ItemDetailsPage.qml")
        case "personDetails":
            return Qt.resolvedUrl("../pages/PersonDetailsPage.qml")
        case "search":
            return Qt.resolvedUrl("../pages/SearchPage.qml")
        case "settings":
            return Qt.resolvedUrl("../pages/SettingsPage.qml")
        default:
            return Qt.resolvedUrl("../pages/HomePage.qml")
        }
    }

    function loaderFor(key) {
        if (pages[key])
            return pages[key]
        const loader = pageLoaderComponent.createObject(root)
        loader.setSource(pageSource(key), {
                             "shell": root.shell
                         })
        pages[key] = loader
        return loader
    }

    function showRoute() {
        const loader = loaderFor(pageKey(route))
        pendingLoader = loader
        const warm = loader.status === Loader.Ready && Boolean(loader.item)
        uiTransitionToken = InputLatency.beginUiTransition("route:" + route + (warm ? ":warm" : ":cold"))
        if (warm)
            activatePending()
    }

    function handleLoaded(loader) {
        if (pendingLoader === loader)
            activatePending()
    }

    function activatePending() {
        const loader = pendingLoader
        if (!loader || loader.status !== Loader.Ready || !loader.item)
            return
        pendingLoader = null
        const item = loader.item
        if (item.shell !== root.shell)
            item.shell = root.shell
        if (pageKey(route) === "settings")
            item.subtitleEditor = route === "subtitleSettings"
        if (activeLoader !== loader) {
            const previous = activeLoader
            activeLoader = loader
            loader.visible = true
            if (previous)
                previous.visible = false
        }
        InputKeys.focus(item)
        dropTransientPages()
        if (Session.authenticated && !pages["settings"])
            prewarmTimer.start()
        completeUiTransitionIfReady()
    }

    function dropTransientPages() {
        for (const key of ["login", "scaleSetup"]) {
            const loader = pages[key]
            if (loader && loader !== activeLoader) {
                delete pages[key]
                loader.destroy()
            }
        }
    }

    // Memory-pressure eviction: keep the active page plus the cheap,
    // frequently visited residents (home/settings/libraryGrid).
    function trim() {
        for (const key of ["itemDetails", "personDetails", "search"]) {
            const loader = pages[key]
            if (loader && loader !== activeLoader) {
                delete pages[key]
                loader.destroy()
                console.info("route host: evicted", key)
            }
        }
    }

    function completeUiTransitionIfReady() {
        if (!activeItem || uiTransitionToken === 0)
            return
        if (typeof activeItem.contentReady !== "undefined" && !activeItem.contentReady)
            return
        InputLatency.markUiTransitionReady(uiTransitionToken)
        uiTransitionToken = 0
    }

    onRouteChanged: if (ready)
                        showRoute()
    Component.onCompleted: {
        ready = true
        showRoute()
    }

    Connections {
        target: root.activeItem
        ignoreUnknownSignals: true
        function onContentReadyChanged() {
            root.completeUiTransitionIfReady()
        }
    }

    function routeKey(key, phase, repeat) {
        return Boolean(activeItem && activeItem.routeKey && activeItem.routeKey(key, phase, repeat))
    }

    function activate() {
        if (activeItem && activeItem.activate)
            activeItem.activate()
    }

    function longPress() {
        return Boolean(activeItem && activeItem.longPress && activeItem.longPress())
    }

    function back() {
        return Boolean(activeItem && activeItem.back && activeItem.back())
    }

    Timer {
        id: prewarmTimer
        interval: 1500
        repeat: false
        onTriggered: if (!root.pages["settings"])
                         root.loaderFor("settings")
    }

    Component {
        id: pageLoaderComponent

        Loader {
            id: pageLoader
            anchors.fill: parent
            asynchronous: true
            visible: false
            onLoaded: root.handleLoaded(pageLoader)
            onStatusChanged: if (status === Loader.Error)
                                 console.warn("route host: failed to load", source)
        }
    }
}
