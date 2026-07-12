pragma ComponentBehavior: Bound

import QtQuick
import "../theme"
import "../primitives"

FocusScope {
    id: root

    property var shell
    property int currentIndex: 0
    property var settingsRows: []

    readonly property bool gpuNextDiagnosticsAvailable: !NativeWindow.smartTvPlatform
    readonly property var groupOrder: ["General", "Appearance", "Playback", "Subtitles", "Subtitle Appearance",
        "Diagnostics", "Button Remap", "About"]
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
        makeRow("shell/diagnostics", "Diagnostics", "toggle", "Diagnostics Overlay"), makeRow("shell/latencyGuard",
                                                                                              "Diagnostics", "toggle",
                                                                                              "Latency Guard"), makeRow(
            "action/clearLatencyStatistics", "Diagnostics", "action", "Clear Latency Statistics"), makeRow(
            "about/version", "About", "readonly", "Jellyfin Native for webOS", "Qt 6.11 client, native mpv playback"),
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

    function rowVisible(row) {
        return row && row.visible !== false && (row.key !== "settings/toneMappingVisualization"
                                                || gpuNextDiagnosticsAvailable)
    }

    function rebuildSettingsRows() {
        const schema = Settings.settingsSchema
        const rows = []
        for (let groupIndex = 0; groupIndex < groupOrder.length; ++groupIndex) {
            const group = groupOrder[groupIndex]
            for (let index = 0; index < pageRows.length; ++index)
                if (pageRows[index].group === group && rowVisible(pageRows[index]))
                    rows.push(pageRows[index])
            for (let index = 0; index < schema.length; ++index)
                if (schema[index].group === group && rowVisible(schema[index]))
                    rows.push(schema[index])
        }
        settingsRows = rows
        focusRow(Math.min(currentIndex, rows.length - 1))
    }

    function rowAt(index) {
        return index >= 0 && index < settingsRows.length ? settingsRows[index] : null
    }

    function showGroupHeader(index) {
        const row = rowAt(index)
        const previous = rowAt(index - 1)
        return row && (!previous || previous.group !== row.group)
    }

    function focusRow(index) {
        if (settingsRows.length <= 0)
            return
        currentIndex = Math.max(0, Math.min(settingsRows.length - 1, index))
        settingsList.currentIndex = currentIndex
        InputKeys.focus(settingsList)
        settingsList.positionViewAtIndex(currentIndex, ListView.Contain)
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
        case "subtitles/language":
            return Settings.subtitleLanguageIndex
        default:
            const value = Settings.values[row.key]
            return value === undefined ? row.defaultValue : value
        }
    }

    function rowDescription(row) {
        return row.key === "session/server" ? Session.serverUrl : row.description || ""
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
        if (row.key === "session/server")
            return Session.serverUrl.length > 0 ? "Connected" : "Offline"
        if (row.key === "theme/name")
            return "Jellyfin Dark"
        if (row.key === "about/version")
            return "v" + Qt.application.version
        if (row.key === "about/locale")
            return I18n.currentLocale
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
        } else if (row.type === "toggle") {
            setRowValue(row, !Boolean(settingsValue(row)), -1)
        } else if (row.type === "select") {
            adjustRow(row, 1)
        }
    }

    function adjustRow(row, direction) {
        if (!row)
            return false
        if (row.type === "select") {
            const options = rowOptions(row)
            if (options.length > 0)
                setRowChoice(row, (rowCurrentIndex(row) + direction + options.length) % options.length)
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

    function routeKey(key, phase, repeat) {
        const row = rowAt(settingsList.currentIndex)
        if (InputKeys.isHorizontal(key) && adjustRow(row, key === Qt.Key_Right ? 1 : -1))
            return true
        if (key === Qt.Key_Up && settingsList.currentIndex <= 0) {
            if (shell)
                shell.focusNavBar()
            return true
        }
        return settingsList.routeKey(key, phase, repeat)
    }

    function activate() {
        activateRow(rowAt(settingsList.currentIndex), settingsList.currentIndex)
    }

    focus: true
    onActiveFocusChanged: if (activeFocus)
    focusRow(currentIndex)
    Component.onCompleted: Qt.callLater(function () {
        rebuildSettingsRows()
        focusRow(0)
    })

    Connections {
        target: Settings
        function onSettingsValuesChanged() {
            settingsList.forceLayout()
        }
        function onSubtitleSettingsChanged() {
            settingsList.forceLayout()
        }
    }

    MenuListView {
        id: settingsList
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin(root.width)
        model: root.settingsRows
        dismissOnBack: false
        dismissOnHorizontal: false
        spacing: Metrics.scaled(10)
        currentIndex: root.currentIndex
        onCurrentIndexChanged: if (currentIndex >= 0) {
            root.currentIndex = currentIndex
            positionViewAtIndex(currentIndex, ListView.Contain)
        }
        onAccepted: index => root.activateRow(root.rowAt(index), index)
        onEdgeUp: if (root.shell)
        root.shell.focusNavBar()

        delegate: Column {
            required property int index
            required property var modelData
            width: settingsList.width
            spacing: Metrics.scaled(10)

            SectionHeader {
                width: parent.width
                visible: root.showGroupHeader(index)
                title: modelData.group
            }
            Loader {
                width: parent.width
                property var row: modelData
                property int rowIndex: index
                sourceComponent: modelData.type === "toggle" ? toggleComponent : modelData.type === "select"
                                                               ? selectComponent : modelData.type === "slider"
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
            onClicked: root.activateRow(row, rowIndex)
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
            onToggled: checked => root.setRowValue(row, checked, -1)
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
            focus: false
            focusPolicy: Qt.NoFocus
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
        }
    }
}
