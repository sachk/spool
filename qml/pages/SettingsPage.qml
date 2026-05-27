import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    property var shell
    property int currentIndex: 0
    property int categoryIndex: 0
    property var settingsRows: []
    readonly property var categories: [
        { label: "General" },
        { label: "Appearance" },
        { label: "Playback" },
        { label: "Diagnostics" },
        { label: "Input" },
        { label: "Button Remap" },
        { label: "SyncPlay" },
        { label: "About" }
    ]

    function categoryTarget(index) {
        const targets = [themeRow, posterSizeRow, nightModeRow, diagnosticsRow,
                         shortcutsRow, redButtonRow, syncPlayStatusRow, aboutVersionRow]
        const row = index >= 0 && index < targets.length ? targets[index] : themeRow
        return row ? Math.max(0, row.settingIndex) : 0
    }

    function rebuildSettingsRows() {
        const rows = [
            themeRow, languageRow, accentRow, uiScaleRow, logoutRow, posterSizeRow,
            gridColumnsRow, railLabelsRow, reducedMotionRow,
            renderModeRow, antialiasedRow, metadataRow,
            nightModeRow, audioDelayRow, audioOutputRow, bitrateRow,
            remuxRow, diagnosticsRow, shortcutsRow,
            redButtonRow, greenButtonRow, yellowButtonRow, blueButtonRow,
            syncPlayStatusRow
        ]
        for (let i = 0; i < groupRepeater.count; ++i) {
            const row = groupRepeater.itemAt(i)
            if (row)
                rows.push(row)
        }
        rows.push(syncPlayCreateRow, aboutVersionRow, aboutServerRow, aboutLocaleRow)
        settingsRows = rows
        for (let i = 0; i < settingsRows.length; ++i)
            settingsRows[i].settingIndex = i
        currentIndex = Math.max(0, Math.min(currentIndex, settingsRows.length - 1))
        syncCategoryForRow(currentIndex)
    }

    function buttonActionOptions() {
        if (!appController) return ["No action"]
        const result = []
        const actions = appController.availableButtonActions
        for (let i = 0; i < actions.length; ++i) {
            result.push(appController.buttonActionLabel(actions[i]))
        }
        return result
    }

    function buttonActionIndex(currentAction) {
        if (!appController) return 0
        const actions = appController.availableButtonActions
        for (let i = 0; i < actions.length; ++i) {
            if (actions[i] === currentAction) return i
        }
        return 0
    }

    function actionFromIndex(i) {
        if (!appController) return "none"
        const actions = appController.availableButtonActions
        return (i >= 0 && i < actions.length) ? actions[i] : "none"
    }
    focus: true

    function currentRow() {
        return currentIndex >= 0 && currentIndex < settingsRows.length ? settingsRows[currentIndex] : null
    }

    function focusRow(index) {
        if (settingsRows.length <= 0)
            return
        currentIndex = Math.max(0, Math.min(settingsRows.length - 1, index))
        const row = currentRow()
        if (row)
            row.forceActiveFocus()
        syncCategoryForRow(currentIndex)
        ensureCurrentVisible()
    }

    function markFocused(index) {
        currentIndex = Math.max(0, Math.min(settingsRows.length - 1, index))
        syncCategoryForRow(currentIndex)
        ensureCurrentVisible()
    }

    function focusCategory(index) {
        categoryIndex = Math.max(0, Math.min(categories.length - 1, index))
        categoryList.forceActiveFocus()
    }

    function activateCategory(index) {
        categoryIndex = Math.max(0, Math.min(categories.length - 1, index))
        focusRow(categoryTarget(categoryIndex))
    }

    function syncCategoryForRow(rowIndex) {
        let nextCategory = 0
        for (let i = 0; i < categories.length; ++i) {
            if (rowIndex >= categoryTarget(i))
                nextCategory = i
        }
        categoryIndex = nextCategory
    }

    Connections {
        target: appController ? appController.syncPlay : null
        function onGroupsChanged() { Qt.callLater(root.rebuildSettingsRows) }
    }

    function ensureCurrentVisible() {
        const row = currentRow()
        if (!row)
            return
        const margin = 12
        const top = Math.max(0, row.y - margin)
        const bottom = row.y + row.height + margin
        const maxY = Math.max(0, settingsFlick.contentHeight - settingsFlick.height)
        if (top < settingsFlick.contentY)
            settingsFlick.contentY = Math.max(0, top)
        else if (bottom > settingsFlick.contentY + settingsFlick.height)
            settingsFlick.contentY = Math.min(maxY, bottom - settingsFlick.height)
    }

    function handleNavigationKey(key) {
        if (categoryList.activeFocus) {
            if (key === Qt.Key_Left) {
                shell.focusRail()
                return true
            }
            if (key === Qt.Key_Right || key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select) {
                activateCategory(categoryIndex)
                return true
            }
            if (key === Qt.Key_Up) {
                focusCategory(categoryIndex - 1)
                return true
            }
            if (key === Qt.Key_Down) {
                focusCategory(categoryIndex + 1)
                return true
            }
            return false
        }
        const row = currentRow()
        if (row && row.handleNavigationKey && row.handleNavigationKey(key))
            return true
        if (key === Qt.Key_Left) {
            focusCategory(categoryIndex)
            return true
        }
        if (key === Qt.Key_Up) {
            focusRow(currentIndex - 1)
            return true
        }
        if (key === Qt.Key_Down) {
            focusRow(currentIndex + 1)
            return true
        }
        return false
    }

    Component.onCompleted: focusRow(0)
    onActiveFocusChanged: if (activeFocus) focusRow(currentIndex)

    RowLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin(width)
        spacing: 18

        ListView {
            id: categoryList
            Layout.preferredWidth: 260
            Layout.fillHeight: true
            model: root.categories
            spacing: 8
            focus: false
            currentIndex: root.categoryIndex
            keyNavigationEnabled: false

            delegate: Surface {
                required property int index
                required property var modelData
                width: 250
                height: 38
                focused: categoryList.activeFocus && root.categoryIndex === index
                baseColor: root.categoryIndex === index ? Theme.accentPanel : Theme.bgPanel

                AppText {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    text: modelData.label
                    color: root.categoryIndex === index ? Theme.textPrimary : Theme.textSecondary
                    font.pixelSize: Metrics.metaPx(root.width)
                    font.weight: root.categoryIndex === index ? Font.DemiBold : Font.Medium
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                TapHandler {
                    onTapped: root.activateCategory(index)
                }
            }
        }

        Flickable {
            id: settingsFlick
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: settings.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Behavior on contentY { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }

            ColumnLayout {
                id: settings
                width: parent.width
                spacing: 10

                SectionHeader { Layout.fillWidth: true; title: "Settings" }
                SectionHeader { Layout.fillWidth: true; title: "General" }

                SettingRow {
                    id: themeRow
                    Layout.fillWidth: true
                    settingIndex: 0
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Theme"
                    description: "Fixed dark TV interface"
                    valueText: "Jellyfin Dark"
                    pointerActivationEnabled: false
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SelectRow {
                    id: languageRow
                    Layout.fillWidth: true
                    settingIndex: 1
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: qsTrId("settings.language.title")
                    description: "Restart the app for full effect on cached strings"
                    options: {
                        if (!i18n) return ["System default"]
                        const result = []
                        const list = i18n.availableLocales
                        for (let i = 0; i < list.length; ++i)
                            result.push(i18n.displayNameFor(list[i]))
                        return result
                    }
                    currentIndex: {
                        if (!i18n) return 0
                        const list = i18n.availableLocales
                        for (let i = 0; i < list.length; ++i) {
                            if ((list[i] === "system" && i18n.useSystemLocale)
                                || list[i] === i18n.currentLocale)
                                return i
                        }
                        return 0
                    }
                    onSelected: (i, v) => {
                        if (!i18n) return
                        const list = i18n.availableLocales
                        if (i >= 0 && i < list.length)
                            i18n.setLocale(list[i])
                    }
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SelectRow {
                    id: accentRow
                    Layout.fillWidth: true
                    settingIndex: 1
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Accent"
                    options: ["Jellyfin Blue", "Jellyfin Purple", "Blue-Purple"]
                    currentIndex: Theme.accentIndex
                    onSelected: (i, v) => Theme.accentIndex = i
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SliderRow {
                    id: uiScaleRow
                    Layout.fillWidth: true
                    settingIndex: 2
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "UI Scale"
                    description: "Runtime type and spacing scale"
                    value: Metrics.userUiScale
                    onValueEdited: Metrics.userUiScale = value
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SettingRow {
                    id: logoutRow
                    Layout.fillWidth: true
                    settingIndex: 3
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Logout"
                    description: "Clear the saved session and return to sign in"
                    valueText: "Sign out"
                    onClicked: appController.logout()
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                    function handleNavigationKey(key) {
                        if (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select || key === Qt.Key_Space) {
                            appController.logout()
                            return true
                        }
                        return false
                    }
                    Keys.onReleased: (event) => {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Select || event.key === Qt.Key_Space) {
                            appController.logout()
                            event.accepted = true
                        }
                    }
                }
                SectionHeader { Layout.fillWidth: true; title: "Appearance" }
                SelectRow {
                    id: posterSizeRow
                    Layout.fillWidth: true
                    settingIndex: 4
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Poster Size"
                    options: ["Compact", "Normal", "Large"]
                    currentIndex: Metrics.userPosterSizeBias + 1
                    onSelected: (i, v) => Metrics.userPosterSizeBias = i - 1
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SelectRow {
                    id: gridColumnsRow
                    Layout.fillWidth: true
                    settingIndex: 5
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Grid Columns"
                    options: ["Auto", "4", "5", "6", "7", "8", "9"]
                    onSelected: (i, v) => Metrics.userColumnOverride = i === 0 ? 0 : Number(v)
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SelectRow {
                    id: railLabelsRow
                    Layout.fillWidth: true
                    settingIndex: 6
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Side Rail Labels"
                    options: ["Never", "On focus", "Always"]
                    currentIndex: 1
                    onSelected: (i, v) => Theme.sideRailLabels = v
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                ToggleRow {
                    id: reducedMotionRow
                    Layout.fillWidth: true
                    settingIndex: 7
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Reduced Motion"
                    checked: Theme.reducedMotion
                    onToggled: Theme.reducedMotion = checked
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SelectRow {
                    id: renderModeRow
                    Layout.fillWidth: true
                    settingIndex: 8
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Text Render Mode"
                    options: ["Auto", "QtRendering", "CurveRendering"]
                    currentIndex: 1
                    onSelected: (i, v) => Theme.normalTextRenderType = v === "CurveRendering" ? Text.CurveRendering : Text.QtRendering
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                ToggleRow {
                    id: antialiasedRow
                    Layout.fillWidth: true
                    settingIndex: 9
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Antialiased Text"
                    checked: Theme.antialiasedText
                    onToggled: Theme.antialiasedText = checked
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SelectRow {
                    id: metadataRow
                    Layout.fillWidth: true
                    settingIndex: 10
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Show Technical Metadata"
                    options: ["Always", "On details only", "Hidden"]
                    onSelected: (i, v) => Theme.technicalMetadataMode = v
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SectionHeader { Layout.fillWidth: true; title: "Playback" }
                ToggleRow {
                    id: nightModeRow
                    Layout.fillWidth: true
                    settingIndex: 11
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Night mode"
                    description: "Dialogue lift and late-night dynamic range"
                    checked: appController.nightModeEnabled
                    onToggled: appController.setNightModeEnabled(checked)
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                AudioDelayRow {
                    id: audioDelayRow
                    Layout.fillWidth: true
                    settingIndex: 12
                    title: "A/V sync"
                    description: "Audio delay in milliseconds"
                    valueMs: appController.audioDelayMs
                    onValueEdited: (value) => appController.setAudioDelayMs(value)
                    onRowFocusChanged: if (rowFocus) root.markFocused(settingIndex)
                }
                SelectRow {
                    id: audioOutputRow
                    Layout.fillWidth: true
                    settingIndex: 13
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Audio output"
                    description: "Takes effect on the next playback start"
                    options: ["ALSA", "Starfish (AAC)", "Starfish (PCM)"]
                    currentIndex: appController.audioOutputMode === "starfish-pcm" ? 2
                                  : (appController.audioOutputMode === "starfish" ? 1 : 0)
                    onSelected: (i, v) => appController.setAudioOutputMode(i === 2 ? "starfish-pcm"
                                                                          : (i === 1 ? "starfish" : "alsa"))
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SelectRow {
                    id: bitrateRow
                    Layout.fillWidth: true
                    settingIndex: 14
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Maximum remote bitrate"
                    options: ["Auto", "20 Mbps", "40 Mbps", "80 Mbps", "Unlimited"]
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                ToggleRow {
                    id: remuxRow
                    Layout.fillWidth: true
                    settingIndex: 15
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Prefer remux over transcode"
                    checked: true
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SectionHeader { Layout.fillWidth: true; title: "Diagnostics" }
                ToggleRow {
                    id: diagnosticsRow
                    Layout.fillWidth: true
                    settingIndex: 16
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Diagnostics overlay"
                    checked: shell.diagnosticsVisible
                    onToggled: shell.diagnosticsVisible = checked
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SectionHeader { Layout.fillWidth: true; title: "Input" }
                ToggleRow {
                    id: shortcutsRow
                    Layout.fillWidth: true
                    settingIndex: 17
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Keyboard shortcuts enabled"
                    checked: true
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SectionHeader { Layout.fillWidth: true; title: "Button Remap" }
                SelectRow {
                    id: redButtonRow
                    Layout.fillWidth: true
                    settingIndex: 18
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Red button"
                    description: "TV remote color button"
                    options: root.buttonActionOptions()
                    currentIndex: root.buttonActionIndex(appController ? appController.redButtonAction : "none")
                    onSelected: (i, v) => appController.setRedButtonAction(root.actionFromIndex(i))
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SelectRow {
                    id: greenButtonRow
                    Layout.fillWidth: true
                    settingIndex: 19
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Green button"
                    description: "Defaults to skip back 10 s + enable subs"
                    options: root.buttonActionOptions()
                    currentIndex: root.buttonActionIndex(appController ? appController.greenButtonAction : "none")
                    onSelected: (i, v) => appController.setGreenButtonAction(root.actionFromIndex(i))
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SelectRow {
                    id: yellowButtonRow
                    Layout.fillWidth: true
                    settingIndex: 20
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Yellow button"
                    options: root.buttonActionOptions()
                    currentIndex: root.buttonActionIndex(appController ? appController.yellowButtonAction : "none")
                    onSelected: (i, v) => appController.setYellowButtonAction(root.actionFromIndex(i))
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SelectRow {
                    id: blueButtonRow
                    Layout.fillWidth: true
                    settingIndex: 21
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Blue button"
                    options: root.buttonActionOptions()
                    currentIndex: root.buttonActionIndex(appController ? appController.blueButtonAction : "none")
                    onSelected: (i, v) => appController.setBlueButtonAction(root.actionFromIndex(i))
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }

                SectionHeader { Layout.fillWidth: true; title: "SyncPlay" }
                SettingRow {
                    id: syncPlayStatusRow
                    Layout.fillWidth: true
                    settingIndex: 22
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Status"
                    description: appController && appController.syncPlay && appController.syncPlay.enabled
                                 ? "Synced with " + appController.syncPlay.currentGroupName
                                 : "Not in a group"
                    valueText: appController && appController.syncPlay && appController.syncPlay.enabled ? "Leave" : "Refresh"
                    onClicked: {
                        if (!appController || !appController.syncPlay) return
                        if (appController.syncPlay.enabled) appController.syncPlay.leaveGroup()
                        else appController.syncPlay.refreshGroups()
                    }
                    function handleNavigationKey(key) {
                        if (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select || key === Qt.Key_Space) {
                            if (!appController || !appController.syncPlay) return true
                            if (appController.syncPlay.enabled) appController.syncPlay.leaveGroup()
                            else appController.syncPlay.refreshGroups()
                            return true
                        }
                        return false
                    }
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                Repeater {
                    id: groupRepeater
                    model: appController && appController.syncPlay ? appController.syncPlay.groups : []
                    delegate: Surface {
                        required property int index
                        required property var modelData
                        property int settingIndex: -1
                        readonly property string groupId: modelData ? modelData.GroupId || "" : ""
                        readonly property string groupName: modelData ? modelData.GroupName || "Group" : "Group"
                        readonly property int participantCount: modelData && modelData.Participants ? modelData.Participants.length : 0
                        property bool rowFocus: root.currentIndex === settingIndex || activeFocus
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56
                        focus: true
                        focused: rowFocus
                        baseColor: Theme.bgPanel

                        function handleNavigationKey(key) {
                            if (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select || key === Qt.Key_Space) {
                                if (appController && appController.syncPlay)
                                    appController.syncPlay.joinGroup(groupId)
                                return true
                            }
                            return false
                        }

                        Component.onCompleted: Qt.callLater(root.rebuildSettingsRows)
                        Component.onDestruction: Qt.callLater(root.rebuildSettingsRows)
                        onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            spacing: 12

                            AppText {
                                text: groupName
                                Layout.fillWidth: true
                                font.weight: Font.Medium
                            }
                            AppText {
                                text: participantCount + " member" + (participantCount === 1 ? "" : "s")
                                color: Theme.textMuted
                                font.pixelSize: 13
                            }
                            ActionButton {
                                text: "Join"
                                onClicked: if (appController && appController.syncPlay) appController.syncPlay.joinGroup(groupId)
                            }
                        }
                    }
                }
                SettingRow {
                    id: syncPlayCreateRow
                    Layout.fillWidth: true
                    settingIndex: 23
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Create group"
                    description: "Start a new SyncPlay session"
                    valueText: "Create"
                    onClicked: if (appController && appController.syncPlay) appController.syncPlay.createGroup("Group")
                    function handleNavigationKey(key) {
                        if (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select || key === Qt.Key_Space) {
                            if (appController && appController.syncPlay) appController.syncPlay.createGroup("Group")
                            return true
                        }
                        return false
                    }
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }

                SectionHeader { Layout.fillWidth: true; title: "About" }
                SettingRow {
                    id: aboutVersionRow
                    Layout.fillWidth: true
                    settingIndex: 24
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Jellyfin Native for webOS"
                    description: "Qt 6.11 client, native mpv playback"
                    valueText: "v" + Qt.application.version
                    pointerActivationEnabled: false
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SettingRow {
                    id: aboutServerRow
                    Layout.fillWidth: true
                    settingIndex: 25
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Connected server"
                    description: appController ? appController.serverUrl : ""
                    valueText: appController && appController.serverUrl.length > 0 ? "Connected" : "Offline"
                    pointerActivationEnabled: false
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SettingRow {
                    id: aboutLocaleRow
                    Layout.fillWidth: true
                    settingIndex: 26
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "UI locale"
                    description: i18n ? "Active translation tag" : ""
                    valueText: i18n ? i18n.currentLocale : "en-US"
                    pointerActivationEnabled: false
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }

                Component.onCompleted: {
                    root.rebuildSettingsRows()
                    root.focusRow(root.currentIndex)
                    // Best-effort refresh on page open so users see existing
                    // groups without needing to hit "Refresh".
                    if (appController && appController.syncPlay)
                        appController.syncPlay.refreshGroups()
                }
            }
        }
    }
}
