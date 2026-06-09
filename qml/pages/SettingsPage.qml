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
    readonly property bool smartTvPlatform: nativeWindow ? nativeWindow.smartTvPlatform : true
    readonly property bool gpuNextDiagnosticsAvailable: !smartTvPlatform
    readonly property var categories: [
        { label: "General" },
        { label: "Appearance" },
        { label: "Playback" },
        { label: "Subtitles" },
        { label: "Diagnostics" },
        { label: "Input" },
        { label: "About" }
    ]
    readonly property var subtitleModeValues: ["Default", "Smart", "OnlyForced", "Always", "None"]
    readonly property var subtitleModeOptions: ["Default", "Smart", "Only forced", "Always play", "None"]
    readonly property var subtitleBurnInValues: ["", "onlyimageformats", "allcomplexformats", "all"]
    readonly property var subtitleBurnInOptions: ["Auto", "Only image formats", "All complex formats", "All"]
    readonly property var subtitleStylingValues: ["Auto", "Custom", "Native"]
    readonly property var subtitleStylingOptions: ["Auto", "Custom", "Native"]
    readonly property var subtitleTextSizeValues: ["smaller", "small", "", "large", "larger", "extralarge"]
    readonly property var subtitleTextSizeOptions: ["Smaller", "Small", "Normal", "Large", "Larger", "Extra large"]
    readonly property var subtitleTextWeightValues: ["normal", "bold"]
    readonly property var subtitleTextWeightOptions: ["Normal", "Bold"]
    readonly property var subtitleFontValues: ["", "typewriter", "print", "console", "cursive", "casual", "smallcaps"]
    readonly property var subtitleFontOptions: ["Default", "Typewriter", "Print", "Console", "Cursive", "Casual", "Small caps"]
    readonly property var subtitleTextColorValues: ["#ffffff", "#d3d3d3", "#808080", "#ffff00", "#008000", "#00ffff", "#0000ff", "#ff00ff", "#ff0000", "#000000"]
    readonly property var subtitleTextColorOptions: ["White", "Light gray", "Gray", "Yellow", "Green", "Cyan", "Blue", "Magenta", "Red", "Black"]
    readonly property var subtitleDropShadowValues: ["none", "raised", "depressed", "uniform", ""]
    readonly property var subtitleDropShadowOptions: ["None", "Raised", "Depressed", "Uniform", "Drop shadow"]

    component SettingsSelectRow: SelectRow {
        metricsWidth: root.width
    }

    component SettingsSliderRow: SliderRow {
        metricsWidth: root.width
    }

    function categoryTarget(index) {
        const targets = [themeRow, posterSizeRow, nightModeRow, subtitleLanguageRow,
                         diagnosticsRow, redButtonRow, aboutVersionRow]
        const row = index >= 0 && index < targets.length ? targets[index] : themeRow
        return row ? Math.max(0, row.settingIndex) : 0
    }

    function rebuildSettingsRows() {
        const rows = [
            themeRow, languageRow, accentRow, uiScaleRow, logoutRow, posterSizeRow,
            gridColumnsRow, railLabelsRow, reducedMotionRow,
            renderModeRow, antialiasedRow, metadataRow,
            nightModeRow, audioDelayRow, audioOutputRow,
            subtitleLanguageRow, subtitleModeRow, subtitleBurnInRow, subtitleRenderPgsRow,
            subtitleAlwaysBurnInRow, subtitleStylingRow, subtitleTextSizeRow, subtitleTextWeightRow,
            subtitleFontRow, subtitleTextColorRow, subtitleDropShadowRow, subtitleVerticalPositionRow,
            diagnosticsRow, gpuNextToneMappingRow,
            redButtonRow, greenButtonRow, yellowButtonRow, blueButtonRow
        ].filter(row => row && row.visible !== false)
        rows.push(aboutVersionRow, aboutServerRow, aboutLocaleRow)
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

    function valueIndex(values, currentValue) {
        for (let i = 0; i < values.length; ++i) {
            if (values[i] === currentValue)
                return i
        }
        return 0
    }

    function valueFromIndex(values, index) {
        return index >= 0 && index < values.length ? values[index] : values[0]
    }

    function subtitleModeDescription(mode) {
        if (mode === "Smart") return "Show subtitles when audio is not in your preferred language"
        if (mode === "OnlyForced") return "Show only forced subtitle tracks"
        if (mode === "Always") return "Show subtitles whenever a matching track is available"
        if (mode === "None") return "Do not automatically show subtitles"
        return "Use the Jellyfin account default"
    }

    function subtitleStylingDescription(styling) {
        if (styling === "Custom") return "Use the subtitle appearance values below"
        if (styling === "Native") return "Respect embedded subtitle styling when available"
        return "Use custom styling when it improves readability"
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
        categoryList.positionViewAtIndex(categoryIndex, ListView.Contain)
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
            if (key === Qt.Key_Right || InputKeys.isAccept(key, false)) {
                activateCategory(categoryIndex)
                return true
            }
            if (key === Qt.Key_Up) {
                if (categoryIndex <= 0) shell.focusNavBar()
                else focusCategory(categoryIndex - 1)
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
            if (currentIndex <= 0) shell.focusNavBar()
            else focusRow(currentIndex - 1)
            return true
        }
        if (key === Qt.Key_Down) {
            focusRow(currentIndex + 1)
            return true
        }
        return false
    }

    Component.onCompleted: Qt.callLater(function() {
        root.rebuildSettingsRows()
        root.focusRow(0)
    })
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
            onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)
            FastWheelHandler { flickable: categoryList }

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

            Behavior on contentY {
                enabled: !Theme.reducedMotion
                NumberAnimation { duration: 90; easing.type: Easing.OutCubic }
            }
            FastWheelHandler { flickable: settingsFlick }

            ColumnLayout {
                id: settings
                width: parent.width
                spacing: 10

                SectionHeader { Layout.fillWidth: true; title: "Preferences" }
                SectionHeader { Layout.fillWidth: true; title: "General" }

                SettingRow {
                    id: themeRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Theme"
                    description: "Fixed dark TV interface"
                    valueText: "Jellyfin Dark"
                    pointerActivationEnabled: false
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SettingsSelectRow {
                    id: languageRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Language"
                    description: "Restart the app to update cached server text"
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
                SettingsSelectRow {
                    id: accentRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Accent"
                    options: ["Jellyfin Blue", "Jellyfin Purple", "Blue-Purple"]
                    currentIndex: Theme.accentIndex
                    onSelected: (i, v) => Theme.accentIndex = i
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SettingsSliderRow {
                    id: uiScaleRow
                    Layout.fillWidth: true
                    selected: root.currentIndex === settingIndex
                    title: "UI Scale"
                    description: "Runtime type and spacing scale"
                    from: 0.75
                    to: 1.5
                    step: 0.05
                    decimals: 2
                    unitText: "x"
                    valueBoxWidth: 78
                    sliderPreferredWidth: 280
                    value: Metrics.userUiScale
                    onValueEdited: Metrics.userUiScale = value
                    onRowFocusChanged: if (rowFocus) root.markFocused(settingIndex)
                }
                SettingRow {
                    id: logoutRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Logout"
                    description: "Clear the saved session and return to sign in"
                    valueText: "Sign out"
                    onClicked: appController.logout()
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                    function handleNavigationKey(key) {
                        if (InputKeys.isAccept(key)) {
                            appController.logout()
                            return true
                        }
                        return false
                    }
                    Keys.onReleased: (event) => {
                        if (InputKeys.isAccept(event.key)) {
                            appController.logout()
                            event.accepted = true
                        }
                    }
                }
                SectionHeader { Layout.fillWidth: true; title: "Appearance" }
                SettingsSelectRow {
                    id: posterSizeRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Poster Size"
                    options: ["Compact", "Normal", "Large"]
                    currentIndex: Metrics.userPosterSizeBias + 1
                    onSelected: (i, v) => Metrics.userPosterSizeBias = i - 1
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SettingsSelectRow {
                    id: gridColumnsRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Grid Columns"
                    options: ["Auto", "4", "5", "6", "7", "8", "9"]
                    currentIndex: {
                        if (Metrics.userColumnOverride <= 0) return 0
                        const columns = [4, 5, 6, 7, 8, 9]
                        for (let i = 0; i < columns.length; ++i) {
                            if (columns[i] === Metrics.userColumnOverride)
                                return i + 1
                        }
                        return 0
                    }
                    onSelected: (i, v) => Metrics.userColumnOverride = i === 0 ? 0 : Number(v)
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SettingsSelectRow {
                    id: railLabelsRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Side Rail Labels"
                    options: ["Never", "On focus", "Always"]
                    currentIndex: {
                        if (Theme.sideRailLabels === "Never") return 0
                        if (Theme.sideRailLabels === "Always") return 2
                        return 1
                    }
                    onSelected: (i, v) => Theme.sideRailLabels = v
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                ToggleRow {
                    id: reducedMotionRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Reduced Motion"
                    checked: Theme.reducedMotion
                    onToggled: Theme.reducedMotion = checked
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SettingsSelectRow {
                    id: renderModeRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Text Render Mode"
                    options: ["Qt", "Curve"]
                    currentIndex: Theme.normalTextRenderType === Text.CurveRendering ? 1 : 0
                    onSelected: (i, v) => Theme.normalTextRenderType = i === 1 ? Text.CurveRendering : Text.QtRendering
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                ToggleRow {
                    id: antialiasedRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Antialiased Text"
                    checked: Theme.antialiasedText
                    onToggled: Theme.antialiasedText = checked
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SettingsSelectRow {
                    id: metadataRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Show Technical Metadata"
                    options: ["Always", "On details only", "Hidden"]
                    currentIndex: {
                        if (Theme.technicalMetadataMode === "On details only") return 1
                        if (Theme.technicalMetadataMode === "Hidden") return 2
                        return 0
                    }
                    onSelected: (i, v) => Theme.technicalMetadataMode = v
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SectionHeader { Layout.fillWidth: true; title: "Playback" }
                ToggleRow {
                    id: nightModeRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Night Mode"
                    description: "Dialogue lift and late-night dynamic range"
                    checked: appController.nightModeEnabled
                    onToggled: appController.setNightModeEnabled(checked)
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SettingsSliderRow {
                    id: audioDelayRow
                    Layout.fillWidth: true
                    selected: root.currentIndex === settingIndex
                    title: "A/V Sync"
                    description: "Audio delay in milliseconds"
                    from: -2000
                    to: 2000
                    step: 10
                    decimals: 0
                    unitText: "ms"
                    valueBoxWidth: 92
                    sliderPreferredWidth: 340
                    value: appController.audioDelayMs
                    onValueEdited: (value) => appController.setAudioDelayMs(Math.round(value))
                    onRowFocusChanged: if (rowFocus) root.markFocused(settingIndex)
                }
                SettingsSelectRow {
                    id: audioOutputRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Audio Output"
                    description: "Takes effect on the next playback start"
                    options: ["ALSA", "Starfish"]
                    currentIndex: (appController.audioOutputMode === "starfish"
                                   || appController.audioOutputMode === "starfish-pcm") ? 1 : 0
                    onSelected: (i, v) => appController.setAudioOutputMode(i === 1 ? "starfish-pcm" : "alsa")
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SectionHeader { Layout.fillWidth: true; title: "Subtitles" }
                SettingsSelectRow {
                    id: subtitleLanguageRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Preferred Language"
                    options: appController ? appController.subtitleLanguageOptions : ["Any language"]
                    currentIndex: appController ? appController.subtitleLanguageIndex : 0
                    onSelected: (i, v) => appController.setSubtitleLanguageIndex(i)
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SettingsSelectRow {
                    id: subtitleModeRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Playback Mode"
                    description: root.subtitleModeDescription(appController ? appController.subtitleMode : "Default")
                    options: root.subtitleModeOptions
                    currentIndex: root.valueIndex(root.subtitleModeValues, appController ? appController.subtitleMode : "Default")
                    onSelected: (i, v) => appController.setSubtitleMode(root.valueFromIndex(root.subtitleModeValues, i))
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SettingsSelectRow {
                    id: subtitleBurnInRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Burn Subtitles"
                    description: "Used when transcoding is enabled"
                    options: root.subtitleBurnInOptions
                    currentIndex: root.valueIndex(root.subtitleBurnInValues, appController ? appController.subtitleBurnIn : "")
                    onSelected: (i, v) => appController.setSubtitleBurnIn(root.valueFromIndex(root.subtitleBurnInValues, i))
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                ToggleRow {
                    id: subtitleRenderPgsRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Render PGS Subtitles"
                    description: "Prefer local rendering for image subtitles"
                    checked: appController ? appController.subtitleRenderPgs : false
                    onToggled: appController.setSubtitleRenderPgs(checked)
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                ToggleRow {
                    id: subtitleAlwaysBurnInRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Always Burn In"
                    description: "When playback falls back to transcoding"
                    checked: appController ? appController.subtitleAlwaysBurnIn : false
                    onToggled: appController.setSubtitleAlwaysBurnIn(checked)
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SectionHeader { Layout.fillWidth: true; title: "Subtitle Appearance" }
                SettingsSelectRow {
                    id: subtitleStylingRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Styling"
                    description: root.subtitleStylingDescription(appController ? appController.subtitleStyling : "Auto")
                    options: root.subtitleStylingOptions
                    currentIndex: root.valueIndex(root.subtitleStylingValues, appController ? appController.subtitleStyling : "Auto")
                    onSelected: (i, v) => appController.setSubtitleStyling(root.valueFromIndex(root.subtitleStylingValues, i))
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SettingsSelectRow {
                    id: subtitleTextSizeRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Text Size"
                    options: root.subtitleTextSizeOptions
                    currentIndex: root.valueIndex(root.subtitleTextSizeValues, appController ? appController.subtitleTextSize : "")
                    onSelected: (i, v) => appController.setSubtitleTextSize(root.valueFromIndex(root.subtitleTextSizeValues, i))
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SettingsSelectRow {
                    id: subtitleTextWeightRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Text Weight"
                    options: root.subtitleTextWeightOptions
                    currentIndex: root.valueIndex(root.subtitleTextWeightValues, appController ? appController.subtitleTextWeight : "normal")
                    onSelected: (i, v) => appController.setSubtitleTextWeight(root.valueFromIndex(root.subtitleTextWeightValues, i))
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SettingsSelectRow {
                    id: subtitleFontRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Font"
                    options: root.subtitleFontOptions
                    currentIndex: root.valueIndex(root.subtitleFontValues, appController ? appController.subtitleFont : "")
                    onSelected: (i, v) => appController.setSubtitleFont(root.valueFromIndex(root.subtitleFontValues, i))
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SettingsSelectRow {
                    id: subtitleTextColorRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Text Color"
                    options: root.subtitleTextColorOptions
                    currentIndex: root.valueIndex(root.subtitleTextColorValues, appController ? appController.subtitleTextColor : "#ffffff")
                    onSelected: (i, v) => appController.setSubtitleTextColor(root.valueFromIndex(root.subtitleTextColorValues, i))
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SettingsSelectRow {
                    id: subtitleDropShadowRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Drop Shadow"
                    options: root.subtitleDropShadowOptions
                    currentIndex: root.valueIndex(root.subtitleDropShadowValues, appController ? appController.subtitleDropShadow : "")
                    onSelected: (i, v) => appController.setSubtitleDropShadow(root.valueFromIndex(root.subtitleDropShadowValues, i))
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SettingsSliderRow {
                    id: subtitleVerticalPositionRow
                    Layout.fillWidth: true
                    selected: root.currentIndex === settingIndex
                    title: "Vertical Position"
                    description: "Negative values place subtitles near the bottom"
                    from: -16
                    to: 16
                    step: 1
                    decimals: 0
                    unitText: ""
                    valueBoxWidth: 72
                    value: appController ? appController.subtitleVerticalPosition : -3
                    onValueEdited: (value) => appController.setSubtitleVerticalPosition(value)
                    onRowFocusChanged: if (rowFocus) root.markFocused(settingIndex)
                }
                SectionHeader { Layout.fillWidth: true; title: "Diagnostics" }
                ToggleRow {
                    id: diagnosticsRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Diagnostics Overlay"
                    checked: shell.diagnosticsVisible
                    onToggled: shell.diagnosticsVisible = checked
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                ToggleRow {
                    id: gpuNextToneMappingRow
                    Layout.fillWidth: true
                    visible: root.gpuNextDiagnosticsAvailable
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "GPU-next Tone Mapping View"
                    description: "False-colour libplacebo tone-mapping diagnostic"
                    checked: appController ? appController.toneMappingVisualizationEnabled : false
                    onToggled: if (appController) appController.setToneMappingVisualizationEnabled(checked)
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SectionHeader { Layout.fillWidth: true; title: "Input" }
                SectionHeader { Layout.fillWidth: true; title: "Button Remap" }
                SettingsSelectRow {
                    id: redButtonRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Red Button"
                    description: "TV remote color button"
                    options: root.buttonActionOptions()
                    currentIndex: root.buttonActionIndex(appController ? appController.redButtonAction : "none")
                    onSelected: (i, v) => appController.setRedButtonAction(root.actionFromIndex(i))
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SettingsSelectRow {
                    id: greenButtonRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Green Button"
                    description: "Defaults to skip back 10 s + enable subs"
                    options: root.buttonActionOptions()
                    currentIndex: root.buttonActionIndex(appController ? appController.greenButtonAction : "none")
                    onSelected: (i, v) => appController.setGreenButtonAction(root.actionFromIndex(i))
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SettingsSelectRow {
                    id: yellowButtonRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Yellow Button"
                    options: root.buttonActionOptions()
                    currentIndex: root.buttonActionIndex(appController ? appController.yellowButtonAction : "none")
                    onSelected: (i, v) => appController.setYellowButtonAction(root.actionFromIndex(i))
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SettingsSelectRow {
                    id: blueButtonRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Blue Button"
                    options: root.buttonActionOptions()
                    currentIndex: root.buttonActionIndex(appController ? appController.blueButtonAction : "none")
                    onSelected: (i, v) => appController.setBlueButtonAction(root.actionFromIndex(i))
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }

                SectionHeader { Layout.fillWidth: true; title: "About" }
                SettingRow {
                    id: aboutVersionRow
                    Layout.fillWidth: true
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
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Connected Server"
                    description: appController ? appController.serverUrl : ""
                    valueText: appController && appController.serverUrl.length > 0 ? "Connected" : "Offline"
                    pointerActivationEnabled: false
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SettingRow {
                    id: aboutLocaleRow
                    Layout.fillWidth: true
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "UI Locale"
                    description: i18n ? "Active translation tag" : ""
                    valueText: i18n ? i18n.currentLocale : "en-US"
                    pointerActivationEnabled: false
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }

                Component.onCompleted: {
                    root.rebuildSettingsRows()
                    root.focusRow(root.currentIndex)
                }
            }
        }
    }
}
