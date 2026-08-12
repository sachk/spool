import QtQuick
import "../theme"

// A saved account on the picker: initial-avatar tile, name, and the server it
// belongs to. Everything it draws is derived from the account itself so the
// picker only has to hand it the profile fields.
FocusScope {
    id: root

    property int tileSize: Metrics.scaled(152)
    property string username: ""
    property string serverName: ""
    property string serverAddress: ""
    property bool needsSignIn: false
    property bool addTile: false
    property bool focused: activeFocus

    readonly property string initial: {
        const name = String(username).trim()
        return name.length > 0 ? name.charAt(0).toUpperCase() : "?"
    }
    // A stable per-account tint keeps a wall of same-shaped tiles telling
    // itself apart before any of the labels are readable across a room.
    readonly property color avatarColor: {
        const palette = ["#1F4631", "#314026", "#243F46", "#3E3147", "#49352B", "#2D3D55"]
        const name = String(username)
        let hash = 0
        for (let i = 0; i < name.length; ++i)
            hash = ((hash << 5) - hash + name.charCodeAt(i)) | 0
        return palette[Math.abs(hash) % palette.length]
    }

    signal accepted
    signal contextRequested

    width: tileSize
    height: tileSize + Metrics.scaled(76)
    focus: true
    focusPolicy: Qt.StrongFocus
    scale: focused && !Theme.reducedMotion ? 1.055 : 1.0

    Behavior on scale {
        enabled: !Theme.reducedMotion
        NumberAnimation {
            duration: 120
            easing.type: Easing.OutCubic
        }
    }

    Rectangle {
        id: avatar
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: root.tileSize
        height: root.tileSize
        radius: Theme.radiusMedium
        color: root.addTile ? Theme.bgRaised : root.avatarColor
        border.width: root.focused ? Theme.focusBorderWidth : hover.hovered ? Theme.hoverBorderWidth : 0
        border.color: root.focused ? Theme.accent : Theme.borderStrong
        antialiasing: true

        MaterialIcon {
            anchors.centerIn: parent
            visible: root.addTile
            name: "add"
            iconSize: Math.round(root.tileSize * 0.34)
            iconColor: Theme.accent
        }

        AppText {
            anchors.centerIn: parent
            visible: !root.addTile
            text: root.initial
            font.pixelSize: Math.round(root.tileSize * 0.42)
            font.weight: Font.DemiBold
        }

        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Metrics.scaled(8)
            width: Metrics.scaled(30)
            height: width
            radius: width / 2
            visible: root.needsSignIn
            color: Theme.errorPanel
            border.width: Theme.hoverBorderWidth
            border.color: Theme.errorText

            MaterialIcon {
                anchors.centerIn: parent
                name: "lock"
                iconSize: Metrics.scaled(17)
                iconColor: Theme.errorText
            }
        }
    }

    Rectangle {
        anchors.top: avatar.bottom
        anchors.topMargin: Metrics.scaled(8)
        anchors.horizontalCenter: parent.horizontalCenter
        width: root.focused ? Math.round(root.tileSize * 0.74) : 0
        height: Metrics.scaled(3)
        radius: Metrics.scaled(2)
        color: Theme.accentAlternate
        opacity: root.focused ? 1 : 0

        Behavior on width {
            enabled: !Theme.reducedMotion
            NumberAnimation {
                duration: 120
                easing.type: Easing.OutCubic
            }
        }
    }

    Column {
        anchors.top: avatar.bottom
        anchors.topMargin: Metrics.scaled(14)
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Metrics.scaled(3)

        AppText {
            width: parent.width
            text: root.username
            font.pixelSize: Metrics.scaled(18)
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            maximumLineCount: 1
            elide: Text.ElideRight
        }

        AppText {
            width: parent.width
            visible: !root.addTile
            text: root.serverName
            color: Theme.textSecondary
            font.pixelSize: Metrics.scaled(13)
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            maximumLineCount: 1
            elide: Text.ElideRight
        }

        SecondaryText {
            width: parent.width
            visible: !root.addTile
            text: root.serverAddress
            color: Theme.textMuted
            font.pixelSize: Metrics.scaled(11)
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            maximumLineCount: 1
            elide: Text.ElideRight
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: mouse => {
            if (mouse.button === Qt.RightButton)
                root.contextRequested()
            else
                root.accepted()
        }
        onPressAndHold: root.contextRequested()
    }

    HoverHandler {
        id: hover
    }
}
