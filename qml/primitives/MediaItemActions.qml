import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as QQC
import QtQuick.Templates as T
import "../theme"

T.Control {
    id: root

    property var item: ({})
    property var shell
    property bool focused: false
    property bool tvPlatform: nativeWindow ? nativeWindow.tvPlatform : false
    property bool menuOpen: false
    property bool pendingAccept: false
    property bool longPressOpened: false
    property bool pointerLongPress: false
    property int menuIndex: 0
    property var menuOptions: []
    property bool favoriteState: Boolean(item && item.favorite)
    property bool playedState: Boolean(item && item.played)
    property string longPressAction: "menu"
    readonly property int windowWidth: root.Window.window ? root.Window.window.width : 1920
    readonly property int menuEdgeMargin: Math.max(12, Metrics.gap(windowWidth))
    readonly property int menuRowHeight: Math.max(46, Metrics.controlHeight(windowWidth))
    readonly property int menuPanelWidth: Math.min(windowWidth - menuEdgeMargin * 2,
                                                   Math.max(320, Math.min(392, Math.round(windowWidth * 0.22))))
    readonly property int menuPanelHeight: menuOptions.length <= 0 ? 0
                                                                  : menuOptions.length * menuRowHeight
                                                                    + (menuOptions.length - 1) * 4 + 16
    readonly property bool actionable: Boolean(item && item.movieId)
    readonly property string itemType: item && item.itemType ? String(item.itemType) : ""
    readonly property bool episodeOrSeason: itemType === "Episode" || itemType === "Season"
    readonly property bool hasProgress: Number(item && item.resumeTicks ? item.resumeTicks : 0) > 0
    readonly property bool partialEpisode: itemType === "Episode" && hasProgress && !playedState

    signal activated()
    signal detailsRequested()
    signal favoriteToggled(bool favorite)
    signal playedToggled(bool played)
    signal mediaInfoRequested()

    function handleAcceptPressed(key) {
        if (!InputKeys.isAccept(key))
            return false
        if (menuOpen)
            return false
        pendingAccept = true
        longPressOpened = false
        if (tvPlatform && (actionable || longPressAction === "details"))
            holdTimer.restart()
        return true
    }

    onItemChanged: {
        syncItemState()
        if (menuOpen)
            closeMenu()
    }

    Connections {
        target: appController
        function onItemFavoriteChanged(itemId, favorite) {
            if (root.item && String(root.item.movieId || "") === itemId)
                root.favoriteState = favorite
        }
        function onItemPlayedChanged(itemId, played) {
            if (root.item && String(root.item.movieId || "") === itemId)
                root.playedState = played
        }
    }

    function syncItemState() {
        favoriteState = Boolean(item && item.favorite)
        playedState = Boolean(item && item.played)
    }

    function handleAcceptReleased(key) {
        if (!InputKeys.isAccept(key) || !pendingAccept)
            return false
        holdTimer.stop()
        const opened = longPressOpened
        pendingAccept = false
        longPressOpened = false
        if (!opened)
            activated()
        return true
    }

    function menuParentItem() {
        return root.Window.window ? root.Window.window.contentItem : root
    }

    function clamp(value, minimum, maximum) {
        return Math.max(minimum, Math.min(maximum, value))
    }

    function positionMenu() {
        const target = menuParentItem()
        const anchor = root.mapToItem(target, 0, 0)
        const edge = root.menuEdgeMargin
        const maxX = Math.max(edge, target.width - menuPopup.width - edge)
        const maxY = Math.max(edge, target.height - menuPopup.height - edge)
        const desiredX = anchor.x + root.width - menuPopup.width
        const belowY = anchor.y + Math.min(root.height, root.menuRowHeight) + 8
        const aboveY = anchor.y - menuPopup.height - 8
        const desiredY = belowY + menuPopup.height <= target.height - edge ? belowY : aboveY
        menuPopup.x = clamp(desiredX, edge, maxX)
        menuPopup.y = clamp(desiredY, edge, maxY)
    }

    function openMenu() {
        if (!rebuildMenu())
            return false
        menuIndex = 0
        menuOpen = true
        forceActiveFocus()
        positionMenu()
        menuPopup.open()
        Qt.callLater(positionMenu)
        return true
    }

    function closeMenu() {
        menuOpen = false
        pendingAccept = false
        longPressOpened = false
        holdTimer.stop()
        if (menuPopup.opened)
            menuPopup.close()
    }

    function rebuildMenu() {
        const options = []
        if (episodeOrSeason && item.seriesId)
            options.push({ action: "series", icon: "live_tv", label: "Go to series", checked: false })
        if ((itemType === "Episode" && item.seriesId && item.seasonId)
                || (itemType === "Season" && item.seriesId && item.movieId))
            options.push({ action: "season", icon: "video_library", label: "Go to season", checked: false })
        if (actionable) {
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
        }
        menuOptions = options
        return menuOptions.length > 0
    }

    function showLibraryGridRoute() {
        if (shell && shell.replaceRoute)
            shell.replaceRoute("libraryGrid")
    }

    function seasonTitle() {
        return item && item.seasonNumber > 0 ? "Season " + item.seasonNumber : "Season"
    }

    function openSeries() {
        if (!item || !item.seriesId || !appController)
            return
        showLibraryGridRoute()
        appController.openSeriesById(item.seriesId, item.seriesName || "")
    }

    function openSeason() {
        if (!item || !item.seriesId || !appController)
            return
        showLibraryGridRoute()
        appController.openSeasonById(item.seriesId,
                                     item.itemType === "Season" ? item.movieId : (item.seasonId || ""),
                                     item.itemType === "Season" ? (item.title || seasonTitle()) : seasonTitle())
    }

    function activateMenuIndex(index) {
        if (index < 0 || index >= menuOptions.length)
            return
        const action = menuOptions[index].action
        if (action === "series") {
            openSeries()
        } else if (action === "season") {
            openSeason()
        } else if (action === "played") {
            playedState = !playedState
            playedToggled(playedState)
        } else if (action === "clear") {
            playedState = false
            if (appController)
                appController.clearProgress(String(item.movieId || ""))
        } else if (action === "favorite") {
            favoriteState = !favoriteState
            favoriteToggled(favoriteState)
        }
        closeMenu()
    }

    function handleNavigationKey(key) {
        if (!menuOpen)
            return false
        if (InputKeys.isBack(key, false, false) || InputKeys.isHorizontal(key)) {
            closeMenu()
            return true
        }
        if (key === Qt.Key_Up) {
            menuIndex = Math.max(0, menuIndex - 1)
            return true
        }
        if (key === Qt.Key_Down) {
            menuIndex = Math.min(menuOptions.length - 1, menuIndex + 1)
            return true
        }
        if (InputKeys.isAccept(key)) {
            activateMenuIndex(menuIndex)
            return true
        }
        return false
    }

    component OverlayButton: Item {
        id: buttonRoot
        property string iconName: "favorite"
        property bool checked: false
        signal clicked()

        width: 36
        height: 36

        Rectangle {
            anchors.fill: parent
            radius: 18
            color: buttonRoot.checked ? Theme.accentPanel : "#D9141414"
            border.width: 1
            border.color: buttonRoot.checked ? Theme.accent : Theme.controlOutline
            antialiasing: true
        }

        MaterialIcon {
            anchors.centerIn: parent
            name: buttonRoot.iconName
            iconSize: 20
            iconColor: buttonRoot.checked ? Theme.accent : Theme.textPrimary
        }

        MouseArea {
            anchors.fill: parent
            onClicked: buttonRoot.clicked()
        }
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

    anchors.fill: parent

    Timer {
        id: holdTimer
        interval: 520
        repeat: false
        onTriggered: {
            if (root.longPressAction === "details") {
                root.longPressOpened = true
                root.detailsRequested()
            } else {
                root.longPressOpened = root.openMenu()
            }
        }
    }

    hoverEnabled: true

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        pressAndHoldInterval: 520
        onPressed: root.pointerLongPress = false
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                root.pointerLongPress = true
                root.openMenu()
                return
            }
            if (!root.pointerLongPress)
                root.activated()
            root.pointerLongPress = false
        }
        onPressAndHold: {
            root.pointerLongPress = true
            if (root.longPressAction === "details")
                root.detailsRequested()
            else
                root.openMenu()
        }
    }

    Row {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 8
        spacing: 7
        z: 4
        visible: root.actionable && !root.tvPlatform
                 && (root.hovered || root.playedState || root.favoriteState)

        OverlayButton {
            iconName: root.playedState ? "check_circle" : "radio_button_unchecked"
            checked: root.playedState
            onClicked: {
                root.playedState = !root.playedState
                root.playedToggled(root.playedState)
            }
        }

        OverlayButton {
            iconName: root.favoriteState ? "favorite" : "favorite_border"
            checked: root.favoriteState
            onClicked: {
                root.favoriteState = !root.favoriteState
                root.favoriteToggled(root.favoriteState)
            }
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

            Column {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                Repeater {
                    model: root.menuOptions
                    delegate: MenuRow {
                        required property int index
                        optionIndex: index
                        iconName: modelData.icon
                        label: modelData.label
                        checked: Boolean(modelData.checked)
                    }
                }
            }
        }

        Keys.onPressed: (event) => {
            if (root.handleNavigationKey(event.key))
                event.accepted = true
        }
    }
}
