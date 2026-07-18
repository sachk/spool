pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"
import "ProfileNavigation.js" as ProfileNavigation

FocusScope {
    id: root

    property var shell
    property bool addMode: Session.accountProfiles.length === 0 || Session.profileSignInRequired
    property int addStep: Session.profileSignInRequired ? 2 : 1
    property string selectedServerName: Session.serverName
    property string selectedServerAddress: Session.serverUrl
    property string manualServerDraft: ""
    property string manualServerAddress: ""
    property string manualServerStatus: ""
    property string manualServerVersion: ""
    property string manualProbeInput: ""
    property string profileActionMode: ""
    property string profileActionId: ""
    property Item profileActionAnchor: null
    property string profileActionName: ""
    property string profileActionUrl: ""

    readonly property bool hasSavedPair: Session.accountProfiles.length > 0
    readonly property bool textInputActive: shell ? shell.textInputActive : Qt.inputMethod.visible
    readonly property bool manualServerVisible: manualServerAddress.length > 0
    readonly property int contentWidth: Math.min(width - Metrics.pageMarginPx * 2, 1040)
    readonly property int tileSize: width >= 1920 ? Metrics.scaled(190) : width >= 1280 ? Metrics.scaled(164) :
                                                                                          Metrics.scaled(152)
    readonly property string savedServerName: "Jellyfin Server"
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

    function enterProfile(profileId) {
        App.useProfile(profileId)
    }
    function openProfileActions(profileId, anchor, serverName, serverUrl) {
        profileActionId = profileId
        profileActionAnchor = anchor
        profileActionName = serverName
        profileActionUrl = serverUrl
        profileActionMode = "menu"
    }

    function closeProfileActions() {
        profileActionMode = ""
        profileActionId = ""
        profileActionAnchor = null
        Qt.callLater(function () {
            InputKeys.focus(profileList)
        })
    }

    function chooseProfileAction(index) {
        if (index === 0) {
            Session.prepareProfileSignIn(profileActionId)
            closeProfileActions()
            addMode = true
            addStep = 2
            return
        }
        profileActionMode = index === 1 ? "edit" : "remove"
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
            InputKeys.focus(profileList)
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
        if (profileDialogLoader.item)
            return profileDialogLoader.item.back()
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
            return [profileList, addAccountTile]
        if (addStep === 2)
            return [chosenServerCard, usernameRow, passwordRow, signInButton, quickConnectButton]
        const items = [urlRow]
        if (manualServerVisible)
            items.push(manualServerCard)
        if (Discovery.tlsTrustPending)
            items.push(trustCertificateButton)
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
        if (!addMode && (current === profileList || current === addAccountTile)) {
            const next = ProfileNavigation.move(profileList.currentIndex, profileList.count, current === addAccountTile,
                                                delta)
            if (next.addFocused) {
                InputKeys.focus(addAccountTile)
            } else {
                profileList.currentIndex = next.profileIndex
                profileList.positionViewAtIndex(next.profileIndex, ListView.Contain)
                InputKeys.focus(profileList)
            }
            return
        }
        if (current === profileList) {
            const nextProfile = profileList.currentIndex + delta
            if (nextProfile >= 0 && nextProfile < profileList.count) {
                profileList.currentIndex = nextProfile
                profileList.positionViewAtIndex(nextProfile, ListView.Contain)
                return
            }
        }
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
        if (profileDialogLoader.item)
            return profileDialogLoader.item.routeKey(key, phase, repeat)
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
        if (profileDialogLoader.item) {
            profileDialogLoader.item.activate()
            return
        }
        const control = focusedControl()
        if (control === profileList && profileList.currentItem)
            enterProfile(profileList.currentItem.profileId)
        else if (control === addAccountTile)
            openAddAccount()
        else if (control === manualServerCard)
            chooseManualServer()
        else if (control === trustCertificateButton)
            Discovery.trustPendingCertificate()
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
        target: Session

        function onProfileSignInRequiredChanged() {
            if (!Session.profileSignInRequired)
                return
            root.selectedServerName = Session.serverName
            root.selectedServerAddress = Session.serverUrl
            root.addMode = true
            root.addStep = 2
            Qt.callLater(function () {
                usernameRow.focusField()
            })
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
            root.manualServerStatus = Discovery.tlsTrustPending ? "Certificate not trusted" : "Not found"
            root.manualServerVersion = Discovery.tlsTrustPending ? Discovery.pendingTlsFingerprint : message
        }

        function onTlsTrustPendingChanged() {
            if (!Discovery.tlsTrustPending)
                return
            root.manualServerStatus = "Certificate not trusted"
            root.manualServerVersion = Discovery.pendingTlsFingerprint
            Qt.callLater(function () {
                InputKeys.focus(trustCertificateButton)
            })
        }
    }

    Component.onCompleted: Qt.callLater(function () {
        if (hasSavedPair)
            InputKeys.focus(profileList)
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

        Column {
            anchors.bottom: profileRow.top
            anchors.bottomMargin: Metrics.scaled(30)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Metrics.scaled(8)

            AppText {
                width: Math.min(root.contentWidth, Metrics.scaled(720))
                text: "Who’s watching?"
                font.pixelSize: Metrics.titleSizePx + Metrics.scaled(10)
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }

            MonoText {
                width: Math.min(root.contentWidth, Metrics.scaled(720))
                text: "Choose a Jellyfin account and its paired server"
                color: Theme.textSecondary
                font.pixelSize: Metrics.bodySizePx
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Row {
            id: profileRow
            anchors.centerIn: parent
            spacing: Metrics.scaled(28)

            ListView {
                id: profileList
                width: Math.min(count * root.tileSize + Math.max(0, count - 1) * spacing, root.tileSize * 4 + spacing
                                * 3)
                height: root.tileSize + Metrics.scaled(76)
                orientation: ListView.Horizontal
                spacing: Metrics.scaled(28)
                clip: contentWidth > width
                focus: false
                keyNavigationEnabled: false
                boundsBehavior: Flickable.StopAtBounds
                model: Session.accountProfiles
                currentIndex: count > 0 ? 0 : -1

                delegate: ProfileTile {
                    required property int index
                    required property var modelData
                    readonly property string profileId: String(modelData.profileId || "")

                    tileSize: root.tileSize
                    username: String(modelData.userName || "Saved account")
                    serverName: String(modelData.serverName || root.savedServerName)
                    serverAddress: String(modelData.serverHost || modelData.serverUrl || "")
                    status: String(modelData.status || "")
                    initial: root.firstInitial(username)
                    focused: ListView.isCurrentItem && profileList.activeFocus
                    onAccepted: root.enterProfile(profileId)
                    onContextRequested: root.openProfileActions(profileId, this, serverName, String(modelData.serverUrl
                                                                                                    || ""))
                }
            }

            ProfileTile {
                id: addAccountTile
                tileSize: root.tileSize
                addTile: true
                username: "Add account"
                onAccepted: root.openAddAccount()
            }
        }
        MonoText {
            anchors.top: profileRow.bottom
            anchors.topMargin: Metrics.scaled(18)
            anchors.horizontalCenter: parent.horizontalCenter
            visible: profileList.count > 4
            text: (profileList.currentIndex + 1) + " / " + profileList.count
            color: Theme.textMuted
            font.pixelSize: Metrics.metaSizePx
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
            // Keep both fields above the tall virtual keyboard. Moving the
            // whole form is more stable than chasing keyboard animation.
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

    Loader {
        id: profileDialogLoader
        anchors.fill: parent
        active: root.profileActionMode.length > 0
        z: 200
        sourceComponent: root.profileActionMode === "menu" ? profileActionMenu : root.profileActionMode === "remove"
                                                             ? removeProfileDialog : editProfileDialog
    }

    Component {
        id: profileActionMenu

        OptionPickerDialog {
            title: "Account actions"
            options: ["Sign in again", "Edit server", "Remove from this device"]
            currentIndex: 0
            anchorItem: root.profileActionAnchor
            onSelected: index => root.chooseProfileAction(index)
            onDismissed: root.closeProfileActions()
        }
    }

    Component {
        id: removeProfileDialog

        ConfirmationDialog {
            title: "Remove " + root.profileActionName + "?"
            message: "This removes the saved account and token for " + root.profileActionName
                     + " on this device. Other accounts are unchanged."
            confirmText: "Remove"
            destructive: true
            onAccepted: {
                Session.removeProfile(root.profileActionId)
                root.closeProfileActions()
                if (Session.accountProfiles.length === 0)
                root.openAddAccount()
            }
            onDismissed: root.closeProfileActions()
        }
    }

    Component {
        id: editProfileDialog

        FocusScope {
            id: editDialog
            anchors.fill: parent
            focus: true

            function routeKey(key, phase, repeat) {
                if (InputKeys.isBack(key, false, false)) {
                    if (phase === "release")
                        root.closeProfileActions()
                    return true
                }
                if (!InputKeys.isDirection(key))
                    return InputKeys.isAccept(key)
                if (phase !== "press")
                    return true
                const controls = [nameField, addressField, cancelEditButton, saveEditButton]
                let index = nameField.activeFocus ? 0 : addressField.activeFocus ? 1 : cancelEditButton.activeFocus ? 2 :
                                                                                                                      3
                if (key === Qt.Key_Down || key === Qt.Key_Right)
                    index = Math.min(controls.length - 1, index + 1)
                else if (key === Qt.Key_Up || key === Qt.Key_Left)
                    index = Math.max(0, index - 1)
                if (controls[index] === nameField || controls[index] === addressField)
                    controls[index].focusRow()
                else
                    InputKeys.focus(controls[index])
                return true
            }

            function activate() {
                if (saveEditButton.activeFocus)
                    saveEditButton.clicked()
                else if (cancelEditButton.activeFocus)
                    root.closeProfileActions()
            }

            function back() {
                root.closeProfileActions()
                return true
            }

            Component.onCompleted: Qt.callLater(function () {
                nameField.focusRow()
            })

            Rectangle {
                anchors.fill: parent
                color: "#99000000"
            }

            Surface {
                anchors.centerIn: parent
                width: Math.min(parent.width - Metrics.scaled(96), Metrics.scaled(620))
                height: editContent.implicitHeight + Metrics.scaled(48)
                elevated: true
                baseColor: Theme.floatingPanel

                ColumnLayout {
                    id: editContent
                    anchors.fill: parent
                    anchors.margins: Metrics.scaled(24)
                    spacing: Metrics.scaled(14)

                    AppText {
                        Layout.fillWidth: true
                        text: "Edit server"
                        font.pixelSize: Metrics.titleSizePx
                        font.weight: Font.DemiBold
                    }

                    TextFieldRow {
                        id: nameField
                        Layout.fillWidth: true
                        label: "Server label"
                        text: root.profileActionName
                        onTextEdited: root.profileActionName = text
                    }

                    TextFieldRow {
                        id: addressField
                        Layout.fillWidth: true
                        label: "Server address"
                        text: root.profileActionUrl
                        inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                        onTextEdited: root.profileActionUrl = text
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Metrics.scaled(12)

                        Item {
                            Layout.fillWidth: true
                        }

                        ActionButton {
                            id: cancelEditButton
                            text: "Cancel"
                            onClicked: root.closeProfileActions()
                        }

                        ActionButton {
                            id: saveEditButton
                            text: "Save"
                            kind: "primary"
                            onClicked: {
                                Session.updateProfileServer(root.profileActionId, root.profileActionName,
                                                            root.profileActionUrl)
                                root.closeProfileActions()
                            }
                        }
                    }
                }
            }
        }
    }

    component ProfileTile: FocusScope {
        id: tile

        property int tileSize: Metrics.scaled(152)
        property string username: ""
        property string serverName: ""
        property string serverAddress: ""
        property string status: ""
        property string initial: ""
        property color avatarColor: "#1F4631"
        property bool addTile: false
        property bool focused: activeFocus
        property bool pointerHovered: hover.hovered

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
            width: tile.tileSize
            height: tile.tileSize
            radius: Theme.radiusMedium
            color: tile.addTile ? Theme.bgRaised : tile.avatarColor
            border.width: tile.focused ? 3 : tile.pointerHovered ? 1 : 0
            border.color: tile.focused ? Theme.accent : Theme.borderStrong
            antialiasing: true

            AppText {
                anchors.centerIn: parent
                text: tile.addTile ? "+" : tile.initial
                color: tile.addTile ? Theme.accent : Theme.textPrimary
                font.pixelSize: tile.addTile ? Math.round(tile.tileSize * 0.34) : Math.round(tile.tileSize * 0.42)
                font.weight: Font.DemiBold
            }

            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Metrics.scaled(8)
                width: Metrics.scaled(30)
                height: width
                radius: width / 2
                visible: tile.status.length > 0
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
            width: tile.focused ? Math.round(tile.tileSize * 0.74) : 0
            height: Metrics.scaled(3)
            radius: Metrics.scaled(2)
            color: Theme.accentPurple
            opacity: tile.focused ? 1 : 0

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
                text: tile.username
                color: Theme.textPrimary
                font.pixelSize: Metrics.scaled(18)
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
                font.pixelSize: Metrics.scaled(13)
                horizontalAlignment: Text.AlignHCenter
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            MonoText {
                width: parent.width
                visible: !tile.addTile
                text: tile.serverAddress
                color: Theme.textMuted
                font.pixelSize: Metrics.scaled(11)
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
                tile.contextRequested()
                else
                tile.accepted()
            }
            onPressAndHold: tile.contextRequested()
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
