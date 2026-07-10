import QtQuick
import "../primitives"

FocusScope {
    id: root
    property string route: "login"
    property var shell
    focus: true
    property bool ready: false

    function routeSource(nextRoute) {
        switch (nextRoute) {
        case "login":
            return Qt.resolvedUrl("../pages/LoginPage.qml")
        case "scaleSetup":
            return Qt.resolvedUrl("../pages/ScaleSetupPage.qml")
        case "home":
        case "libraries":
            return Qt.resolvedUrl("../pages/HomePage.qml")
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

    function loadRoute() {
        const source = routeSource(route)
        loader.setSource(source, {
                             "shell": root.shell
                         })
    }

    onRouteChanged: if (ready)
                        loadRoute()
    onShellChanged: {
        if (ready && loader.item)
            loader.item.shell = root.shell
    }
    Component.onCompleted: {
        ready = true
        loadRoute()
    }
    function routeKey(key, phase, repeat) {
        return Boolean(loader.item && loader.item.routeKey && loader.item.routeKey(key, phase, repeat))
    }

    function activate() {
        if (loader.item && loader.item.activate)
            loader.item.activate()
    }

    function longPress() {
        return Boolean(loader.item && loader.item.longPress && loader.item.longPress())
    }

    function back() {
        return Boolean(loader.item && loader.item.back && loader.item.back())
    }

    Loader {
        id: loader
        anchors.fill: parent
        focus: true
        asynchronous: true
        onLoaded: {
            if (!item)
                return
            if (item.shell !== root.shell)
                item.shell = root.shell
            InputKeys.focus(item)
        }
    }
}
