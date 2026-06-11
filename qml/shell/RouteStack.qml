import QtQuick
import "../pages"

FocusScope {
    id: root
    property string route: "login"
    property var shell
    focus: true
    Accessible.role: Accessible.Pane
    Accessible.name: route === "login" ? "Sign in"
                     : route === "home" ? "My Media"
                     : route === "libraries" ? "Libraries"
                     : route === "libraryGrid" ? "Library"
                     : route === "itemDetails" ? "Item details"
                     : route === "personDetails" ? "Person details"
                     : route === "search" ? "Search"
                     : route === "settings" ? "Settings"
                     : route === "playerOverlay" ? "Playback controls"
                     : "Jellyfin"

    function handleNavigationKey(key) {
        if (loader.item && loader.item.handleNavigationKey)
            return loader.item.handleNavigationKey(key)
        return false
    }

    function handlePressedKey(key) {
        if (loader.item && loader.item.handlePressedKey)
            return loader.item.handlePressedKey(key)
        return false
    }

    function handleBack() {
        if (loader.item && loader.item.handleBack)
            return loader.item.handleBack()
        return false
    }

    Loader {
        id: loader
        anchors.fill: parent
        focus: true
        sourceComponent: route === "login" ? loginComponent
                         : route === "home" ? homeComponent
                         : route === "libraries" ? librariesComponent
                         : route === "libraryGrid" ? gridComponent
                         : route === "itemDetails" ? detailsComponent
                         : route === "personDetails" ? personComponent
                         : route === "search" ? searchComponent
                         : route === "settings" ? settingsComponent
                         : route === "playerOverlay" ? playerComponent
                         : homeComponent
        onLoaded: item.forceActiveFocus()
    }

    Component { id: loginComponent; LoginPage { shell: root.shell } }
    Component { id: homeComponent; HomePage { shell: root.shell } }
    Component { id: librariesComponent; LibrariesPage { shell: root.shell } }
    Component { id: gridComponent; LibraryGridPage { shell: root.shell } }
    Component { id: detailsComponent; ItemDetailsPage { shell: root.shell } }
    Component { id: personComponent; PersonDetailsPage { shell: root.shell } }
    Component { id: searchComponent; SearchPage { shell: root.shell } }
    Component { id: settingsComponent; SettingsPage { shell: root.shell } }
    Component { id: playerComponent; PlayerOverlayPage { shell: root.shell } }
}
