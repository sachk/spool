pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root

    property var shell
    property bool addMode: !(App.hasDefaultProfile && Session.serverUrl.length > 0)
    property int addStep: 1
    property string selectedServerName: ""
    property string selectedServerAddress: Session.serverUrl
    property string manualServerDraft: ""
    property string manualServerAddress: ""
    property string manualServerStatus: ""
    property string manualServerVersion: ""
    property string manualProbeInput: ""

    readonly property bool hasSavedPair: App.hasDefaultProfile && Session.serverUrl.length > 0
    readonly property bool textInputActive: shell ? shell.textInputActive : Qt.inputMethod.visible
    readonly property bool manualServerVisible: manualServerAddress.length > 0
    readonly property int contentWidth: Math.min(width - Metrics.pageMarginPx * 2, 1040)
    readonly property string savedServerName: "Jellyfin Server"
    readonly property string savedUsername: Session.username.length > 0 ? Session.username : "Saved user"
    readonly property string chosenServerName: selectedServerName.length > 0 ? selectedServerName : savedServerName
    readonly property string chosenServerAddress: selectedServerAddress.length > 0 ? selectedServerAddress : Session
                                                                                     ? Session.serverUrl : ""

    focus: true

    function enterProfile() {
        if (App.useDefaultProfile() && shell)
            shell.replaceRoute("home")
    }

    function openAddAccount() {
        addMode = true
        addStep = 1
        Qt.callLater(focusServerStep)
    }

    function showProfiles() {
        if (!hasSavedPair)
            return
        addMode = false
        Qt.callLater(function () {
            InputKeys.focus(profileTile)
        })
    }

    function submitManualServer() {
        const address = String(manualServerDraft || "").trim()
        if (address.length === 0)
            return
        manualProbeInput = address
        manualServerAddress = address
        manualServerStatus = "Checking"
        manualServerVersion = ""
        selectedServerName = ""
        selectedServerAddress = ""
        Discovery.probeServer(address)
        Qt.callLater(function () {
            InputKeys.focus(manualServerCard)
        })
    }

    function chooseManualServer() {
        if (!manualServerVisible || manualServerStatus.indexOf("Online") !== 0)
            return
        selectedServerName = savedServerName
        selectedServerAddress = manualServerAddress
        App.rememberServer(selectedServerName, manualServerAddress)
        addStep = 2
        Qt.callLater(function () {
            usernameRow.focusField()
        })
    }

    function chooseDiscoveredServer(index, name, address, online) {
        if (index < 0)
            return
        if (!online) {
            manualServerDraft = address
            submitManualServer()
            return
        }
        App.chooseDiscoveredServer(index)
        selectedServerName = name && name.length > 0 ? name : savedServerName
        selectedServerAddress = address
        addStep = 2
        Qt.callLater(function () {
            usernameRow.focusField()
        })
    }

    function focusServerStep() {
        if (!addMode || addStep !== 1)
            return
        if (manualServerVisible)
            InputKeys.focus(manualServerCard)
        else if (discoveredList.count > 0) {
            if (discoveredList.currentIndex < 0)
                discoveredList.currentIndex = 0
            InputKeys.focus(discoveredList)
        } else {
            urlRow.focusRow()
        }
    }

    function signIn() {
        App.clearError()
        Session.login()
    }

    function back() {
        if (addMode && addStep === 2) {
            addStep = 1
            Qt.callLater(focusServerStep)
            return true
        }
        if (addMode && hasSavedPair) {
            showProfiles()
            return true
        }
        return false
    }

    function controls() {
        if (!addMode)
            return [profileTile, addAccountTile]
        if (addStep === 2)
            return [chosenServerCard, usernameRow, passwordRow, signInButton, quickConnectButton]
        const items = [urlRow]
        if (manualServerVisible)
            items.push(manualServerCard)
        if (discoveredList.count > 0)
            items.push(discoveredList)
        return items
    }

    function focusedControl() {
        const items = controls()
        for (let i = 0; i < items.length; ++i)
            if (items[i].activeFocus)
                return items[i]
        return items[0]
    }

    function focusControl(item) {
        if (item === urlRow || item === usernameRow || item === passwordRow)
            item.focusRow()
        else
            InputKeys.focus(item)
    }

    function moveControl(delta) {
        const items = controls()
        const current = focusedControl()
        if (current === discoveredList) {
            const next = discoveredList.currentIndex + delta
            if (next >= 0 && next < discoveredList.count) {
                discoveredList.currentIndex = next
                return
            }
        }
        focusControl(items[Math.max(0, Math.min(items.length - 1, items.indexOf(current) + delta))])
    }

    function routeKey(key, phase, repeat) {
        if (InputKeys.isMedia(key) && phase === "press") {
            signIn()
            return true
        }
        if (!InputKeys.isDirection(key))
            return false
        const current = focusedControl()
        if (InputKeys.isHorizontal(key)) {
            if (!addMode || current === signInButton || current === quickConnectButton)
                moveControl(key === Qt.Key_Right ? 1 : -1)
            return true
        }
        if (!addMode)
            return true
        if (addStep === 2 && current === quickConnectButton && key === Qt.Key_Up)
            focusControl(passwordRow)
        else
            moveControl(key === Qt.Key_Down ? 1 : -1)
        return true
    }

    function activate() {
        const control = focusedControl()
        if (control === profileTile)
            enterProfile()
        else if (control === addAccountTile)
            openAddAccount()
        else if (control === manualServerCard)
            chooseManualServer()
        else if (control === discoveredList && discoveredList.currentItem)
            discoveredList.currentItem.accepted()
        else if (control === urlRow || control === usernameRow || control === passwordRow)
            control.focusField()
        else if (control === chosenServerCard) {
            addStep = 1
            Qt.callLater(focusServerStep)
        } else if (control === quickConnectButton) {
            quickConnectButton.clicked()
        } else {
            signIn()
        }
    }

    Connections {
        target: Discovery

        function onServerProbeSucceeded(input, server, version, plainHttp) {
            if (input !== root.manualProbeInput)
                return
            root.manualServerAddress = server.address
            root.manualServerStatus = plainHttp ? "Online · HTTP" : "Online"
            root.manualServerVersion = version
            root.selectedServerName = server.name
            root.selectedServerAddress = server.address
            Session.serverUrl = server.address
        }

        function onServerProbeFailed(input, message) {
            if (input !== root.manualProbeInput)
                return
            root.manualServerAddress = root.manualProbeInput
            root.manualServerStatus = "Not found"
            root.manualServerVersion = message
        }
    }

    Component.onCompleted: Qt.callLater(function () {
        if (hasSavedPair)
            InputKeys.focus(profileTile)
        else
            focusServerStep()
    })

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
    }

    Row {
        id: brand
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: Metrics.pageMarginPx
        anchors.topMargin: 18
        spacing: 10

        Rectangle {
            width: 24
            height: 24
            radius: Theme.radiusSmall
            color: "transparent"
            border.width: 1
            border.color: Theme.border

            Image {
                anchors.fill: parent
                anchors.margins: 3
                source: ""
                fillMode: Image.PreserveAspectFit
            }
        }

        AppText {
            anchors.verticalCenter: parent.verticalCenter
            text: "Jellyfin"
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }
    }

    Item {
        id: profileScreen
        anchors.fill: parent
        visible: !root.addMode

        Row {
            anchors.centerIn: parent
            spacing: 16

            ActionButton {
                id: profileTile
                width: Metrics.scaled(340)
                height: Metrics.scaled(68)
                text: root.savedUsername + " — " + root.savedServerName
                iconName: "person"
                kind: "primary"
                onClicked: root.enterProfile()
            }

            ActionButton {
                id: addAccountTile
                width: Metrics.scaled(250)
                height: Metrics.scaled(68)
                text: "Add account"
                iconName: "person_add"
                onClicked: root.openAddAccount()
            }
        }
    }

    Item {
        id: addScreen
        anchors.fill: parent
        visible: root.addMode

        IconButton {
            id: backButton
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: Metrics.pageMarginPx
            anchors.topMargin: 64
            width: 46
            height: 46
            iconName: "arrow_back"
            visible: root.hasSavedPair || root.addStep === 2
            onClicked: {
                if (root.addStep === 2) {
                    root.addStep = 1
                    Qt.callLater(root.focusServerStep)
                } else {
                    root.showProfiles()
                }
            }
        }

        ColumnLayout {
            id: serverStep
            visible: root.addStep === 1
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: Math.max(110, Math.round(parent.height * 0.21))
            width: root.contentWidth
            spacing: 16

            AppText {
                Layout.fillWidth: true
                text: "Choose a server"
                font.pixelSize: Metrics.titleSizePx + Metrics.scaled(8)
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }

            MonoText {
                Layout.fillWidth: true
                text: "Select a discovered server or enter its address"
                color: Theme.textSecondary
                font.pixelSize: Metrics.bodySizePx
                horizontalAlignment: Text.AlignHCenter
            }

            TextFieldRow {
                id: urlRow
                Layout.fillWidth: true
                label: "URL"
                placeholderText: "192.168.1.10:8096 or https://jellyfin.example.com"
                text: root.manualServerDraft
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                onTextEdited: root.manualServerDraft = text
                onAccepted: root.submitManualServer()
            }

            ServerChoice {
                id: manualServerCard
                Layout.fillWidth: true
                visible: root.manualServerVisible
                focus: false
                title: root.selectedServerName.length > 0 ? root.selectedServerName : root.savedServerName
                address: root.manualServerAddress
                status: root.manualServerStatus
                detail: root.manualServerVersion
                selectable: root.manualServerStatus.indexOf("Online") === 0
                onAccepted: root.chooseManualServer()
            }

            ListView {
                id: discoveredList
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(390, Math.max(96, contentHeight))
                clip: true
                spacing: 10
                focus: false
                keyNavigationEnabled: false
                boundsBehavior: Flickable.StopAtBounds
                model: DiscoveredServers
                currentIndex: count > 0 ? 0 : -1
                onCountChanged: {
                    if (root.addMode && root.addStep === 1 && count > 0 && !urlRow.editing &&
                        !manualServerCard.activeFocus)
                    Qt.callLater(root.focusServerStep)
                }
                onCurrentIndexChanged: if (currentIndex >= 0)
                positionViewAtIndex(currentIndex, ListView.Contain)

                FastWheelHandler {
                    flickable: discoveredList
                }

                delegate: ServerChoice {
                    required property int index
                    required property string name
                    required property string address
                    required property bool online

                    width: discoveredList.width
                    title: name.length > 0 ? name : root.savedServerName
                    address: address
                    status: online ? "Online" : "Saved"
                    focused: ListView.isCurrentItem && discoveredList.activeFocus
                    onAccepted: root.chooseDiscoveredServer(index, title, address, online)

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            discoveredList.currentIndex = index
                            root.chooseDiscoveredServer(index, title, address, online)
                        }
                    }
                }
            }

            EmptyPlaceholder {
                Layout.fillWidth: true
                visible: discoveredList.count === 0 && !root.manualServerVisible
                title: "No servers found"
                detail: ""
            }
        }

        ColumnLayout {
            id: accountStep
            visible: root.addStep === 2
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            // Keep both fields above webOS's tall virtual keyboard. Moving the
            // whole form is more stable than chasing the keyboard animation,
            // which can otherwise repeatedly dismiss and reopen the IME.
            anchors.topMargin: Math.max(Metrics.scaled(64), Math.round(parent.height * 0.09))
            width: Math.min(root.contentWidth, Metrics.scaled(760))
            spacing: Metrics.scaled(16)

            AppText {
                Layout.fillWidth: true
                text: "Sign in"
                font.pixelSize: Metrics.titleSizePx + Metrics.scaled(8)
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }

            ServerChoice {
                id: chosenServerCard
                Layout.fillWidth: true
                title: root.chosenServerName
                address: root.chosenServerAddress
                status: "Online"
                onAccepted: {
                    root.addStep = 1
                    Qt.callLater(root.focusServerStep)
                }
            }

            TextFieldRow {
                id: usernameRow
                Layout.fillWidth: true
                label: "Username"
                placeholderText: "Username"
                text: Session.username
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                onTextEdited: Session.username = text
                onAccepted: passwordRow.focusField()
            }

            TextFieldRow {
                id: passwordRow
                Layout.fillWidth: true
                label: "Password"
                placeholderText: "Password"
                text: Session.password
                echoMode: TextInput.Password
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                onTextEdited: Session.password = text
                onAccepted: InputKeys.focus(signInButton)
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
                    Layout.preferredHeight: Metrics.scaled(62)
                    onClicked: root.signIn()
                }

                ActionButton {
                    id: quickConnectButton
                    text: QuickConnect.active ? "Cancel" : "Quick Connect"
                    iconName: "bolt"
                    Layout.fillWidth: true
                    Layout.preferredHeight: Metrics.scaled(62)
                    onClicked: {
                        if (QuickConnect.active) {
                            QuickConnect.cancel()
                        } else {
                            App.clearError()
                            QuickConnect.start(Session.serverUrl)
                        }
                    }
                }
            }

            Surface {
                Layout.fillWidth: true
                Layout.preferredHeight: Metrics.scaled(118)
                visible: QuickConnect.active
                baseColor: Theme.accentPanel

                Column {
                    anchors.centerIn: parent
                    spacing: 6

                    AppText {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: QuickConnect.code
                        font.pixelSize: 32
                        font.weight: Font.Bold
                    }

                    AppText {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Log in from another device"
                        color: Theme.textSecondary
                        font.pixelSize: Metrics.scaled(16)
                    }

                    MonoText {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: QuickConnect.status
                        color: Theme.textSecondary
                    }
                }
            }
        }
    }

    component ServerChoice: FocusScope {
        id: choice

        property string title: ""
        property string address: ""
        property string status: ""
        property string detail: ""
        property bool selectable: true
        property bool focused: activeFocus
        property bool pointerHovered: hover.hovered

        signal accepted

        implicitHeight: Metrics.scaled(choice.detail.length > 0 ? 112 : 94)
        focusPolicy: Qt.StrongFocus

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusMedium
            color: choice.focused ? Theme.accentPanel : Theme.bgRaised
            border.width: choice.focused ? 3 : choice.pointerHovered ? 1 : 1
            border.color: choice.focused ? Theme.accent : choice.pointerHovered ? Theme.borderStrong : Theme.border
            opacity: choice.selectable ? 1.0 : 0.68
            antialiasing: true
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: Metrics.scaled(18)
            spacing: Metrics.scaled(16)

            MaterialIcon {
                name: "dns"
                iconSize: Metrics.scaled(30)
                iconColor: choice.focused ? Theme.accent : Theme.textSecondary
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Metrics.scaled(5)

                AppText {
                    Layout.fillWidth: true
                    text: choice.title
                    font.pixelSize: Metrics.bodySizePx + Metrics.scaled(3)
                    font.weight: Font.Medium
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }

                MonoText {
                    Layout.fillWidth: true
                    text: choice.address
                    color: Theme.textMuted
                    font.pixelSize: Metrics.metaSizePx + Metrics.scaled(3)
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }

                MonoText {
                    Layout.fillWidth: true
                    visible: choice.detail.length > 0
                    text: choice.detail
                    color: Theme.textMuted
                    font.pixelSize: Metrics.metaSizePx
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }
            }

            AppText {
                text: choice.status
                color: choice.status.indexOf("Online") === 0 ? Theme.success : choice.status === "Not found"
                                                               ? Theme.errorText : Theme.textSecondary
                font.pixelSize: Metrics.metaSizePx + Metrics.scaled(3)
                font.weight: Font.Medium
                maximumLineCount: 1
            }
        }

        HoverHandler {
            id: hover
        }
    }
}
