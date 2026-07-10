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

    readonly property bool hasSavedPair: App.hasDefaultProfile && Session.serverUrl.length > 0
    readonly property bool textInputActive: shell ? shell.textInputActive : Qt.inputMethod.visible
    readonly property bool manualServerVisible: manualServerAddress.length > 0
    readonly property int tileSize: width >= 1920 ? 190 : width >= 1280 ? 164 : 152
    readonly property int contentWidth: Math.min(width - Metrics.pageMargin(width) * 2, 1040)
    readonly property string savedServerName: "Jellyfin Server"
    readonly property string savedServerAddress: Session.serverUrl
    readonly property string savedUsername: Session.username.length > 0 ? Session.username : "Saved user"
    readonly property string chosenServerName: selectedServerName.length > 0 ? selectedServerName : savedServerName
    readonly property string chosenServerAddress: selectedServerAddress.length > 0 ? selectedServerAddress : Session
                                                                                     ? Session.serverUrl : ""

    focus: true

    function firstInitial(value) {
        const text = String(value || "").trim()
        return text.length > 0 ? text.charAt(0).toUpperCase() : "?"
    }

    function profileTint(value) {
        const palette = ["#1F4631", "#314026", "#243F46", "#3E3147", "#49352B", "#2D3D55"]
        let hash = 0
        const text = String(value || "")
        for (let i = 0; i < text.length; ++i)
            hash = ((hash << 5) - hash + text.charCodeAt(i)) | 0
        return palette[Math.abs(hash) % palette.length]
    }

    function normalizeServerUrl(value) {
        const trimmed = String(value || "").trim()
        if (trimmed.length === 0)
            return ""
        return /^https?:\/\//i.test(trimmed) ? trimmed : "https://" + trimmed
    }

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
        const address = normalizeServerUrl(manualServerDraft)
        if (address.length === 0)
            return
        manualServerAddress = address
        manualServerStatus = "Checking"
        selectedServerName = savedServerName
        selectedServerAddress = address
        Session.serverUrl = address
        manualProbe.restart()
        Qt.callLater(function () {
            InputKeys.focus(manualServerCard)
        })
    }

    function chooseManualServer() {
        if (!manualServerVisible || manualServerStatus !== "Online")
            return
        selectedServerName = savedServerName
        selectedServerAddress = manualServerAddress
        Session.serverUrl = manualServerAddress
        addStep = 2
        Qt.callLater(function () {
            usernameRow.focusField()
        })
    }

    function chooseDiscoveredServer(index, name, address) {
        if (index < 0)
            return
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
        App.login()
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

    Timer {
        id: manualProbe
        interval: 650
        repeat: false
        onTriggered: manualServerStatus = "Online"
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
        anchors.leftMargin: Metrics.pageMargin(root.width)
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
            spacing: 28

            ProfileTile {
                id: profileTile
                tileSize: root.tileSize
                username: root.savedUsername
                serverName: root.savedServerName
                serverAddress: root.savedServerAddress
                avatarColor: root.profileTint(username + serverAddress)
                initial: root.firstInitial(username)
                defaultProfile: App.hasDefaultProfile
                onAccepted: root.enterProfile()
            }

            ProfileTile {
                id: addAccountTile
                tileSize: root.tileSize
                addTile: true
                username: "Add account"
                onAccepted: root.openAddAccount()
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
            anchors.leftMargin: Metrics.pageMargin(root.width)
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

            TextFieldRow {
                id: urlRow
                Layout.fillWidth: true
                label: "URL"
                placeholderText: "https://jellyfin.example.com"
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
                title: root.savedServerName
                address: root.manualServerAddress
                status: root.manualServerStatus
                selectable: root.manualServerStatus === "Online"
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

                    width: discoveredList.width
                    title: name.length > 0 ? name : root.savedServerName
                    address: address
                    status: "Online"
                    focused: ListView.isCurrentItem && discoveredList.activeFocus
                    onAccepted: root.chooseDiscoveredServer(index, title, address)

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            discoveredList.currentIndex = index
                            root.chooseDiscoveredServer(index, title, address)
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
            anchors.topMargin: Math.max(110, Math.round(parent.height * 0.19))
            width: Math.min(root.contentWidth, 620)
            spacing: 16

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
                    onClicked: root.signIn()
                }

                ActionButton {
                    id: quickConnectButton
                    text: QuickConnect.active ? "Cancel" : "Quick Connect"
                    iconName: "bolt"
                    Layout.fillWidth: true
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
                Layout.preferredHeight: 92
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

                    MonoText {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: QuickConnect.status
                        color: Theme.textSecondary
                    }
                }
            }
        }
    }

    component ProfileTile: FocusScope {
        id: tile

        property int tileSize: 152
        property string username: ""
        property string serverName: ""
        property string serverAddress: ""
        property string initial: ""
        property color avatarColor: "#1F4631"
        property bool defaultProfile: false
        property bool addTile: false
        property bool pointerHovered: hover.hovered

        signal accepted

        width: tileSize
        height: tileSize + 76
        focus: true
        focusPolicy: Qt.StrongFocus
        scale: activeFocus && !Theme.reducedMotion ? 1.055 : 1.0

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
            width: tile.tileSize
            height: tile.tileSize
            radius: Theme.radiusMedium
            color: tile.addTile ? Theme.bgRaised : tile.avatarColor
            border.width: tile.activeFocus ? 3 : tile.pointerHovered ? 1 : 0
            border.color: tile.activeFocus ? Theme.accent : Theme.borderStrong
            antialiasing: true

            AppText {
                anchors.centerIn: parent
                text: tile.addTile ? "+" : tile.initial
                color: tile.addTile ? Theme.accent : Theme.textPrimary
                font.pixelSize: tile.addTile ? Math.round(tile.tileSize * 0.34) : Math.round(tile.tileSize * 0.42)
                font.weight: Font.DemiBold
            }

            AppText {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: 10
                anchors.topMargin: 8
                visible: tile.defaultProfile && !tile.addTile
                text: "★"
                color: "#F6C544"
                font.pixelSize: 22
                font.weight: Font.Bold
            }
        }

        Rectangle {
            anchors.top: avatar.bottom
            anchors.topMargin: 8
            anchors.horizontalCenter: parent.horizontalCenter
            width: tile.activeFocus ? Math.round(tile.tileSize * 0.74) : 0
            height: 3
            radius: 2
            color: Theme.accentPurple
            opacity: tile.activeFocus ? 1 : 0

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
            anchors.topMargin: 14
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 3

            AppText {
                width: parent.width
                text: tile.username
                color: Theme.textPrimary
                font.pixelSize: 18
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            AppText {
                width: parent.width
                visible: !tile.addTile
                text: tile.serverName
                color: Theme.textSecondary
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            MonoText {
                width: parent.width
                visible: !tile.addTile
                text: tile.serverAddress
                color: Theme.textMuted
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                maximumLineCount: 1
                elide: Text.ElideRight
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: tile.accepted()
        }

        HoverHandler {
            id: hover
        }
    }

    component ServerChoice: FocusScope {
        id: choice

        property string title: ""
        property string address: ""
        property string status: ""
        property bool selectable: true
        property bool focused: activeFocus
        property bool pointerHovered: hover.hovered

        signal accepted

        implicitHeight: 78
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
            anchors.margins: 14
            spacing: 14

            MaterialIcon {
                name: "dns"
                iconSize: 24
                iconColor: choice.focused ? Theme.accent : Theme.textSecondary
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                AppText {
                    Layout.fillWidth: true
                    text: choice.title
                    font.pixelSize: Metrics.bodyPx(root.width)
                    font.weight: Font.Medium
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }

                MonoText {
                    Layout.fillWidth: true
                    text: choice.address
                    color: Theme.textMuted
                    font.pixelSize: Metrics.metaPx(root.width)
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }
            }

            AppText {
                text: choice.status
                color: choice.status === "Online" ? Theme.success : Theme.textSecondary
                font.pixelSize: Metrics.metaPx(root.width)
                font.weight: Font.Medium
                maximumLineCount: 1
            }
        }

        HoverHandler {
            id: hover
        }
    }
}
