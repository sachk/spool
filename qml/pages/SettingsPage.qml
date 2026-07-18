pragma ComponentBehavior: Bound

import QtQuick
import QtQml.Models
import "../theme"
import "../primitives"

FocusScope {
    id: root

    property var shell
    property var uiTransitionToken: 0
    property int currentIndex: 0
    property var rowsByKey: ({})
    property int detailLevelRevision: 0
    property bool subtitleEditor: false
    property bool playbackPreview: false
    property bool choiceDialogVisible: false
    property var choiceDialogRow: null
    property Item choiceDialogAnchor: null
    readonly property var choiceDialog: choiceDialogLoader.item
    property bool contentReady: false

    signal dismissed

    readonly property bool gpuNextDiagnosticsAvailable: !Platform.isTV
    readonly property var groupOrder: ["General", "Appearance", "Playback", "Subtitles", "Diagnostics", "Button Remap",
        "About"]
    readonly property var pageRows: [makeRow("action/switchUser", "General", "action", "Switch User",
                                             "Return to profile selection"), makeRow("action/logout", "General",
                                                                                     "action", "Logout",
                                                                                     "Clear the saved session and return to sign in"),
        makeRow("session/server", "General", "readonly", "Connected Server"), makeRow("theme/name", "General",
                                                                                      "readonly", "Theme",
                                                                                      "Fixed dark TV interface"),
        makeRow("i18n/locale", "General", "select", "Language", "Restart the app to update cached server text"), makeRow(
            "theme/accent", "Appearance", "select", "Accent", "", ["Jellyfin Blue", "Jellyfin Purple", "Blue-Purple"],
            [0, 1, 2]), makeRow("action/uiScaleSetup", "Appearance", "action", "Scale Setup",
                                "Compare Compact, Balanced, and Relaxed layouts"), makeRow("theme/railLabels",
                                                                                           "Appearance", "select",
                                                                                           "Side Rail Labels", "",
                                                                                           ["Never", "On focus",
                                                                                            "Always"], ["Never",
                                                                                                        "On focus",
                                                                                                        "Always"]),
        makeRow("theme/reducedMotion", "Appearance", "toggle", "Reduced Motion"), makeRow("theme/renderMode",
                                                                                          "Appearance", "select",
                                                                                          "Text Render Mode", "", ["Qt",
                                                                                                                   "Curve"], [Text.QtRendering,
                                                                                                                              Text.CurveRendering]),
        makeRow("theme/antialiasedText", "Appearance", "toggle", "Antialiased Text"), makeRow("theme/technicalMetadata",
                                                                                              "Appearance", "select",
                                                                                              "Show Technical Metadata",
                                                                                              "", ["Always",
                                                                                                   "On details only",
                                                                                                   "Hidden"], ["Always",
                                                                                                               "On details only",
                                                                                                               "Hidden"]),
        makeRow("action/subtitleSettings", "Subtitles", "action", "Subtitle settings",
                "Preview and adjust subtitle language and appearance"), makeRow("shell/diagnostics", "Diagnostics",
                                                                                "toggle", "Diagnostics Overlay"),
        makeRow("shell/latencyGuard", "Diagnostics", "toggle", "Latency Logging"), makeRow("shell/latencyOverlay",
                                                                                           "Diagnostics", "toggle",
                                                                                           "Latency Warnings"), makeRow(
            "action/clearLatencyStatistics", "Diagnostics", "action", "Clear Latency Statistics"), makeRow(
            "about/version", "About", "readonly", "Jellyfin Native for " + Platform.deviceName,
            "Qt 6.11 client, native mpv playback"), makeRow("action/openSourceNotices", "About", "action",
                                                            "Open-source notices",
                                                            "Acknowledgements, licenses, and corresponding source"),
        makeRow("about/locale", "About", "readonly", "UI Locale")]

    function makeRow(key, group, type, title, description, labels, values) {
        return {
            key: key,
            source: "page",
            group: group,
            type: type,
            title: title,
            description: description || "",
            choiceLabels: labels || [],
            choiceValues: values || []
        }
    }

    function sliderRow(key, group, title, description, from, to, step, decimals, unit) {
        const row = makeRow(key, group, "slider", title, description)
        row.from = from
        row.to = to
        row.step = step
        row.decimals = decimals
        row.unitText = unit
        return row
    }

    function rowAvailable(row) {
        return row && row.visible !== false && (row.key !== "settings/toneMappingVisualization"
                                                || gpuNextDiagnosticsAvailable) && (row.key
                                                                                    !== "playback/maxStreamingBitrateMbps"
                                                                                    || Settings.values["playback/manualStreamingBitrate"]
                                                                                    === true) && (row.group
                                                                                                  !== "Button Remap"
                                                                                                  || Platform.isTV)
    }

    function currentDetailLevel() {
        const value = String(Settings.values["settings/detailLevel"] || "Essential")
        return value === "All" ? 2 : value === "More" ? 1 : 0
    }

    function rowDetailLevel(row) {
        if (!row)
            return 2
        if (row.group === "Diagnostics" || row.group === "Button Remap")
            return 2
        if (row.key === "theme/renderMode" || row.key === "theme/antialiasedText" || row.key
                === "theme/technicalMetadata" || row.key === "settings/audioOutputMode" || row.key
                === "playback/forwardCacheSizeMiB" || row.key === "playback/showVolumeSlider" || row.key
                === "settings/audioDelayMs")
            return 1
        return 0
    }

    function expandedSchemaRow(row) {
        if (!row || row.key !== "subtitles/font" || !Platform.hasSystemFonts)
            return row
        const expanded = Object.assign({}, row)
        expanded.choiceLabels = []
        expanded.choiceValues = []
        for (let index = 0; index < row.choiceLabels.length; ++index) {
            expanded.choiceLabels.push(row.choiceLabels[index])
            expanded.choiceValues.push(row.choiceValues[index])
        }
        const families = Settings.systemSubtitleFonts
        for (let index = 0; index < families.length; ++index) {
            expanded.choiceLabels.push("System — " + families[index])
            expanded.choiceValues.push("system:" + families[index])
        }
        return expanded
    }

    function previewFontFamily() {
        const value = String(Settings.values["subtitles/font"] || "")
        if (value.indexOf("system:") === 0)
            return value.slice(7)
        if (value === "serif")
            return String(previewSerif.item ? previewSerif.item.name : Typography.sans)
        if (value === "typewriter")
            return "Courier New"
        if (value === "print")
            return "Georgia"
        if (value === "console")
            return "Consolas"
        if (value === "cursive")
            return "Lucida Handwriting"
        if (value === "casual")
            return "Segoe Print"
        if (value === "smallcaps")
            return "Copperplate Gothic"
        return String(Typography.regularFamily || "Inter")
    }

    function previewTextSize() {
        const value = String(Settings.values["subtitles/textSize"] || "")
        const factors = {
            "smaller": 0.8,
            "small": 0.91,
            "large": 1.16,
            "extralarge": 1.53
        }
        return Metrics.scaled(28) * Number(factors[value] || 1) * Number(Settings.values["subtitles/scalePercent"]
                                                                         || 100) / 100
    }

    function previewBackgroundColor() {
        const value = String(Settings.values["subtitles/textBackground"] || "transparent")
        if (value === "opaque")
            return "#ff000000"
        if (value === "translucent")
            return "#a0000000"
        return "transparent"
    }

    function appendSourceRows(rows, context, rowMap) {
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
            settingsRowsSourceModel.append({
                                               "rowKey": row.key,
                                               "rowContext": context,
                                               "detailLevel": level,
                                               "headerEssential": headers[0],
                                               "headerAdvanced": headers[1],
                                               "headerExpert": headers[2]
                                           })
        }
    }

    function buildSettingsRowsSource() {
        const schema = Settings.settingsSchema
        const detailKey = "settings/detailLevel"
        const scaleKey = "appearance/uiScalePercent"
        const mainRows = []
        const subtitleRows = []
        const rowMap = {}

        for (let index = 0; index < schema.length; ++index) {
            if (schema[index].key !== scaleKey)
                continue
            const scaleRow = Object.assign({}, expandedSchemaRow(schema[index]))
            scaleRow.group = "General"
            mainRows.push(scaleRow)
        }
        for (let groupIndex = 0; groupIndex < groupOrder.length; ++groupIndex) {
            const group = groupOrder[groupIndex]
            for (let index = 0; index < pageRows.length; ++index)
                if (pageRows[index].group === group)
                    mainRows.push(pageRows[index])
            for (let index = 0; index < schema.length; ++index) {
                const row = schema[index]
                if (row.group === group && row.key !== detailKey && row.key !== scaleKey && row.group !== "Subtitles"
                        && row.group !== "Subtitle Appearance")
                    mainRows.push(expandedSchemaRow(row))
            }
        }
        for (let index = 0; index < schema.length; ++index) {
            const row = schema[index]
            if (row.group === "Subtitles" || row.group === "Subtitle Appearance")
                subtitleRows.push(expandedSchemaRow(row))
        }

        settingsRowsSourceModel.clear()
        appendSourceRows(mainRows, "main", rowMap)
        appendSourceRows(subtitleRows, "subtitle", rowMap)
        rowsByKey = rowMap
        settingsRowsModel.invalidate()
    }

    function refreshSettingsFilter(resetSelection) {
        settingsRowsModel.invalidate()
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
        currentIndex = Math.max(0, Math.min(settingsList.count - 1, index))
        settingsList.currentIndex = currentIndex
        settingsList.positionViewAtIndex(currentIndex, ListView.Contain)
        if (takeFocus !== false)
            InputKeys.focus(settingsList)
    }

    function focusEntry() {
        if (!subtitleEditor) {
            InputKeys.focus(detailSelector)
            return
        }
        selectRow(currentIndex, true)
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
        if (row.key === "session/server")
            return Session.serverUrl
        if (row.key === "settings/audioDelayMs" && Platform.usesPerOutputAudioDelay)
            return "User trim for " + Settings.audioDelayTargetLabel + "; automatic compensation is applied separately"
        return row.description || ""
    }

    function rowValueText(row) {
        if (row.key === "action/switchUser")
            return "Choose"
        if (row.key === "action/logout")
            return "Sign out"
        if (row.key === "action/uiScaleSetup")
            return "Open"
        if (row.key === "action/clearLatencyStatistics")
            return "Clear"
        if (row.key === "action/openSourceNotices")
            return "Open"
        if (row.key === "session/server")
            return Session.serverUrl.length > 0 ? "Connected" : "Offline"
        if (row.key === "theme/name")
            return "Jellyfin Dark"
        if (row.key === "about/version")
            return "v" + Qt.application.version
        if (row.key === "about/locale")
            return I18n.currentLocale
        if (row.key === "action/subtitleSettings")
            return "Open"
        return ""
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
            if (values[index] === value)
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
            Theme.accentIndex = value
            break
        case "theme/railLabels":
            Theme.sideRailLabels = value
            break
        case "theme/reducedMotion":
            Theme.reducedMotion = value
            break
        case "theme/renderMode":
            Theme.normalTextRenderType = value
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
        if (index >= 0 && index < values.length)
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
            else if (row.key === "action/clearLatencyStatistics")
                InputLatency.clearStatistics()
            else if (row.key === "action/uiScaleSetup" && shell)
                shell.pushRoute("scaleSetup", {
                                    "returnRoute": "settings"
                                })
            else if (row.key === "action/subtitleSettings" && shell)
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
        } else if (row.type === "slider") {
            const control = rowControlAt(index)
            if (control && control.focusSlider)
                control.focusSlider()
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
        if (choiceDialogVisible) {
            closeChoiceDialog()
            return true
        }
        if (subtitleEditor && playbackPreview) {
            dismissed()
            return true
        }
        return false
    }

    function routeKey(key, phase, repeat) {
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
            if (subtitleEditor) {
                if (shell)
                    shell.focusNavBar()
            } else {
                InputKeys.focus(detailSelector)
            }
            return true
        }
        return settingsList.routeKey(key, phase, repeat)
    }

    function activate() {
        if (choiceDialogVisible)
            choiceDialog.activate()
        else if (!detailSelector.activeFocus)
            activateRow(currentRow(), settingsList.currentIndex)
    }

    focus: true
    onActiveFocusChanged: if (activeFocus)
    focusEntry()
    onVisibleChanged: if (visible && activeFocus)
    Qt.callLater(focusEntry)
    // Only take focus if the page is actually active: the route host
    // prewarms an invisible instance, which must not steal focus.
    Component.onCompleted: Qt.callLater(function () {
        buildSettingsRowsSource()
        if (activeFocus)
            focusEntry()
    })
    onSubtitleEditorChanged: refreshSettingsFilter(true)

    Connections {
        target: Settings

        function onSettingChanged(key) {
            if (key === "settings/detailLevel")
                ++root.detailLevelRevision
            if (key === "playback/manualStreamingBitrate" || key === "settings/detailLevel")
                root.refreshSettingsFilter(key === "settings/detailLevel")
        }
    }
    Loader {
        id: previewSerif
        active: root.subtitleEditor
        sourceComponent: FontLoader {
            source: Qt.resolvedUrl("../fonts/SourceSerif4-Regular.ttf")
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: root.subtitleEditor && !root.playbackPreview
        color: "black"
    }

    Rectangle {
        anchors.fill: settingsList
        anchors.margins: -Metrics.scaled(12)
        visible: root.subtitleEditor
        radius: Theme.radiusLarge
        color: "#d9000000"
        border.width: Theme.hoverBorderWidth
        border.color: Theme.border
    }

    Rectangle {
        id: subtitlePreviewBackground
        visible: root.subtitleEditor && !root.playbackPreview && Settings.values["subtitles/verticalPositionPercent"]
        !== undefined
        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.round((parent.height - height) * Number(Settings.values["subtitles/verticalPositionPercent"] || 0)
                      / 100)
        width: subtitlePreviewText.implicitWidth + Metrics.scaled(24)
        height: subtitlePreviewText.implicitHeight + Metrics.scaled(12)
        radius: Theme.radiusSmall
        color: root.previewBackgroundColor()
    }

    AppText {
        id: subtitlePreviewText
        visible: root.subtitleEditor && !root.playbackPreview
        anchors.centerIn: subtitlePreviewBackground
        text: "This is how your subtitles will look."
        font.family: root.previewFontFamily()
        font.pixelSize: root.previewTextSize()
        font.weight: Settings.values["subtitles/textWeight"] === "bold" ? Font.Bold : Font.Normal
        style: Settings.values["subtitles/dropShadow"] === "none" ? Text.Normal : Text.Outline
        styleColor: "#cc000000"
        color: Settings.values["subtitles/textColor"] || "white"
        horizontalAlignment: Text.AlignHCenter
    }

    ListModel {
        id: settingsRowsSourceModel
    }

    SortFilterProxyModel {
        id: settingsRowsModel
        model: settingsRowsSourceModel
        filters: FunctionFilter {
            function filter(data: RowData): bool {
                const row = root.rowsByKey[data.rowKey]
                if (!root.rowAvailable(row))
                    return false
                if (root.subtitleEditor)
                    return data.rowContext === "subtitle"
                return data.rowContext === "main" && data.detailLevel <= root.currentDetailLevel()
            }
        }
    }
    component RowData: QtObject {
        property string rowKey
        property string rowContext
        property int detailLevel
    }

    ChoiceStrip {
        id: detailSelector
        visible: !root.subtitleEditor
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
        width: root.subtitleEditor ? Math.min(parent.width - pageInset * 2, Metrics.scaled(760)) : Math.max(0,
                                                                                                            parent.width
                                                                                                            - pageInset
                                                                                                            * 2)
        anchors.top: root.subtitleEditor ? parent.top : detailSelector.bottom
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.topMargin: root.subtitleEditor ? pageInset : Metrics.scaled(10)
        anchors.rightMargin: pageInset
        anchors.bottomMargin: pageInset
        bottomMargin: root.choiceDialogVisible && root.choiceDialog ? root.choiceDialog.panelHeight + Metrics.scaled(16) :
                                                                      0
        model: settingsRowsModel
        dismissOnBack: false
        dismissOnHorizontal: false
        spacing: Metrics.scaled(10)
        currentIndex: root.currentIndex
        onCurrentIndexChanged: if (currentIndex >= 0) {
            root.currentIndex = currentIndex
            positionViewAtIndex(currentIndex, ListView.Contain)
        }
        onAccepted: index => root.activateRow(root.rowAtVisibleIndex(index), index)
        onEdgeUp: if (root.shell && !root.subtitleEditor)
        root.shell.focusNavBar()
        delegate: Column {
            required property int index
            required property string rowKey
            required property bool headerEssential
            required property bool headerAdvanced
            required property bool headerExpert
            readonly property var rowData: root.rowsByKey[rowKey]
            readonly property bool showHeader: root.subtitleEditor ? (index === 0 || headerExpert) : root.currentDetailLevel(
                                                                         ) === 0 ? headerEssential :
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
                                                               ? sliderComponent : settingComponent
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
            metricsWidth: root.width
            selected: settingsList.activeFocus && settingsList.currentIndex === rowIndex
            title: row ? row.title : ""
            description: row ? root.rowDescription(row) : ""
            from: row ? Number(row.from) : 0
            to: row ? Number(row.to) : 100
            step: row ? Number(row.step || 1) : 1
            decimals: row ? Number(row.decimals || 0) : 0
            unitText: row ? String(row.unitText || "") : ""
            valueBoxWidth: row && row.valueBoxWidth > 0 ? row.valueBoxWidth : 86
            sliderPreferredWidth: row && row.sliderPreferredWidth > 0 ? row.sliderPreferredWidth : 300
            value: row ? Number(root.settingsValue(row)) : 0
            onValueEdited: value => root.setRowValue(row, value, -1)
            onInteractionStarted: root.selectRow(rowIndex, false)
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
