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
        { label: "General", target: 0 },
        { label: "Appearance", target: 3 },
        { label: "Playback", target: 10 },
        { label: "Diagnostics", target: 14 },
        { label: "Input", target: 15 }
    ]
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
        focusRow(categories[categoryIndex].target)
    }

    function syncCategoryForRow(rowIndex) {
        let nextCategory = 0
        for (let i = 0; i < categories.length; ++i) {
            if (rowIndex >= categories[i].target)
                nextCategory = i
        }
        categoryIndex = nextCategory
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
                SectionHeader { Layout.fillWidth: true; title: "Appearance" }
                SelectRow {
                    id: posterSizeRow
                    Layout.fillWidth: true
                    settingIndex: 3
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
                    settingIndex: 4
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Grid Columns"
                    options: ["Auto", "4", "5", "6", "7", "8", "9"]
                    onSelected: (i, v) => Metrics.userColumnOverride = i === 0 ? 0 : Number(v)
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SelectRow {
                    id: railLabelsRow
                    Layout.fillWidth: true
                    settingIndex: 5
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
                    settingIndex: 6
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Reduced Motion"
                    checked: Theme.reducedMotion
                    onToggled: Theme.reducedMotion = checked
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SelectRow {
                    id: renderModeRow
                    Layout.fillWidth: true
                    settingIndex: 7
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
                    settingIndex: 8
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Antialiased Text"
                    checked: Theme.antialiasedText
                    onToggled: Theme.antialiasedText = checked
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SelectRow {
                    id: metadataRow
                    Layout.fillWidth: true
                    settingIndex: 9
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
                    settingIndex: 10
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
                    settingIndex: 11
                    title: "A/V sync"
                    description: "Audio delay in milliseconds"
                    valueMs: appController.audioDelayMs
                    onValueEdited: (value) => appController.setAudioDelayMs(value)
                    onRowFocusChanged: if (rowFocus) root.markFocused(settingIndex)
                }
                SelectRow {
                    id: bitrateRow
                    Layout.fillWidth: true
                    settingIndex: 12
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Maximum remote bitrate"
                    options: ["Auto", "20 Mbps", "40 Mbps", "80 Mbps", "Unlimited"]
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                ToggleRow {
                    id: remuxRow
                    Layout.fillWidth: true
                    settingIndex: 13
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Prefer remux over transcode"
                    checked: true
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }
                SectionHeader { Layout.fillWidth: true; title: "Diagnostics" }
                ToggleRow {
                    id: diagnosticsRow
                    Layout.fillWidth: true
                    settingIndex: 14
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
                    settingIndex: 15
                    rowFocus: root.currentIndex === settingIndex || activeFocus
                    title: "Keyboard shortcuts enabled"
                    checked: true
                    onActiveFocusChanged: if (activeFocus) root.markFocused(settingIndex)
                }

                Component.onCompleted: {
                    root.settingsRows = [
                        themeRow, accentRow, uiScaleRow, posterSizeRow,
                        gridColumnsRow, railLabelsRow, reducedMotionRow,
                        renderModeRow, antialiasedRow, metadataRow,
                        nightModeRow, audioDelayRow, bitrateRow,
                        remuxRow, diagnosticsRow, shortcutsRow
                    ]
                    for (let i = 0; i < root.settingsRows.length; ++i)
                        root.settingsRows[i].settingIndex = i
                    root.focusRow(root.currentIndex)
                }
            }
        }
    }
}
