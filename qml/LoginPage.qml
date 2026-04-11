import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

FocusScope {
    id: loginPage
    focus: true

    Keys.onReleased: (event) => {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            appController.login()
            event.accepted = true
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 80
        spacing: 26

        Item { Layout.fillHeight: true; Layout.preferredHeight: 30 }

        Label {
            text: "Jellyfin Native"
            font.pixelSize: 72
            font.weight: Font.DemiBold
            color: "#f1fbff"
            Layout.alignment: Qt.AlignHCenter
        }

        Label {
            text: "Native Qt Quick client for LG webOS"
            font.pixelSize: 26
            color: "#82daf5"
            Layout.alignment: Qt.AlignHCenter
        }

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 20
            Layout.preferredWidth: 980
            Layout.maximumWidth: 980
            Layout.fillWidth: true
            implicitHeight: formColumn.implicitHeight + 56
            Layout.preferredHeight: implicitHeight
            radius: 32
            color: "#70111d28"
            border.color: "#2d607d"
            border.width: 1

            ColumnLayout {
                id: formColumn
                anchors.fill: parent
                anchors.margins: 28
                spacing: 18

                TextField {
                    id: serverField
                    Layout.fillWidth: true
                    placeholderText: "Server URL"
                    text: appController.serverUrl
                    inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText
                    focus: true
                    onTextChanged: appController.setServerUrl(text)
                    onActiveFocusChanged: {
                        if (activeFocus)
                            Qt.inputMethod.show()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 18

                    TextField {
                        Layout.fillWidth: true
                        placeholderText: "Username"
                        text: appController.username
                        inputMethodHints: Qt.ImhNoPredictiveText
                        onTextChanged: appController.setUsername(text)
                        onActiveFocusChanged: {
                            if (activeFocus)
                                Qt.inputMethod.show()
                        }
                    }

                    TextField {
                        Layout.fillWidth: true
                        placeholderText: "Password"
                        echoMode: TextInput.Password
                        text: appController.password
                        inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                        onTextChanged: appController.setPassword(text)
                        onActiveFocusChanged: {
                            if (activeFocus)
                                Qt.inputMethod.show()
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 260
                        text: "Sign In"
                        onClicked: appController.login()
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 260
                        text: appController.quickConnectActive ? "Cancel Quick Connect" : "Quick Connect"
                        onClicked: {
                            if (appController.quickConnectActive)
                                appController.cancelQuickConnect()
                            else
                                appController.startQuickConnect()
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    visible: appController.quickConnectActive
                    radius: 24
                    color: "#501a2e40"
                    border.color: "#76d8ef"
                    border.width: 1
                    implicitHeight: quickConnectColumn.implicitHeight + 28

                    ColumnLayout {
                        id: quickConnectColumn
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10

                        Label {
                            text: "Quick Connect"
                            font.pixelSize: 26
                            color: "#dff8ff"
                        }

                        Label {
                            text: appController.quickConnectCode
                            font.pixelSize: 48
                            font.letterSpacing: 6
                            font.weight: Font.Bold
                            color: "#82daf5"
                        }

                        Label {
                            text: appController.quickConnectStatus
                            font.pixelSize: 22
                            color: "#edf8ff"
                        }
                    }
                }
            }
        }

        Label {
            text: "Detected local servers"
            font.pixelSize: 24
            color: "#9bc2d4"
            Layout.leftMargin: 24
        }

        ListView {
            id: discoveredList
            Layout.fillWidth: true
            Layout.preferredHeight: 300
            clip: true
            spacing: 12
            model: appController.discoveredServers
            visible: count > 0

            delegate: Rectangle {
                required property int index
                required property string name
                required property string address

                width: discoveredList.width
                height: 76
                radius: 24
                color: ListView.isCurrentItem ? "#2a6f92" : "#48131d28"
                border.color: ListView.isCurrentItem ? "#c8f5ff" : "#2a536a"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 18

                    Label {
                        text: name
                        color: "#eff9ff"
                        font.pixelSize: 28
                        Layout.fillWidth: true
                    }

                    Label {
                        text: address
                        color: "#8ec7de"
                        font.pixelSize: 22
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        discoveredList.currentIndex = index
                        appController.chooseDiscoveredServer(index)
                    }
                    onDoubleClicked: {
                        discoveredList.currentIndex = index
                        appController.chooseDiscoveredServer(index)
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 132
            visible: discoveredList.count === 0
            radius: 24
            color: "#30131d28"
            border.color: "#2a536a"
            border.width: 1

            Column {
                anchors.centerIn: parent
                spacing: 10

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "No local Jellyfin servers found yet."
                    color: "#d7ebf5"
                    font.pixelSize: 24
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Enter a server manually or wait for the next LAN probe."
                    color: "#8ec7de"
                    font.pixelSize: 20
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
