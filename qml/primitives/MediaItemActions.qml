import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import "../theme"

T.Control {
    id: root

    property var item: ({})
    property bool focused: false
    property bool tvPlatform: nativeWindow ? nativeWindow.tvPlatform : false
    property bool menuOpen: false
    property bool pendingAccept: false
    property bool longPressOpened: false
    property bool pointerLongPress: false
    property int menuIndex: 0
    property bool favoriteState: Boolean(item && item.favorite)
    property bool playedState: Boolean(item && item.played)
    property string longPressAction: "menu"
    readonly property bool actionable: Boolean(item && item.movieId)

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

    onItemChanged: syncItemState()

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

    function openMenu() {
        if (!actionable)
            return false
        menuIndex = 0
        menuOpen = true
        forceActiveFocus()
        return true
    }

    function closeMenu() {
        menuOpen = false
        pendingAccept = false
        longPressOpened = false
        holdTimer.stop()
    }

    function activateMenuIndex(index) {
        if (!actionable)
            return
        if (index === 0) {
            playedState = !playedState
            playedToggled(playedState)
        } else if (index === 1) {
            favoriteState = !favoriteState
            favoriteToggled(favoriteState)
        } else {
            mediaInfoRequested()
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
            menuIndex = Math.min(2, menuIndex + 1)
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

        width: menuPanel.width - 12
        height: 42

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusSmall
            color: root.menuIndex === rowRoot.optionIndex ? Theme.focusedFill : "transparent"
            border.width: root.menuIndex === rowRoot.optionIndex ? 1 : 0
            border.color: Theme.accent
        }

        Row {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 10

            MaterialIcon {
                anchors.verticalCenter: parent.verticalCenter
                name: rowRoot.iconName
                iconSize: 19
                iconColor: Theme.textSecondary
            }

            AppText {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 38
                text: rowRoot.label
                color: Theme.textPrimary
                font.pixelSize: Metrics.metaPx(root.Window.window ? root.Window.window.width : 1920)
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }

        MouseArea {
            anchors.fill: parent
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
        acceptedButtons: Qt.LeftButton
        pressAndHoldInterval: 520
        onPressed: root.pointerLongPress = false
        onClicked: {
            if (!root.pointerLongPress)
                root.activated()
            root.pointerLongPress = false
        }
        onPressAndHold: {
            if (!root.tvPlatform)
                return
            root.pointerLongPress = true
            if (root.longPressAction === "details") {
                root.longPressOpened = true
                root.detailsRequested()
            } else {
                root.openMenu()
            }
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

    Rectangle {
        id: menuPanel
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 8
        width: Math.min(260, Math.max(210, parent.width - 16))
        height: menuOpen ? menuColumn.implicitHeight + 12 : 0
        visible: menuOpen
        z: 8
        radius: Theme.radiusMedium
        color: Theme.floatingPanel
        border.width: 1
        border.color: Theme.borderStrong
        clip: true

        Column {
            id: menuColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 6

            MenuRow {
                optionIndex: 0
                iconName: root.playedState ? "radio_button_unchecked" : "check_circle"
                label: root.playedState ? "Mark unwatched" : "Mark watched"
            }
            MenuRow {
                optionIndex: 1
                iconName: root.favoriteState ? "favorite_border" : "favorite"
                label: root.favoriteState ? "Remove favourite" : "Add favourite"
            }
            MenuRow {
                optionIndex: 2
                iconName: "info"
                label: "Media info"
            }
        }
    }
}
