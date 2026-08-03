pragma ComponentBehavior: Bound

import QtQuick
import "../theme"
import "../primitives"
import "SettingsNavigation.js" as SettingsNavigation

FocusScope {
    id: root

    property var shell
    property var uiTransitionToken: 0
    property int currentIndex: 0
    property var rowsByKey: ({})
    property var allSettingsRows: []
    property var settingsRows: []
    property int detailLevelRevision: 0
    property bool choiceDialogVisible: false
    property var choiceDialogRow: null
    property Item choiceDialogAnchor: null
    readonly property var choiceDialog: choiceDialogLoader.item
    readonly property var mpvFolderDialog: mpvFolderDialogLoader.item
    property bool contentReady: false
    property bool certificateManagerVisible: false
    property bool diagnosticsExportVisible: false
    property string diagnosticsExportPreview: ""
    property bool pendingCustomMpvMode: false

    readonly property var groupOrder: ["Appearance", "Playback", "Subtitles", "Account", "Diagnostics", "Button Remap",
        "About"]

    function rowAvailable(row) {
        return SettingsNavigation.rowAvailable(row, Platform.isTV, Player.hdrPlayback, function (key) {
            return settingsValue({
                                     "key": key,
                                     "defaultValue": ""
                                 })
        })
    }

    function currentDetailLevel() {
        const value = String(Settings.values["settings/detailLevel"] || "Essential")
        return value === "All" ? 2 : value === "More" ? 1 : 0
    }

    function rowDetailLevel(row) {
        return SettingsNavigation.detailLevel(row)
    }

    function appendSourceRows(rows, rowMap, targetRows) {
        const lastGroups = ["", "", ""]
        for (let index = 0; index < rows.length; ++index) {
            const row = rows[index]
            const level = rowDetailLevel(row)
            rowMap[row.key] = row
            const headers = [false, false, false]
            for (let detail = 0; detail < 3; ++detail) {
                if (level > detail || !rowAvailable(row))
                    continue
                headers[detail] = lastGroups[detail] !== row.group
                lastGroups[detail] = row.group
            }
            targetRows.push({
                                "rowKey": row.key,
                                "detailLevel": level,
                                "headerEssential": headers[0],
                                "headerAdvanced": headers[1],
                                "headerExpert": headers[2]
                            })
        }
    }

    function rebuildVisibleRows() {
        const visibleRows = []
        for (let index = 0; index < allSettingsRows.length; ++index) {
            const entry = allSettingsRows[index]
            const row = rowsByKey[entry.rowKey]
            if (!rowAvailable(row))
                continue
            if (entry.detailLevel <= currentDetailLevel())
                visibleRows.push(entry)
        }
        settingsRows = visibleRows
        contentReady = visibleRows.length === 0
    }

    function buildSettingsRowsSource() {
        const schema = Settings.settingsSchema
        const detailKey = "settings/detailLevel"
        const mainRows = []
        const rowMap = {}
        const sourceRows = [];

        // "Subtitle Appearance" lives in SubtitleSettingsPanel, which can show
        // it over live video where the changes are actually visible.
        for (let groupIndex = 0; groupIndex < groupOrder.length; ++groupIndex) {
            const group = groupOrder[groupIndex]
            for (let index = 0; index < schema.length; ++index) {
                const row = schema[index]
                if (row.group === group && row.key !== detailKey && row.group !== "Subtitle Appearance")
                    mainRows.push(row)
            }
        }

        appendSourceRows(mainRows, rowMap, sourceRows)
        rowsByKey = rowMap
        allSettingsRows = sourceRows
        rebuildVisibleRows()
    }

    function refreshSettingsFilter(resetSelection) {
        rebuildVisibleRows()
        if (resetSelection)
            currentIndex = 0
        Qt.callLater(function () {
            if (settingsList.count > 0)
                selectRow(Math.min(currentIndex, settingsList.count - 1), false)
        })
    }

    function currentRow() {
        const delegate = settingsList.currentItem
        return delegate ? delegate.rowData : null
    }

    function rowAtVisibleIndex(index) {
        const delegate = settingsList.itemAtIndex(index)
        return delegate ? delegate.rowData : null
    }

    function selectRow(index, takeFocus) {
        if (settingsList.count <= 0)
            return
        currentIndex = SettingsNavigation.clampIndex(index, settingsList.count)
        settingsList.currentIndex = currentIndex
        settingsList.positionViewAtIndex(currentIndex, ListView.Contain)
        if (takeFocus !== false)
            InputKeys.focus(settingsList)
    }

    function focusEntry() {
        InputKeys.focus(detailSelector)
    }

    function rowControlAt(index) {
        const delegate = settingsList.itemAtIndex(index)
        return delegate ? delegate.controlItem : null
    }

    function settingsValue(row) {
        switch (row.key) {
        case "i18n/locale":
            return I18n.useSystemLocale ? "system" : I18n.currentLocale
        case "theme/accent":
            return Theme.accentIndex
        case "theme/railLabels":
            return Theme.sideRailLabels
        case "theme/reducedMotion":
            return Theme.reducedMotion
        case "theme/renderMode":
            return Theme.normalTextRenderType
        case "theme/antialiasedText":
            return Theme.antialiasedText
        case "theme/technicalMetadata":
            return Theme.technicalMetadataMode
        case "shell/diagnostics":
            return shell ? shell.diagnosticsVisible : false
        case "shell/latencyGuard":
            return InputLatency.enabled
        case "shell/latencyOverlay":
            return InputLatency.overlayEnabled
        case "subtitles/language":
            return Settings.subtitleLanguageIndex
        default:
            const value = Settings.values[row.key]
            return value === undefined ? row.defaultValue : value
        }
    }

    function rowDescription(row) {
        if (row.key === "session/account")
            return Session.serverUrl
        if (row.key === "settings/audioDelayMs" && Platform.usesPerOutputAudioDelay)
            return "User trim for " + Settings.audioDelayTargetLabel + "; automatic compensation is applied separately"
        if (row.key === "subtitles/mode" || row.key === "audio/trackMode") {
            const index = rowCurrentIndex(row)
            const labels = rowOptions(row)
            return index >= 0 && index < labels.length ? labels[index] : row.description
        }
        return row.description || ""
    }

    // Choice labels may carry a "%1" placeholder for the user's preferred
    // language, e.g. "Smart (English when available)".
    function preferredLanguageWord() {
        const labels = Settings.subtitleLanguageOptions
        const index = Settings.subtitleLanguageIndex
        if (index <= 0 || index >= labels.length)
            return "your language"
        return String(labels[index]).split(" ")[0]
    }

    function substitutedLabels(labels) {
        const word = preferredLanguageWord()
        const result = []
        for (let index = 0; index < labels.length; ++index)
            result.push(String(labels[index]).replace("%1", word))
        return result
    }

    function rowValueText(row) {
        if (row.key === "action/switchUser")
            return "Choose"
        if (row.key === "action/logout")
            return "Sign out"
        if (row.key === "action/openSourceNotices" || row.key === "action/exportDiagnostics" || row.key
                === "action/subtitleSettings" || row.key === "action/manageCertificates")
            return "Open"
        if (row.key === "action/clearLatencyStatistics" || row.key === "action/clearLogs")
            return "Clear"
        if (row.key === "session/account")
            return Session.activeProfileLabel.length > 0 ? Session.activeProfileLabel : "Offline"
        if (row.key === "about/version")
            return "v" + Qt.application.version
        if (row.key === "about/locale")
            return I18n.currentLocale
        return row.valueSummary || ""
    }

    function rowOptions(row) {
        if (row.key === "i18n/locale") {
            const result = []
            for (let index = 0; index < I18n.availableLocales.length; ++index)
                result.push(I18n.displayNameFor(I18n.availableLocales[index]))
            return result
        }
        if (row.key === "subtitles/language")
            return Settings.subtitleLanguageOptions
        if (row.key === "subtitles/mode" || row.key === "audio/trackMode")
            return substitutedLabels(row.choiceLabels || [])
        return row.choiceLabels || []
    }

    function rowChoiceValues(row) {
        if (row.key === "i18n/locale")
            return I18n.availableLocales
        if (row.key === "subtitles/language")
            return Settings.subtitleLanguageOptions
        return row.choiceValues || []
    }

    function valueIndex(values, value) {
        for (let index = 0; index < values.length; ++index)
            if (String(values[index]) === String(value))
                return index
        return 0
    }

    function rowCurrentIndex(row) {
        if (row.key === "subtitles/language")
            return Settings.subtitleLanguageIndex
        return valueIndex(rowChoiceValues(row), settingsValue(row))
    }

    function setRowValue(row, value, index) {
        switch (row.key) {
        case "i18n/locale":
            I18n.setLocale(value)
            break
        case "theme/accent":
            Theme.accentIndex = Number(value)
            break
        case "theme/railLabels":
            Theme.sideRailLabels = value
            break
        case "theme/reducedMotion":
            Theme.reducedMotion = value
            break
        case "theme/renderMode":
            Theme.normalTextRenderType = Number(value)
            break
        case "theme/antialiasedText":
            Theme.antialiasedText = value
            break
        case "theme/technicalMetadata":
            Theme.technicalMetadataMode = value
            break
        case "shell/diagnostics":
            if (shell)
                shell.diagnosticsVisible = value
            break
        case "shell/latencyGuard":
            InputLatency.enabled = value
            break
        case "shell/latencyOverlay":
            InputLatency.overlayEnabled = value
            break
        case "subtitles/language":
            Settings.setSubtitleLanguageIndex(index)
            break
        default:
            Settings.setValue(row.key, value)
        }
    }

    function setRowChoice(row, index) {
        const values = rowChoiceValues(row)
        if (index < 0 || index >= values.length)
            return
        if (row.key === "playback/mpvConfigMode" && values[index] === "custom" && !String(
                    Settings.values["playback/mpvConfigDirectory"] || "").length && mpvFolderDialog) {
            pendingCustomMpvMode = true
            mpvFolderDialog.open()
            return
        }
        setRowValue(row, values[index], index)
    }

    function activateRow(row, index) {
        if (!row)
            return
        currentIndex = index
        if (row.type === "action") {
            if (row.key === "action/switchUser" && shell)
                shell.switchUser()
            else if (row.key === "action/logout")
                App.logout()
            else if (row.key === "action/manageCertificates")
                certificateManagerVisible = true
            else if (row.key === "action/clearLatencyStatistics")
                InputLatency.clearStatistics()
            else if (row.key === "action/clearLogs")
                App.clearLogs()
            else if (row.key === "action/exportDiagnostics") {
                diagnosticsExportPreview = App.diagnosticsPreview()
                diagnosticsExportVisible = true
            } else if (row.key === "action/subtitleSettings" && shell)
                shell.pushRoute("subtitleSettings")
            else if (row.key === "action/openSourceNotices" && shell)
                shell.pushRoute("openSourceNotices")
        } else if (row.type === "toggle") {
            setRowValue(row, !Boolean(settingsValue(row)), -1)
        } else if (row.type === "select") {
            settingsList.positionViewAtIndex(index, ListView.Contain)
            Qt.callLater(function () {
                const anchor = rowControlAt(index)
                if (!anchor)
                    return
                choiceDialogRow = row
                choiceDialogAnchor = anchor
                choiceDialogVisible = true
            })
        } else if (row.type === "text") {
            const control = rowControlAt(index)
            if (control && control.activate)
                control.activate()
        }
    }

    function adjustRow(row, direction) {
        if (!row)
            return false
        if (row.type === "select") {
            const control = rowControlAt(settingsList.currentIndex)
            if (control && control.move)
                return control.move(direction)
            return true
        }
        if (row.type === "slider") {
            const from = Number(row.from || 0)
            const to = Number(row.to || 100)
            const next = Math.max(from, Math.min(to, Number(settingsValue(row)) + Number(row.step || 1) * direction))
            setRowValue(row, next, -1)
            return true
        }
        if (row.type === "text") {
            const control = rowControlAt(settingsList.currentIndex)
            return control && control.move ? control.move(direction) : true
        }
        return false
    }

    function closeChoiceDialog() {
        choiceDialogVisible = false
        choiceDialogAnchor = null
        choiceDialogRow = null
        Qt.callLater(function () {
            selectRow(currentIndex, true)
        })
    }

    function makeChoiceSpace(pixels) {
        const maximum = Math.max(0, settingsList.contentHeight + settingsList.bottomMargin - settingsList.height)
        settingsList.contentY = Math.min(maximum, Math.max(0, settingsList.contentY + pixels))
        if (choiceDialog)
            Qt.callLater(choiceDialog.completePresentation)
    }

    function back() {
        if (certificateManagerVisible) {
            certificateManagerVisible = false
            InputKeys.focus(settingsList)
            return true
        }
        if (choiceDialogVisible) {
            closeChoiceDialog()
            return true
        }
        return false
    }

    function routeKey(key, phase, repeat) {
        if (certificateManagerVisible)
            return certificateManagerLoader.item.routeKey(key, phase, repeat)
        if (choiceDialogVisible)
            return choiceDialog.routeKey(key, phase, repeat)
        if (phase === "release" && InputKeys.isDirection(key))
            return true
        if (detailSelector.activeFocus) {
            if (InputKeys.isHorizontal(key))
                return detailSelector.move(key === Qt.Key_Right ? 1 : -1)
            if (key === Qt.Key_Down) {
                selectRow(0, true)
                return true
            }
            if (key === Qt.Key_Up) {
                if (shell)
                    shell.focusNavBar()
                return true
            }
            return InputKeys.isAccept(key)
        }
        const row = currentRow()
        if (InputKeys.isHorizontal(key) && adjustRow(row, key === Qt.Key_Right ? 1 : -1))
            return true
        if (InputKeys.isVertical(key) && !settingsList.activeFocus)
            InputKeys.focus(settingsList)
        if (key === Qt.Key_Up && settingsList.currentIndex <= 0) {
            InputKeys.focus(detailSelector)
            return true
        }
        return settingsList.routeKey(key, phase, repeat)
    }

    function activate() {
        if (certificateManagerVisible) {
            certificateManagerLoader.item.activate()
            return
        }
        if (choiceDialogVisible)
            choiceDialog.activate()
        else if (!detailSelector.activeFocus)
            activateRow(currentRow(), settingsList.currentIndex)
    }

    focus: true
    onActiveFocusChanged: if (activeFocus)
    focusEntry()
    onVisibleChanged: {
        if (visible)
        ensureRowsBuilt()
        if (visible && activeFocus)
        Qt.callLater(focusEntry)
    }

    property bool rowsBuilt: false

    function ensureRowsBuilt() {
        if (rowsBuilt)
            return
        rowsBuilt = true
        buildSettingsRowsSource()
    }

    // Only take focus if the page is actually active: the route host
    // prewarms an invisible instance, which must not steal focus.
    Component.onCompleted: Qt.callLater(function () {
        Settings.loadRemote()
        ensureRowsBuilt()
        if (activeFocus)
            focusEntry()
    })
    Connections {
        target: Settings

        function onSettingChanged(key) {
            if (key === "settings/detailLevel")
                ++root.detailLevelRevision
            root.refreshSettingsFilter(key === "settings/detailLevel")
        }
    }
    Connections {
        target: Player
        function onHdrPlaybackChanged() {
            root.refreshSettingsFilter(true)
        }
    }

    ChoiceStrip {
        id: detailSelector
        width: settingsList.width
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: Metrics.pageMarginPx
        anchors.rightMargin: Metrics.pageMarginPx
        title: "Settings Shown"
        description: "Choose how much control and diagnostic detail is visible"
        options: ["Essential", "Advanced", "Expert"]
        currentIndex: {
            root.detailLevelRevision
            return root.currentDetailLevel()
        }
        onSelected: index => Settings.setValue("settings/detailLevel", ["Essential", "More", "All"][index])
    }

    MenuListView {
        id: settingsList
        readonly property real pageInset: Metrics.pageMarginPx
        width: Math.max(0, parent.width - pageInset * 2)
        anchors.top: detailSelector.bottom
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.topMargin: Metrics.scaled(10)
        anchors.rightMargin: pageInset
        anchors.bottomMargin: pageInset
        bottomMargin: root.choiceDialogVisible && root.choiceDialog ? root.choiceDialog.panelHeight + Metrics.scaled(16) :
                                                                      0
        model: root.settingsRows
        dismissOnBack: false
        dismissOnHorizontal: false
        spacing: Metrics.scaled(10)
        currentIndex: root.currentIndex
        onCurrentIndexChanged: if (currentIndex >= 0) {
            root.currentIndex = currentIndex
            positionViewAtIndex(currentIndex, ListView.Contain)
        }
        onAccepted: index => root.activateRow(root.rowAtVisibleIndex(index), index)
        onEdgeUp: if (root.shell)
        root.shell.focusNavBar()
        delegate: Column {
            required property int index
            required property var modelData
            readonly property string rowKey: modelData.rowKey
            readonly property bool headerEssential: modelData.headerEssential
            readonly property bool headerAdvanced: modelData.headerAdvanced
            readonly property bool headerExpert: modelData.headerExpert
            readonly property var rowData: root.rowsByKey[rowKey]
            readonly property bool showHeader: root.currentDetailLevel() === 0 ? headerEssential :
                                                                                 root.currentDetailLevel() === 1
                                                                                 ? headerAdvanced : headerExpert
            width: settingsList.width
            Component.onCompleted: {
                InputLatency.noteDelegate("settings_row", 1)
                if (index === 0)
                root.contentReady = true
            }
            Component.onDestruction: InputLatency.noteDelegate("settings_row", -1)
            readonly property var controlItem: rowLoader.item
            spacing: Metrics.scaled(10)

            SectionHeader {
                width: parent.width
                visible: parent.showHeader
                title: rowData.group
            }
            Loader {
                id: rowLoader
                width: parent.width
                property var row: rowData
                property int rowIndex: index
                sourceComponent: rowData.type === "toggle" ? toggleComponent : rowData.type === "select"
                                                             ? selectComponent : rowData.type === "slider"
                                                               ? sliderComponent : rowData.type === "text"
                                                                 ? textComponent : settingComponent
                onLoaded: {
                    item.row = row
                    item.rowIndex = rowIndex
                }
            }
        }
    }

    Component {
        id: settingComponent
        SettingRow {
            property var row
            property int rowIndex: -1
            width: settingsList.width
            focus: false
            focusPolicy: Qt.NoFocus
            rowFocus: settingsList.activeFocus && settingsList.currentIndex === rowIndex
            title: row ? row.title : ""
            description: row ? root.rowDescription(row) : ""
            valueText: row ? root.rowValueText(row) : ""
            pointerActivationEnabled: row && row.type === "action"
            onClicked: {
                root.selectRow(rowIndex, true)
                root.activateRow(row, rowIndex)
            }
        }
    }

    Component {
        id: toggleComponent
        ToggleRow {
            property var row
            property int rowIndex: -1
            width: settingsList.width
            focus: false
            focusPolicy: Qt.NoFocus
            rowFocus: settingsList.activeFocus && settingsList.currentIndex === rowIndex
            title: row ? row.title : ""
            description: row ? root.rowDescription(row) : ""
            checked: row ? Boolean(root.settingsValue(row)) : false
            onToggled: checked => {
                root.selectRow(rowIndex, true)
                root.setRowValue(row, checked, -1)
            }
        }
    }

    Component {
        id: selectComponent
        SelectRow {
            property var row
            property int rowIndex: -1
            width: settingsList.width
            metricsWidth: root.width
            focus: false
            focusPolicy: Qt.NoFocus
            rowFocus: settingsList.activeFocus && settingsList.currentIndex === rowIndex
            title: row ? row.title : ""
            description: row ? root.rowDescription(row) : ""
            onOpened: {
                root.selectRow(rowIndex, true)
                root.activateRow(row, rowIndex)
            }
            options: row ? root.rowOptions(row) : []
            currentIndex: row ? root.rowCurrentIndex(row) : 0
            onSelected: (index, value) => root.setRowChoice(row, index)
        }
    }

    Component {
        id: sliderComponent
        SliderRow {
            property var row
            property int rowIndex: -1
            width: settingsList.width
            selected: settingsList.activeFocus && settingsList.currentIndex === rowIndex
            title: row ? row.title : ""
            description: row ? root.rowDescription(row) : ""
            from: row ? Number(row.from) : 0
            to: row ? Number(row.to) : 100
            step: row ? Number(row.step || 1) : 1
            decimals: row ? Number(row.decimals || 0) : 0
            unitText: row ? String(row.unitText || "") : ""
            value: row ? Number(root.settingsValue(row)) : 0
            onValueEdited: value => root.setRowValue(row, value, -1)
            onInteractionStarted: root.selectRow(rowIndex, false)
        }
    }
    Component {
        id: textComponent

        Surface {
            property var row
            property int rowIndex: -1
            width: settingsList.width
            implicitHeight: textContent.implicitHeight + Metrics.scaled(28)
            elevated: true
            focused: settingsList.activeFocus && settingsList.currentIndex === rowIndex

            function activate() {
                if (browseButton.visible && browseButton.activeFocus)
                    root.mpvFolderDialog.open()
                else
                    pathField.focusField()
            }

            function move(direction) {
                if (direction > 0 && browseButton.visible)
                    InputKeys.focus(browseButton)
                else
                    pathField.focusField()
                return true
            }

            Column {
                id: textContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: Metrics.scaled(18)
                spacing: Metrics.scaled(8)

                AppText {
                    width: parent.width
                    text: row ? row.title : ""
                    color: Theme.textPrimary
                    font.pixelSize: Metrics.bodySizePx
                    font.weight: Font.DemiBold
                }

                AppText {
                    width: parent.width
                    text: row ? root.rowDescription(row) : ""
                    color: Theme.textSecondary
                    font.pixelSize: Metrics.metaSizePx
                    wrapMode: Text.Wrap
                }

                Row {
                    width: parent.width
                    spacing: Metrics.scaled(10)

                    TextFieldRow {
                        id: pathField
                        width: browseButton.visible ? Math.max(0, parent.width - browseButton.width - parent.spacing) :
                                                      parent.width
                        label: "Directory"
                        text: row ? String(root.settingsValue(row) || "") : ""
                        placeholderText: "/absolute/path/to/mpv"
                        inputMethodHints: Qt.ImhNoPredictiveText
                        onAccepted: {
                            root.setRowValue(row, text, -1)
                            Qt.callLater(function () {
                                root.selectRow(rowIndex, true)
                            })
                        }
                    }

                    ActionButton {
                        id: browseButton
                        visible: root.mpvFolderDialog !== null
                        width: visible ? Metrics.scaled(132) : 0
                        height: pathField.height
                        text: "Browse"
                        iconName: "folder"
                        onClicked: root.mpvFolderDialog.open()
                    }
                }
            }
        }
    }

    Loader {
        id: mpvFolderDialogLoader
        active: !Platform.isTV
        source: active ? Qt.resolvedUrl("DesktopFolderDialog.qml") : ""
    }

    Connections {
        target: root.mpvFolderDialog

        function onFolderSelected(folder) {
            Settings.setValue("playback/mpvConfigDirectory", folder)
            if (root.pendingCustomMpvMode)
                Settings.setValue("playback/mpvConfigMode", "custom")
            root.pendingCustomMpvMode = false
        }

        function onDismissed() {
            root.pendingCustomMpvMode = false
        }
    }

    Loader {
        id: diagnosticsExportLoader
        anchors.fill: parent
        active: root.diagnosticsExportVisible
        z: 200
        sourceComponent: ConfirmationDialog {
            title: "Save diagnostics report?"
            message: root.diagnosticsExportPreview
            confirmText: "Save"
            onAccepted: {
                App.saveDiagnosticsReport()
                root.diagnosticsExportVisible = false
                InputKeys.focus(settingsList)
            }
            onDismissed: {
                root.diagnosticsExportVisible = false
                InputKeys.focus(settingsList)
            }
        }
    }

    Loader {
        id: certificateManagerLoader
        anchors.fill: parent
        active: root.certificateManagerVisible
        z: 200
        sourceComponent: RememberedCertificatesDialog {
            trustController: TlsTrust
            inputKeys: InputKeys
            onDismissed: {
                root.certificateManagerVisible = false
                InputKeys.focus(settingsList)
            }
        }
    }

    Loader {
        id: choiceDialogLoader
        anchors.fill: parent
        active: root.choiceDialogVisible
        sourceComponent: OptionPickerDialog {
            visible: true
            anchorItem: root.choiceDialogAnchor
            title: root.choiceDialogRow ? root.choiceDialogRow.title : "Choose an option"
            options: root.choiceDialogRow ? root.rowOptions(root.choiceDialogRow) : []
            currentIndex: root.choiceDialogRow ? root.rowCurrentIndex(root.choiceDialogRow) : 0
            onSelected: index => {
                if (root.choiceDialogRow)
                    root.setRowChoice(root.choiceDialogRow, index)
                root.closeChoiceDialog()
            }
            onDismissed: root.closeChoiceDialog()
            onSpaceBelowRequired: pixels => root.makeChoiceSpace(pixels)
        }
    }
}
