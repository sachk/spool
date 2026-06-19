import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    property var shell
    property bool serverChooserOpen: appController
        ? (!appController.loginSameServer || appController.serverUrl.length <= 0)
        : true
    focus: true

    readonly property int panelGap: Metrics.gap(width)
    readonly property int contentWidth: Math.min(width - Metrics.pageMargin(width) * 2,
                                                 serverChooserOpen ? 1220 : 640)
    readonly property string serverLabel: appController && appController.serverUrl.length > 0
        ? appController.serverUrl
        : "No server selected"

    function focusServer() { openServerChooser() }

    function openServerChooser() {
        serverChooserOpen = true
        Qt.callLater(function() { manualServerRow.focusRow() })
    }

    function closeServerChooser(force) {
        if (!appController || appController.serverUrl.length <= 0)
            return
        if (!force && !appController.loginSameServer)
            return
        serverChooserOpen = false
        Qt.callLater(function() { userRow.focusRow() })
    }

    function chooseServer(index) {
        if (!appController)
            return
        appController.chooseDiscoveredServer(index)
        serverChooserOpen = false
        Qt.callLater(function() { userRow.focusRow() })
    }

    function setSameServer(enabled) {
        if (!appController)
            return
        appController.setLoginSameServer(enabled)
        if (!enabled)
            serverChooserOpen = true
        else if (appController.serverUrl.length > 0)
            serverChooserOpen = false
    }

    function signIn() {
        if (appController)
            appController.login()
    }

    Keys.onPressed: (event) => {
        if (InputKeys.isMedia(event.key)) {
            signIn()
            event.accepted = true
        }
    }

    Connections {
        target: appController
        function onLoginSameServerChanged() {
            if (!appController.loginSameServer)
                root.serverChooserOpen = true
            else if (appController.serverUrl.length > 0)
                root.serverChooserOpen = false
        }
    }

    Component.onCompleted: Qt.callLater(function() {
        if (serverChooserOpen && appController && appController.serverUrl.length <= 0)
            manualServerRow.focusRow()
        else
            userRow.focusRow()
    })

    Rectangle { anchors.fill: parent; color: Theme.bg }

    RowLayout {
        anchors.centerIn: parent
        width: Math.max(320, root.contentWidth)
        height: Math.min(parent.height - Metrics.pageMargin(root.width) * 2, 620)
        spacing: root.panelGap

        Item {
            Layout.preferredWidth: root.serverChooserOpen ? 520 : 640
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 18

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 74

                    Rectangle {
                        id: logoMark
                        width: 46
                        height: 46
                        radius: Theme.radiusMedium
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        gradient: Gradient {
                            GradientStop { position: 0; color: Theme.jellyfinBlue }
                            GradientStop { position: 1; color: Theme.jellyfinPurple }
                        }
                    }

                    Column {
                        anchors.left: logoMark.right
                        anchors.leftMargin: 16
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 4

                        AppText {
                            width: parent.width
                            text: "Jellyfin"
                            font.pixelSize: Metrics.titlePx(root.width) + 4
                            font.weight: Font.DemiBold
                            maximumLineCount: 1
                            elide: Text.ElideRight
                        }
                        MonoText {
                            width: parent.width
                            text: root.serverLabel
                            color: appController && appController.serverUrl.length > 0 ? Theme.textSecondary : Theme.errorText
                            font.pixelSize: Metrics.metaPx(root.width)
                            maximumLineCount: 1
                            elide: Text.ElideRight
                        }
                    }
                }

                AppText {
                    Layout.fillWidth: true
                    text: "Please sign in"
                    font.pixelSize: Metrics.titlePx(root.width)
                    font.weight: Font.DemiBold
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }

                TextFieldRow {
                    id: userRow
                    Layout.fillWidth: true
                    label: "Username"
                    placeholderText: "Username"
                    text: appController ? appController.username : ""
                    inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                    onTextEdited: if (appController) appController.setUsername(text)
                    onAccepted: passRow.focusField()
                    KeyNavigation.down: passRow
                }

                TextFieldRow {
                    id: passRow
                    Layout.fillWidth: true
                    label: "Password"
                    placeholderText: "Password"
                    text: appController ? appController.password : ""
                    echoMode: TextInput.Password
                    inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                    onTextEdited: if (appController) appController.setPassword(text)
                    onAccepted: signInButton.forceActiveFocus()
                    KeyNavigation.up: userRow
                    KeyNavigation.down: sameServerRow
                }

                ToggleRow {
                    id: sameServerRow
                    Layout.fillWidth: true
                    title: "Login to this server next time"
                    description: root.serverLabel
                    checked: appController ? appController.loginSameServer : true
                    onToggled: root.setSameServer(checked)
                    KeyNavigation.up: passRow
                    KeyNavigation.down: signInButton
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    ActionButton {
                        id: signInButton
                        text: "Sign In"
                        iconName: "login"
                        kind: "primary"
                        Layout.fillWidth: true
                        onClicked: root.signIn()
                        KeyNavigation.up: sameServerRow
                        KeyNavigation.right: quickConnectButton
                        KeyNavigation.down: changeServerButton.visible ? changeServerButton : signInButton
                    }

                    ActionButton {
                        id: quickConnectButton
                        text: appController && appController.quickConnectActive ? "Cancel" : "Quick Connect"
                        iconName: "bolt"
                        Layout.fillWidth: true
                        onClicked: {
                            if (appController) {
                                if (appController.quickConnectActive)
                                    appController.cancelQuickConnect()
                                else
                                    appController.startQuickConnect()
                            }
                        }
                        KeyNavigation.up: sameServerRow
                        KeyNavigation.left: signInButton
                        KeyNavigation.down: changeServerButton.visible ? changeServerButton : quickConnectButton
                    }
                }

                ActionButton {
                    id: changeServerButton
                    Layout.fillWidth: true
                    visible: !root.serverChooserOpen
                    text: "Change Server"
                    iconName: "dns"
                    onClicked: root.openServerChooser()
                    KeyNavigation.up: signInButton
                    KeyNavigation.down: changeServerButton
                }

                Surface {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 92
                    visible: appController && appController.quickConnectActive
                    baseColor: Theme.accentPanel

                    Column {
                        anchors.centerIn: parent
                        spacing: 6
                        AppText {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: appController ? appController.quickConnectCode : ""
                            font.pixelSize: 32
                            font.weight: Font.Bold
                        }
                        MonoText {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: appController ? appController.quickConnectStatus : ""
                            color: Theme.textSecondary
                        }
                    }
                }

                Surface {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 74
                    visible: appController && appController.errorText.length > 0
                    baseColor: Theme.errorPanel

                    AppText {
                        anchors.fill: parent
                        anchors.margins: 14
                        text: appController ? appController.errorText : ""
                        color: Theme.errorText
                        font.pixelSize: Metrics.metaPx(root.width)
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Item { Layout.fillHeight: true }

                MonoText {
                    Layout.fillWidth: true
                    text: "Qt 6.11 native client for LG webOS"
                    color: Theme.textMuted
                    font.pixelSize: Metrics.metaPx(root.width)
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }
            }
        }

        Surface {
            id: serverPanel
            Layout.preferredWidth: 660
            Layout.fillHeight: true
            visible: root.serverChooserOpen
            baseColor: Theme.bgPanel

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 22
                spacing: 14

                SectionHeader {
                    Layout.fillWidth: true
                    title: "Servers"
                    detail: discoveredList.count + " found"
                }

                TextFieldRow {
                    id: manualServerRow
                    Layout.fillWidth: true
                    label: "Server URL"
                    placeholderText: "https://jellyfin.example.com"
                    text: appController ? appController.serverUrl : ""
                    inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                    onTextEdited: if (appController) appController.setServerUrl(text)
                    onAccepted: {
                        root.closeServerChooser(true)
                        userRow.focusField()
                    }
                    KeyNavigation.down: discoveredList
                    KeyNavigation.left: userRow
                }

                ListView {
                    id: discoveredList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 10
                    focus: false
                    keyNavigationEnabled: false
                    model: appController ? appController.discoveredServers : null
                    KeyNavigation.up: manualServerRow
                    KeyNavigation.left: signInButton
                    KeyNavigation.down: useServerButton
                    onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)

                    FastWheelHandler { flickable: discoveredList }

                    delegate: Surface {
                        required property int index
                        required property string name
                        required property string address

                        width: discoveredList.width
                        height: 76
                        focused: ListView.isCurrentItem && discoveredList.activeFocus
                        baseColor: address === root.serverLabel ? Theme.accentPanel : Theme.bgRaised

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 14

                            MaterialIcon {
                                name: "dns"
                                iconSize: 24
                                iconColor: address === root.serverLabel ? Theme.accent : Theme.textSecondary
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                AppText {
                                    Layout.fillWidth: true
                                    text: name.length > 0 ? name : "Jellyfin Server"
                                    font.pixelSize: Metrics.bodyPx(root.width)
                                    font.weight: Font.Medium
                                    maximumLineCount: 1
                                    elide: Text.ElideRight
                                }
                                MonoText {
                                    Layout.fillWidth: true
                                    text: address
                                    color: Theme.textMuted
                                    font.pixelSize: Metrics.metaPx(root.width)
                                    maximumLineCount: 1
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                discoveredList.currentIndex = index
                                root.chooseServer(index)
                            }
                        }
                    }

                    Keys.onPressed: (event) => {
                        if (InputKeys.isAccept(event.key, false)) {
                            root.chooseServer(currentIndex)
                            event.accepted = true
                        } else if (event.key === Qt.Key_Up && currentIndex <= 0) {
                            manualServerRow.focusRow()
                            event.accepted = true
                        } else if (event.key === Qt.Key_Down && currentIndex >= count - 1) {
                            useServerButton.forceActiveFocus()
                            event.accepted = true
                        }
                    }
                }

                EmptyPlaceholder {
                    Layout.fillWidth: true
                    visible: discoveredList.count === 0
                    title: "No local servers found"
                    detail: "Enter a URL above."
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    ActionButton {
                        id: useServerButton
                        Layout.fillWidth: true
                        text: "Use Server"
                        iconName: "check"
                        kind: "primary"
                        enabled: appController && appController.serverUrl.length > 0
                        onClicked: root.closeServerChooser(true)
                        KeyNavigation.up: discoveredList
                        KeyNavigation.right: showEveryTimeButton
                    }

                    ActionButton {
                        id: showEveryTimeButton
                        Layout.fillWidth: true
                        text: appController && appController.loginSameServer ? "Show Every Time" : "Keep Picker"
                        iconName: "view_list"
                        onClicked: root.setSameServer(false)
                        KeyNavigation.up: discoveredList
                        KeyNavigation.left: useServerButton
                    }
                }
            }
        }
    }
}
