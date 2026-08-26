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
    property Item backReturnControl: null

    // Live probe of whatever is in the address field. `probeInput` is the text
    // handed to Discovery, so comparing it with the current draft tells us
    // whether the result on screen still describes what the user has typed.
    property string manualDraft: ""
    property string probeInput: ""
    // idle | checking | online | offline
    property string probeState: "idle"
    property string probeAddress: ""
    property string probeServerName: ""
    property string probeStatus: ""
    property string probeDetail: ""
    // Set when the user commits to an address that is still being checked, so
    // the wait costs them nothing: the moment it answers, we move on.
    property bool advanceWhenOnline: false

    readonly property bool hasSavedAccounts: Session.accountProfiles.length > 0
    // A real on-screen keyboard, not merely a focused field: on desktop the
    // form must not rearrange itself the moment you click into it.
    readonly property bool keyboardVisible: Qt.inputMethod.visible
    readonly property string draftAddress: String(manualDraft || "").trim()
    readonly property bool probeCardVisible: probeInput.length > 0 && probeInput === draftAddress
    readonly property bool probeOnline: probeCardVisible && probeState === "online"
    readonly property int contentWidth: Math.min(width - Metrics.pageMarginPx * 2, Metrics.scaled(1040))
    readonly property string lane: Metrics.lane(width)
    // Profile tiles grow with the room a lane offers rather than with the
    // pixel count of the panel they happen to be drawn on.
    readonly property int tileSize: Metrics.scaled(lane === "wide" ? 190 : lane === "regular" ? 164 : 152)
    readonly property string fallbackServerName: "Jellyfin Server"
    readonly property string chosenServerName: selectedServerName.length > 0 ? selectedServerName : fallbackServerName
    readonly property string chosenServerAddress: selectedServerAddress.length > 0 ? selectedServerAddress :
                                                                                     Session.serverUrl
    readonly property bool canNavigateBack: Boolean(shell && shell.canCancelSwitchUser) || (addMode && (
                                                                                                hasSavedAccounts
                                                                                                || addStep === 2))

    focus: true

    function enterProfile(profileId) {
        App.useProfile(profileId)
    }

    function openAddAccount() {
        addMode = true
        addStep = 1
        Qt.callLater(focusServerStep)
    }

    function showProfiles() {
        if (!hasSavedAccounts)
            return
        addMode = false
        Qt.callLater(function () {
            InputKeys.focus(profileList)
        })
    }

    // Reaching a server is the slow part of adding an account, so the field
    // starts it as soon as the text names somewhere reachable. Enter still
    // forces an attempt for addresses too unusual to recognise.
    function probeDraft(force) {
        const address = draftAddress
        if (address.length === 0) {
            clearProbe()
            return
        }
        if (!force) {
            if (address === probeInput)
                return
            if (!Discovery.looksLikeServerAddress(address)) {
                clearProbe()
                return
            }
        }
        probeInput = address
        probeAddress = address
        advanceWhenOnline = false
        probeServerName = ""
        probeStatus = ""
        probeDetail = ""
        probeState = "checking"
        selectedServerName = ""
        selectedServerAddress = ""
        Discovery.probeServer(address)
    }

    function clearProbe() {
        Discovery.cancelServerProbe()
        probeInput = ""
        advanceWhenOnline = false
        probeState = "idle"
        probeAddress = ""
        probeServerName = ""
        probeStatus = ""
        probeDetail = ""
    }

    function useServer(name, address) {
        selectedServerName = name && name.length > 0 ? name : fallbackServerName
        selectedServerAddress = address
        addStep = 2
        Qt.callLater(function () {
            usernameRow.focusField()
        })
    }

    // Enter or a click means "use this one", whatever the probe is doing. If
    // the answer has not landed yet the intent is queued rather than refused.
    function commitProbedServer() {
        if (probeOnline) {
            advanceWhenOnline = false
            App.rememberServer(probeServerName, probeAddress)
            useServer(probeServerName, probeAddress)
            return
        }
        if (probeState !== "checking")
            probeDraft(true)
        advanceWhenOnline = probeState === "checking"
    }

    // Picking a server that is already listed goes straight to sign-in. A
    // saved entry has been reached before, and re-checking it first would sit
    // the user in front of a spinner to be told what the row already said.
    function chooseDiscoveredServer(index, name, address) {
        if (index < 0)
            return
        App.chooseDiscoveredServer(index)
        useServer(name, address)
    }

    function focusServerStep() {
        if (!addMode || addStep !== 1)
            return
        if (probeCardVisible)
            InputKeys.focus(probeCard)
        else if (discoveredList.count > 0) {
            if (discoveredList.currentIndex < 0)
                discoveredList.currentIndex = 0
            InputKeys.focus(discoveredList)
        } else {
            // The row decides whether that means the caret or the D-pad stop.
            addressRow.focusRow()
        }
    }

    function signIn() {
        App.clearError()
        Session.login()
    }

    function back() {
        if (profileDialogs.open)
            return profileDialogs.back()
        if (addMode && addStep === 2) {
            addStep = 1
            Qt.callLater(focusServerStep)
            return true
        }
        if (addMode && hasSavedAccounts) {
            showProfiles()
            return true
        }
        if (!addMode && shell && shell.canCancelSwitchUser)
            return shell.cancelSwitchUser()
        return false
    }

    function controls() {
        if (!addMode)
            return [profileList, addAccountTile]
        if (addStep === 2)
            return [chosenServerCard, usernameRow, passwordRow, signInButton, quickConnectButton]
        const items = [addressRow]
        if (probeCardVisible)
            items.push(probeCard)
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
        if (item === addressRow || item === usernameRow || item === passwordRow)
            item.focusRow()
        else
            InputKeys.focus(item)
    }

    function focusBackButton(returnControl) {
        if (!backButton.visible)
            return false
        backReturnControl = returnControl
        InputKeys.focus(backButton)
        return true
    }

    function restoreBackButtonFocus() {
        const items = controls()
        const target = items.indexOf(backReturnControl) >= 0 ? backReturnControl : items.length > 0 ? items[0] : null
        backReturnControl = null
        if (target)
            focusControl(target)
    }

    function moveControl(delta) {
        const items = controls()
        const current = focusedControl()
        if (!addMode && (current === profileList || current === addAccountTile)) {
            const previousIndex = profileList.currentIndex
            const addWasFocused = current === addAccountTile
            const next = ProfileNavigation.move(profileList.currentIndex, profileList.count, addWasFocused, delta)
            if (next.addFocused) {
                InputKeys.focus(addAccountTile)
            } else {
                profileList.currentIndex = next.profileIndex
                profileList.positionViewAtIndex(next.profileIndex, ListView.Contain)
                InputKeys.focus(profileList)
            }
            return next.addFocused !== addWasFocused || next.profileIndex !== previousIndex
        }
        if (current === discoveredList) {
            const next = discoveredList.currentIndex + delta
            if (next >= 0 && next < discoveredList.count) {
                discoveredList.currentIndex = next
                return true
            }
        }
        const currentIndex = items.indexOf(current)
        const nextIndex = Math.max(0, Math.min(items.length - 1, currentIndex + delta))
        if (nextIndex === currentIndex)
            return false
        focusControl(items[nextIndex])
        return true
    }

    function routeKey(key, phase, repeat) {
        if (profileDialogs.open)
            return profileDialogs.routeKey(key, phase, repeat)
        if (InputKeys.isMedia(key) && phase === "press") {
            signIn()
            return true
        }
        if (!InputKeys.isDirection(key))
            return false
        if (backButton.activeFocus) {
            if (key === Qt.Key_Right || key === Qt.Key_Down)
                restoreBackButtonFocus()
            return true
        }

        const current = focusedControl()
        if (InputKeys.isHorizontal(key)) {
            let moved = false
            if (!addMode || current === signInButton || current === quickConnectButton)
                moved = moveControl(key === Qt.Key_Right ? 1 : -1)
            if (!moved && key === Qt.Key_Left)
                focusBackButton(current)
            return true
        }
        if (!addMode) {
            if (key === Qt.Key_Up)
                focusBackButton(current)
            return true
        }

        let moved = false
        if (addStep === 2 && current === quickConnectButton && key === Qt.Key_Up) {
            focusControl(passwordRow)
            moved = true
        } else {
            moved = moveControl(key === Qt.Key_Down ? 1 : -1)
        }
        if (!moved && key === Qt.Key_Up)
            focusBackButton(current)
        return true
    }

    function activate() {
        if (profileDialogs.open) {
            profileDialogs.activate()
            return
        }
        if (backButton.activeFocus) {
            backButton.clicked()
            return
        }
        const control = focusedControl()
        if (control === profileList && profileList.currentItem)
            enterProfile(profileList.currentItem.profileId)
        else if (control === addAccountTile)
            openAddAccount()
        else if (control === probeCard)
            commitProbedServer()
        else if (control === discoveredList && discoveredList.currentItem)
            discoveredList.currentItem.accepted()
        else if (control === addressRow || control === usernameRow || control === passwordRow)
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
            if (input !== root.probeInput)
                return
            root.probeState = "online"
            root.probeAddress = server.address
            root.probeServerName = server.name
            root.probeStatus = plainHttp ? "Online · HTTP" : "Online"
            root.selectedServerName = server.name
            root.selectedServerAddress = server.address
            Session.serverUrl = server.address
            if (root.advanceWhenOnline)
                root.commitProbedServer()
        }

        function onServerProbeFailed(input, message) {
            if (input !== root.probeInput)
                return
            root.probeState = "offline"
            root.advanceWhenOnline = false
            root.probeStatus = TlsTrust.pending ? "Not trusted" : "No server"
            root.probeDetail = TlsTrust.pending ? TlsTrust.pendingFingerprint : message
        }
    }

    Component.onCompleted: Qt.callLater(function () {
        if (hasSavedAccounts)
            InputKeys.focus(profileList)
        else
            focusServerStep()
    })

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
    }

    IconButton {
        id: backButton
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: Metrics.pageMarginPx
        width: Metrics.scaled(46)
        height: Metrics.scaled(46)
        iconName: "arrow_back"
        visible: root.canNavigateBack
        onClicked: root.back()
        z: 2
        onVisibleChanged: if (!visible && activeFocus)
        root.restoreBackButtonFocus()
    }

    Item {
        id: profileScreen
        anchors.fill: parent
        visible: !root.addMode

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
                    serverName: String(modelData.serverName || root.fallbackServerName)
                    serverAddress: String(modelData.serverHost || modelData.serverUrl || "")
                    needsSignIn: Boolean(modelData.needsAuthentication)
                    focused: ListView.isCurrentItem && profileList.activeFocus
                    onAccepted: root.enterProfile(profileId)
                    onContextRequested: profileDialogs.show(profileId, this, serverName, String(modelData.serverUrl
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

        SecondaryText {
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

        ColumnLayout {
            id: serverStep
            visible: root.addStep === 1
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            // The heading is the first thing to give up its space: on a short
            // window, and on a TV once the on-screen keyboard has taken the
            // bottom half, the field and the results matter more.
            anchors.topMargin: root.keyboardVisible ? Metrics.scaled(20) : Math.max(Metrics.scaled(32), Math.min(
                                                                                        Math.round(parent.height * 0.16),
                                                                                        Metrics.scaled(150)))
            anchors.bottomMargin: Metrics.pageMarginPx
            width: root.contentWidth
            spacing: Metrics.scaled(16)

            Behavior on anchors.topMargin {
                enabled: !Theme.reducedMotion
                NumberAnimation {
                    duration: 180
                    easing.type: Easing.OutCubic
                }
            }

            AppText {
                Layout.fillWidth: true
                Layout.bottomMargin: Metrics.scaled(12)
                visible: !root.keyboardVisible
                text: "Choose a server"
                font.pixelSize: Metrics.titleSizePx + Metrics.scaled(8)
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }

            TextFieldRow {
                id: addressRow
                Layout.fillWidth: true
                label: "Server address"
                placeholderText: "192.168.1.10:8096 or https://jellyfin.example.com"
                text: root.manualDraft
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                onTextEdited: text => {
                    root.manualDraft = text
                    probeDebounce.restart()
                }
                // Enter before the debounce has even fired still means "go":
                // it starts the check and rides it in when it answers.
                onAccepted: {
                    probeDebounce.stop()
                    root.commitProbedServer()
                }
            }

            // Grows out of the field rather than appearing beneath it, so the
            // results below slide once instead of jumping.
            ServerCard {
                id: probeCard
                Layout.fillWidth: true
                Layout.preferredHeight: root.probeCardVisible ? implicitHeight : 0
                opacity: root.probeCardVisible ? 1 : 0
                clip: true
                focus: false
                // Clickable while it is still checking too: committing early
                // just queues the move rather than turning the click away.
                selectable: root.probeOnline || root.probeState === "checking"
                tone: root.probeState === "checking" ? "pending" : root.probeState === "online" ? "positive" :
                                                                                                  "negative"

                title: root.probeState === "online" && root.probeServerName.length > 0 ? root.probeServerName :
                                                                                         root.probeAddress
                serverAddress: root.probeState === "online" ? root.probeAddress : ""
                status: root.probeState === "checking" ? "Connecting…" : root.probeStatus
                detail: root.probeState === "offline" ? root.probeDetail : ""
                onAccepted: {
                    InputKeys.focus(probeCard)
                    root.commitProbedServer()
                }

                Behavior on Layout.preferredHeight {
                    enabled: !Theme.reducedMotion
                    NumberAnimation {
                        duration: 160
                        easing.type: Easing.OutCubic
                    }
                }

                Behavior on opacity {
                    enabled: !Theme.reducedMotion
                    NumberAnimation {
                        duration: 160
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Metrics.scaled(18)
                spacing: Metrics.scaled(10)

                AppText {
                    text: "On your network"
                    color: Theme.textSecondary
                    font.pixelSize: Metrics.bodySizePx
                    font.weight: Font.Medium
                }

                Item {
                    Layout.fillWidth: true
                }
            }

            // The results keep their space whether or not anything has been
            // found yet, so a server arriving mid-scan slides into a slot that
            // was already there instead of shoving the page around.
            Item {
                id: discoveryBox
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: Metrics.scaled(96)
                Layout.maximumHeight: Metrics.scaled(302)

                ListView {
                    id: discoveredList
                    anchors.fill: parent
                    visible: count > 0
                    clip: true
                    spacing: Metrics.scaled(10)
                    focus: false
                    keyNavigationEnabled: false
                    boundsBehavior: Flickable.StopAtBounds
                    model: DiscoveredServers
                    currentIndex: count > 0 ? 0 : -1
                    onCountChanged: {
                        if (root.addMode && root.addStep === 1 && count > 0 && !addressRow.editing &&
                            !probeCard.activeFocus)
                        Qt.callLater(root.focusServerStep)
                    }
                    onCurrentIndexChanged: if (currentIndex >= 0)
                    positionViewAtIndex(currentIndex, ListView.Contain)

                    FastWheelHandler {
                        flickable: discoveredList
                    }

                    delegate: ServerCard {
                        required property int index
                        required property string name
                        required property string address
                        required property bool online

                        width: discoveredList.width
                        title: name.length > 0 ? name : root.fallbackServerName
                        serverAddress: address
                        status: online ? "Online" : "Saved"
                        tone: online ? "positive" : "neutral"
                        focused: ListView.isCurrentItem && discoveredList.activeFocus
                        onAccepted: {
                            discoveredList.currentIndex = index
                            InputKeys.focus(discoveredList)
                            root.chooseDiscoveredServer(index, title, address)
                        }
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    visible: discoveredList.count === 0
                    radius: Theme.radiusMedium
                    color: "transparent"
                    border.width: 1
                    border.color: Theme.border

                    Column {
                        anchors.centerIn: parent
                        spacing: Metrics.scaled(6)

                        MaterialIcon {
                            anchors.horizontalCenter: parent.horizontalCenter
                            name: Discovery.active ? "wifi_tethering" : "wifi_off"
                            iconSize: Metrics.scaled(26)
                            iconColor: Theme.textDisabled
                        }

                        SecondaryText {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: Discovery.active ? "Looking for servers" : "No servers found here"
                            color: Theme.textMuted
                            font.pixelSize: Metrics.metaSizePx + Metrics.scaled(1)
                        }
                    }
                }
            }

            Timer {
                id: probeDebounce
                interval: 450
                onTriggered: root.probeDraft(false)
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

            ServerCard {
                id: chosenServerCard
                Layout.fillWidth: true
                title: root.chosenServerName
                serverAddress: root.chosenServerAddress
                status: "Change"
                onAccepted: {
                    InputKeys.focus(chosenServerCard)
                    root.addStep = 1
                    Qt.callLater(root.focusServerStep)
                }
            }

            TextFieldRow {
                id: usernameRow
                Layout.fillWidth: true
                label: "Username"
                text: Session.username
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                onTextEdited: text => Session.username = text
                onAccepted: passwordRow.focusField()
            }

            TextFieldRow {
                id: passwordRow
                Layout.fillWidth: true
                label: "Password"
                text: Session.password
                echoMode: TextInput.Password
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                onTextEdited: text => Session.password = text
                onAccepted: InputKeys.focus(signInButton)
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.scaled(12)

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
                Layout.preferredHeight: quickConnectPanel.implicitHeight + Metrics.scaled(40)
                visible: QuickConnect.active
                baseColor: Theme.accentPanel

                ColumnLayout {
                    id: quickConnectPanel
                    anchors.centerIn: parent
                    width: Math.min(parent.width - Metrics.scaled(56), Metrics.scaled(420))
                    spacing: Metrics.scaled(10)

                    AppText {
                        Layout.alignment: Qt.AlignHCenter
                        text: QuickConnect.code
                        font.pixelSize: Metrics.scaled(34)
                        font.weight: Font.Bold
                        font.letterSpacing: Metrics.scaled(6)
                    }

                    AppText {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignHCenter
                        text: QuickConnect.phase === "server" ? "Signing you in" :
                                                                "Enter this code on a device you are already signed in on"
                        color: Theme.textSecondary
                        font.pixelSize: Metrics.scaled(15)
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }

    LoginProfileDialogs {
        id: profileDialogs
        anchors.fill: parent
        z: 200
        onSignInAgainRequested: {
            root.addMode = true
            root.addStep = 2
        }
        onClosed: Qt.callLater(function () {
            if (!root.addMode)
                InputKeys.focus(profileList)
            if (Session.accountProfiles.length === 0)
                root.openAddAccount()
        })
    }
}
