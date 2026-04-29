import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    property var shell
    focus: true
    Keys.onReleased: (event) => { if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { appController.login(); event.accepted = true } }

    Rectangle { anchors.fill: parent; color: Theme.bg }
    RowLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin(width)
        spacing: Metrics.gap(width) * 2

        Surface {
            Layout.preferredWidth: Math.min(660, root.width * 0.46)
            Layout.maximumWidth: 720
            Layout.fillHeight: true
            baseColor: Theme.bgRaised
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 28
                spacing: 16
                Rectangle { Layout.preferredWidth: 46; Layout.preferredHeight: 46; radius: 8; gradient: Gradient { GradientStop { position: 0; color: Theme.jellyfinBlue } GradientStop { position: 1; color: Theme.jellyfinPurple } } }
                AppText { text: "Jellyfin Native"; font.pixelSize: Metrics.titlePx(root.width); font.weight: Font.DemiBold }
                AppText { text: "Native Qt 6.11 client for LG webOS"; color: Theme.textSecondary; font.pixelSize: Metrics.bodyPx(root.width) }
                TextField { id: serverField; Layout.fillWidth: true; placeholderText: "Server URL"; text: appController.serverUrl; focus: true; inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText; onTextChanged: appController.setServerUrl(text) }
                TextField { Layout.fillWidth: true; placeholderText: "Username"; text: appController.username; inputMethodHints: Qt.ImhNoPredictiveText; onTextChanged: appController.setUsername(text) }
                TextField { Layout.fillWidth: true; placeholderText: "Password"; echoMode: TextInput.Password; text: appController.password; inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText; onTextChanged: appController.setPassword(text) }
                ToggleRow { Layout.fillWidth: true; title: "Remember me"; description: "Keep this server and user in local state"; checked: true }
                RowLayout { Layout.fillWidth: true; ActionButton { text: "Sign In"; kind: "primary"; Layout.fillWidth: true; onClicked: appController.login() } ActionButton { text: appController.quickConnectActive ? "Cancel Quick Connect" : "Quick Connect"; Layout.fillWidth: true; onClicked: appController.quickConnectActive ? appController.cancelQuickConnect() : appController.startQuickConnect() } }
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
                AppText { text: "Enter selects a server and fills the URL."; color: Theme.textMuted; font.pixelSize: Metrics.metaPx(root.width) }
                ListView {
                    id: discoveredList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 10
                    model: appController.discoveredServers
                    delegate: Surface {
                        required property int index
                        required property string name
                        required property string address
                        width: discoveredList.width
                        height: 70
                        focused: ListView.isCurrentItem
                        RowLayout { anchors.fill: parent; anchors.margins: 14; AppText { text: name; Layout.fillWidth: true; font.weight: Font.Medium } MonoText { text: address } }
                        MouseArea { anchors.fill: parent; onClicked: { discoveredList.currentIndex = index; appController.chooseDiscoveredServer(index) } }
                    }
                    Keys.onReleased: (event) => { if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { appController.chooseDiscoveredServer(currentIndex); event.accepted = true } }
                }
                EmptyPlaceholder { Layout.fillWidth: true; visible: discoveredList.count === 0; title: "No local servers found"; detail: "Enter a server manually or wait for LAN discovery." }
            }
        }
    }
}
