import QtQuick
import QtQuick.Templates as T
import "../theme"

T.Control {
    id: root

    property var shell
    property var itemProvider: null
    property bool focused: false
    property bool tvPlatform: NativeWindow.tvPlatform
    property bool pendingAccept: false
    property bool longPressOpened: false
    property bool pointerLongPress: false
    property var item: ({})
    readonly property string movieId: String(item.movieId || "")
    readonly property string itemType: String(item.itemType || "")
    readonly property real resumeTicks: Number(item.resumeTicks || 0)
    readonly property bool favorite: Boolean(item.favorite)
    readonly property bool played: Boolean(item.played)
    property bool favoriteState: favorite
    property bool playedState: played
    property string longPressAction: "menu"
    readonly property bool actionable: movieId.length > 0

    signal activated
    signal detailsRequested
    signal favoriteToggled(bool favorite)
    signal playedToggled(bool played)
    signal mediaInfoRequested

    anchors.fill: parent
    hoverEnabled: true

    onFavoriteChanged: favoriteState = favorite
    onPlayedChanged: playedState = played
    onMovieIdChanged: {
        favoriteState = favorite
        playedState = played
        pendingAccept = false
        longPressOpened = false
        holdTimer.stop()
    }

    function providedItem() {
        return itemProvider ? (itemProvider() || ({})) : (item || ({}))
    }

    function openMenu() {
        if (!shell || !shell.openItemMenu)
            return false
        return shell.openItemMenu(providedItem(), root)
    }

    function handleAcceptPressed(key) {
        if (!InputKeys.isAccept(key))
            return false
        // Key auto-repeat re-delivers press events while OK is held; restarting
        // the hold timer on each one kept the long-press menu from ever opening.
        if (pendingAccept)
            return true
        pendingAccept = true
        longPressOpened = false
        if (tvPlatform && (actionable || longPressAction === "details"))
            holdTimer.restart()
        return true
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

    component OverlayButton: Item {
        id: buttonRoot
        property string iconName: "favorite"
        property bool checked: false
        signal clicked

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

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        pressAndHoldInterval: 520
        onPressed: root.pointerLongPress = false
        onClicked: mouse => {
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
        visible: root.actionable && !root.tvPlatform && (root.hovered || root.playedState || root.favoriteState)

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
}
