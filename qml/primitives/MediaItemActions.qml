import QtQuick
import "../theme"

Item {
    id: root

    property var shell
    property bool tvPlatform: NativeWindow.tvPlatform
    property var item: ({})
    property bool favoriteState: Boolean(item.favorite)
    property bool playedState: Boolean(item.played)
    readonly property string itemId: String(item.movieId || "")
    readonly property bool actionable: itemId.length > 0

    signal activated

    anchors.fill: parent

    onItemChanged: {
        favoriteState = Boolean(item.favorite)
        playedState = Boolean(item.played)
    }

    function openMenu() {
        if (!shell || !shell.openItemMenu)
            return false
        return shell.openItemMenu(item || ({}), root)
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

    MouseArea {
        property bool longPressed: false
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        pressAndHoldInterval: 520
        onPressed: longPressed = false
        onClicked: mouse => {
                       if (mouse.button === Qt.RightButton) {
                           root.openMenu()
                       } else if (!longPressed) {
                           root.activated()
                       }
                       longPressed = false
                   }
        onPressAndHold: {
            longPressed = true
            root.openMenu()
        }
    }

    Row {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 8
        spacing: 7
        z: 4
        visible: root.actionable && !root.tvPlatform && (hover.hovered || root.playedState || root.favoriteState)

        OverlayButton {
            iconName: root.playedState ? "check_circle" : "radio_button_unchecked"
            checked: root.playedState
            onClicked: {
                root.playedState = !root.playedState
                ItemState.setPlayed(root.itemId, root.playedState)
            }
        }

        OverlayButton {
            iconName: root.favoriteState ? "favorite" : "favorite_border"
            checked: root.favoriteState
            onClicked: {
                root.favoriteState = !root.favoriteState
                ItemState.setFavorite(root.itemId, root.favoriteState)
            }
        }
    }
    HoverHandler {
        id: hover
    }
}
