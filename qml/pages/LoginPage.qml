import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    property var shell
    focus: true

    function focusServer() { serverRow.focusRow() }

    Keys.onPressed: (event) => {
        // Submit form from any focused control via Enter when the row currently
        // owns focus (not the embedded TextField, which handles its own input).
        if (InputKeys.isMedia(event.key)) {
            appController.login()
            event.accepted = true
        }
    }

    Rectangle { anchors.fill: parent; color: Theme.bg }
    RowLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin(width)
        spacing: Metrics.gap(width) * 2

        Surface {
            id: formPanel
            Layout.preferredWidth: Math.min(660, root.width * 0.46)
            Layout.maximumWidth: 720
            Layout.fillHeight: true
            baseColor: Theme.bgRaised
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 28
                spacing: 14
                Rectangle { Layout.preferredWidth: 46; Layout.preferredHeight: 46; radius: 8; gradient: Gradient { GradientStop { position: 0; color: Theme.jellyfinBlue } GradientStop { position: 1; color: Theme.jellyfinPurple } } }
                AppText { text: "Jellyfin Native"; font.pixelSize: Metrics.titlePx(root.width); font.weight: Font.DemiBold }
                AppText { text: "Native Qt 6.11 client for LG webOS"; color: Theme.textSecondary; font.pixelSize: Metrics.bodyPx(root.width) }

                TextFieldRow {
                    id: serverRow
                    Layout.fillWidth: true
                    label: "Server URL"
                    placeholderText: "https://jellyfin.example.com"
                    text: appController.serverUrl
                    inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                    focus: true
                    onTextEdited: appController.setServerUrl(text)
                    onAccepted: userRow.focusField()
                    KeyNavigation.down: userRow
                }
                TextFieldRow {
                    id: userRow
                    Layout.fillWidth: true
                    label: "Username"
                    placeholderText: "Username"
                    text: appController.username
                    inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                    onTextEdited: appController.setUsername(text)
                    onAccepted: passRow.focusField()
                    KeyNavigation.up: serverRow
                    KeyNavigation.down: passRow
                }
                TextFieldRow {
                    id: passRow
                    Layout.fillWidth: true
                    label: "Password"
                    placeholderText: "Password"
                    text: appController.password
                    echoMode: TextInput.Password
                    inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                    onTextEdited: appController.setPassword(text)
                    onAccepted: signInButton.forceActiveFocus()
                    KeyNavigation.up: userRow
                    KeyNavigation.down: signInButton
                }
                ToggleRow { id: rememberRow; Layout.fillWidth: true; title: "Remember me"; description: "Keep this server and user in local state"; checked: true; KeyNavigation.up: passRow; KeyNavigation.down: signInButton }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    ActionButton {
                        id: signInButton
                        text: "Sign In"
                        kind: "primary"
                        Layout.fillWidth: true
                        onClicked: appController.login()
                        KeyNavigation.up: passRow
                        KeyNavigation.right: quickConnectButton
                        KeyNavigation.down: discoveredList
                    }
                    ActionButton {
                        id: quickConnectButton
                        text: appController.quickConnectActive ? "Cancel Quick Connect" : "Quick Connect"
                        Layout.fillWidth: true
                        onClicked: appController.quickConnectActive ? appController.cancelQuickConnect() : appController.startQuickConnect()
                        KeyNavigation.up: passRow
                        KeyNavigation.left: signInButton
                        KeyNavigation.down: discoveredList
                    }
                }
                Surface { Layout.fillWidth: true; Layout.preferredHeight: 92; visible: appController.quickConnectActive; baseColor: Theme.bgPanel; Column { anchors.centerIn: parent; spacing: 6; AppText { text: appController.quickConnectCode; font.pixelSize: 32; font.weight: Font.Bold } MonoText { text: appController.quickConnectStatus } } }
                Item { Layout.fillHeight: true }
                MonoText { text: "Qt 6.11 · LG webOS"; color: Theme.textMuted }
            }
        }

        Surface {
            Layout.fillWidth: true
            Layout.fillHeight: true
            baseColor: Theme.bgPanel
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 14
                AppText { text: "Discovered Servers"; font.pixelSize: Metrics.titlePx(root.width) - 4; font.weight: Font.DemiBold }
                AppText { text: "Press Select to use a server. The URL field updates automatically."; color: Theme.textMuted; font.pixelSize: Metrics.metaPx(root.width) }
                ListView {
                    id: discoveredList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 10
                    focus: false
                    keyNavigationEnabled: true
                    model: appController.discoveredServers
                    KeyNavigation.left: signInButton
                    onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)
                    FastWheelHandler { flickable: discoveredList }
                    delegate: Surface {
                        required property int index
                        required property string name
                        required property string address
                        width: discoveredList.width
                        height: 70
                        focused: ListView.isCurrentItem && discoveredList.activeFocus
                        RowLayout { anchors.fill: parent; anchors.margins: 14; AppText { text: name; Layout.fillWidth: true; font.weight: Font.Medium } MonoText { text: address } }
                        MouseArea { anchors.fill: parent; onClicked: { discoveredList.currentIndex = index; appController.chooseDiscoveredServer(index) } }
                    }
                    Keys.onPressed: (event) => {
                        if (InputKeys.isAccept(event.key, false)) {
                            appController.chooseDiscoveredServer(currentIndex)
                            event.accepted = true
                        }
                    }
                }
                EmptyPlaceholder { Layout.fillWidth: true; visible: discoveredList.count === 0; title: "No local servers found"; detail: "Enter a server manually or wait for LAN discovery." }
            }
        }
    }
}
