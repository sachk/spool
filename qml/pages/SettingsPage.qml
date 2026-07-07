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
        { label: "General", group: "General" },
        { label: "Appearance", group: "Appearance" },
        { label: "Playback", group: "Playback" },
        { label: "Subtitles", group: "Subtitles" },
        { label: "Diagnostics", group: "Diagnostics" },
        { label: "Input", group: "Button Remap" },
        { label: "About", group: "About" }
    ]
    readonly property var topRows: [
        { key: "action/switchUser", source: "page", group: "General", type: "action", title: "Switch User", description: "Return to profile selection", valueText: "Choose" },
        { key: "action/logout", source: "page", group: "General", type: "action", title: "Logout", description: "Clear the saved session and return to sign in", valueText: "Sign out" },
        { key: "session/server", source: "page", group: "General", type: "readonly", title: "Connected Server" }
    ]
    readonly property var pageSchemaRows: [
        { key: "theme/name", source: "page", group: "General", type: "readonly", title: "Theme", description: "Fixed dark TV interface", valueText: "Jellyfin Dark" },
        { key: "i18n/locale", source: "page", group: "General", type: "select", title: "Language", description: "Restart the app to update cached server text" },
        { key: "theme/accent", source: "page", group: "General", type: "select", title: "Accent", choiceLabels: ["Jellyfin Blue", "Jellyfin Purple", "Blue-Purple"] },
        { key: "metrics/uiScale", source: "page", group: "General", type: "slider", title: "UI Scale", description: "Runtime type and spacing scale", from: 0.75, to: 1.5, step: 0.05, decimals: 2, unitText: "x", valueBoxWidth: 78, sliderPreferredWidth: 280 },
        { key: "metrics/posterSize", source: "page", group: "Appearance", type: "select", title: "Poster Size", choiceLabels: ["Compact", "Normal", "Large"] },
        { key: "metrics/gridColumns", source: "page", group: "Appearance", type: "select", title: "Grid Columns", choiceLabels: ["Auto", "4", "5", "6", "7", "8", "9"] },
        { key: "theme/railLabels", source: "page", group: "Appearance", type: "select", title: "Side Rail Labels", choiceLabels: ["Never", "On focus", "Always"] },
        { key: "theme/reducedMotion", source: "page", group: "Appearance", type: "toggle", title: "Reduced Motion" },
        { key: "theme/renderMode", source: "page", group: "Appearance", type: "select", title: "Text Render Mode", choiceLabels: ["Qt", "Curve"] },
        { key: "theme/antialiasedText", source: "page", group: "Appearance", type: "toggle", title: "Antialiased Text" },
        { key: "theme/technicalMetadata", source: "page", group: "Appearance", type: "select", title: "Show Technical Metadata", choiceLabels: ["Always", "On details only", "Hidden"] },
        { key: "shell/diagnostics", source: "page", group: "Diagnostics", type: "toggle", title: "Diagnostics Overlay" }
    ]
    readonly property var aboutRows: [
        { key: "about/version", source: "page", group: "About", type: "readonly", title: "Jellyfin Native for webOS", description: "Qt 6.11 client, native mpv playback" },
        { key: "about/locale", source: "page", group: "About", type: "readonly", title: "UI Locale" }
    ]

    component SettingsSelectRow: SelectRow {
        metricsWidth: root.width
        focus: false
        focusPolicy: Qt.NoFocus
    }

    component SettingsSliderRow: SliderRow {
        metricsWidth: root.width
        focus: false
        focusPolicy: Qt.NoFocus
    }

    function controllerSchemaRows() {
        return appController && appController.settings ? appController.settings.settingsSchema : [];
    }

    function rowVisible(row) {
        if (!row || row.visible === false)
            return false;
        if (row.key === "settings/toneMappingVisualization")
            return gpuNextDiagnosticsAvailable;
        return true;
    }

    function rebuildSettingsRows() {
        const rows = [];
        const append = function (list) {
            for (let i = 0; i < list.length; ++i) {
                if (rowVisible(list[i]))
                    rows.push(list[i]);
            }
        };
        append(topRows);
        append(pageSchemaRows);
        append(controllerSchemaRows());
        append(aboutRows);
        settingsRows = rows;
        currentIndex = Math.max(0, Math.min(currentIndex, settingsRows.length - 1));
        settingsList.currentIndex = currentIndex;
        syncCategoryForRow(currentIndex);
    }

    function rowAt(index) {
        return index >= 0 && index < settingsRows.length ? settingsRows[index] : null;
    }

    function previousRow(index) {
        return index > 0 ? settingsRows[index - 1] : null;
    }

    function showPreferencesHeader(index) {
        return index === 0;
    }

    function showGroupHeader(index) {
        const row = rowAt(index);
        const previous = previousRow(index);
        return row && (!previous || previous.group !== row.group);
    }

    function categoryTarget(index) {
        if (settingsRows.length <= 0)
            return 0;
        const category = categories[Math.max(0, Math.min(categories.length - 1, index))];
        for (let i = 0; i < settingsRows.length; ++i) {
            if (settingsRows[i].group === category.group)
                return i;
        }
        return 0;
    }

    function syncCategoryForRow(rowIndex) {
        let nextCategory = 0;
        for (let i = 0; i < categories.length; ++i) {
            if (rowIndex >= categoryTarget(i))
                nextCategory = i;
        }
        categoryIndex = nextCategory;
        if (categoryList.currentIndex !== categoryIndex)
            categoryList.currentIndex = categoryIndex;
    }

    function focusRow(index) {
        if (settingsRows.length <= 0)
            return;
        currentIndex = Math.max(0, Math.min(settingsRows.length - 1, index));
        settingsList.currentIndex = currentIndex;
        InputKeys.focus(settingsList);
        syncCategoryForRow(currentIndex);
        settingsList.positionViewAtIndex(currentIndex, ListView.Contain);
    }

    function focusCategory(index) {
        categoryIndex = Math.max(0, Math.min(categories.length - 1, index));
        categoryList.currentIndex = categoryIndex;
        InputKeys.focus(categoryList);
        categoryList.positionViewAtIndex(categoryIndex, ListView.Contain);
    }

    function activateCategory(index) {
        categoryIndex = Math.max(0, Math.min(categories.length - 1, index));
        focusRow(categoryTarget(categoryIndex));
    }

    function settingsValue(row) {
        if (!appController || !appController.settings)
            return row.defaultValue;
        const values = appController.settings.values;
        const value = values[row.key];
        return value === undefined ? row.defaultValue : value;
    }

    function valueIndex(values, currentValue) {
        for (let i = 0; i < values.length; ++i) {
            if (values[i] === currentValue)
                return i;
        }
        return 0;
    }

    function valueFromIndex(values, index) {
        return index >= 0 && index < values.length ? values[index] : values[0];
    }

    function rowDescription(row) {
        if (row.key === "subtitles/mode") {
            const mode = String(settingsValue(row));
            if (mode === "Smart")
                return "Show subtitles when audio is not in your preferred language";
            if (mode === "OnlyForced")
                return "Show only forced subtitle tracks";
            if (mode === "Always")
                return "Show subtitles whenever a matching track is available";
            if (mode === "None")
                return "Do not automatically show subtitles";
            return "Use the Jellyfin account default";
        }
        if (row.key === "subtitles/styling") {
            const styling = String(settingsValue(row));
            if (styling === "Custom")
                return "Use the subtitle appearance values below";
            if (styling === "Native")
                return "Respect embedded subtitle styling when available";
            return "Use custom styling when it improves readability";
        }
        if (row.key === "session/server")
            return appController ? appController.session.serverUrl : "";
        if (row.key === "about/locale")
            return i18n ? "Active translation tag" : "";
        return row.description || "";
    }

    function rowValueText(row) {
        switch (row.key) {
        case "session/server":
            return appController && appController.session.serverUrl.length > 0 ? "Connected" : "Offline";
        case "about/version":
            return "v" + Qt.application.version;
        case "about/locale":
            return i18n ? i18n.currentLocale : "en-US";
        default:
            return row.valueText || "";
        }
    }

    function rowOptions(row) {
        switch (row.key) {
        case "i18n/locale": {
            if (!i18n)
                return ["System default"];
            const result = [];
            const list = i18n.availableLocales;
            for (let i = 0; i < list.length; ++i)
                result.push(i18n.displayNameFor(list[i]));
            return result;
        }
        case "subtitles/language":
            return appController ? appController.settings.subtitleLanguageOptions : ["Any language"];
        default:
            return row.choiceLabels || [];
        }
    }

    function rowChoiceValues(row) {
        switch (row.key) {
        case "i18n/locale":
            return i18n ? i18n.availableLocales : ["system"];
        case "theme/accent":
        case "metrics/posterSize":
            return [0, 1, 2];
        case "metrics/gridColumns":
            return [0, 4, 5, 6, 7, 8, 9];
        case "theme/railLabels":
            return ["Never", "On focus", "Always"];
        case "theme/renderMode":
            return [Text.QtRendering, Text.CurveRendering];
        case "theme/technicalMetadata":
            return ["Always", "On details only", "Hidden"];
        default:
            return row.choiceValues || [];
        }
    }

    function rowCurrentIndex(row) {
        switch (row.key) {
        case "i18n/locale": {
            if (!i18n)
                return 0;
            const list = i18n.availableLocales;
            for (let i = 0; i < list.length; ++i) {
                if ((list[i] === "system" && i18n.useSystemLocale) || list[i] === i18n.currentLocale)
                    return i;
            }
            return 0;
        }
        case "theme/accent":
            return Theme.accentIndex;
        case "metrics/posterSize":
            return Metrics.userPosterSizeBias + 1;
        case "metrics/gridColumns": {
            if (Metrics.userColumnOverride <= 0)
                return 0;
            const columns = [4, 5, 6, 7, 8, 9];
            for (let i = 0; i < columns.length; ++i) {
                if (columns[i] === Metrics.userColumnOverride)
                    return i + 1;
            }
            return 0;
        }
        case "theme/railLabels":
            if (Theme.sideRailLabels === "Never")
                return 0;
            if (Theme.sideRailLabels === "Always")
                return 2;
            return 1;
        case "theme/renderMode":
            return Theme.normalTextRenderType === Text.CurveRendering ? 1 : 0;
        case "theme/technicalMetadata":
            if (Theme.technicalMetadataMode === "On details only")
                return 1;
            if (Theme.technicalMetadataMode === "Hidden")
                return 2;
            return 0;
        case "subtitles/language":
            return appController ? appController.settings.subtitleLanguageIndex : 0;
        default:
            return valueIndex(rowChoiceValues(row), settingsValue(row));
        }
    }

    function rowBool(row) {
        switch (row.key) {
        case "theme/reducedMotion":
            return Theme.reducedMotion;
        case "theme/antialiasedText":
            return Theme.antialiasedText;
        case "shell/diagnostics":
            return shell ? shell.diagnosticsVisible : false;
        default:
            return Boolean(settingsValue(row));
        }
    }

    function rowNumber(row) {
        switch (row.key) {
        case "metrics/uiScale":
            return Metrics.userUiScale;
        default:
            return Number(settingsValue(row));
        }
    }

    function setRowChoice(row, index) {
        switch (row.key) {
        case "i18n/locale": {
            if (!i18n)
                return;
            const list = i18n.availableLocales;
            if (index >= 0 && index < list.length)
                i18n.setLocale(list[index]);
            return;
        }
        case "theme/accent":
            Theme.accentIndex = index;
            return;
        case "metrics/posterSize":
            Metrics.userPosterSizeBias = index - 1;
            return;
        case "metrics/gridColumns":
            Metrics.userColumnOverride = index === 0 ? 0 : Number(rowOptions(row)[index]);
            return;
        case "theme/railLabels":
            Theme.sideRailLabels = String(rowOptions(row)[index]);
            return;
        case "theme/renderMode":
            Theme.normalTextRenderType = index === 1 ? Text.CurveRendering : Text.QtRendering;
            return;
        case "theme/technicalMetadata":
            Theme.technicalMetadataMode = String(rowOptions(row)[index]);
            return;
        case "subtitles/language":
            if (appController)
                appController.settings.setSubtitleLanguageIndex(index);
            return;
        default:
            if (appController && appController.settings) {
                const values = rowChoiceValues(row);
                appController.settings.setValue(row.key, valueFromIndex(values, index));
            }
        }
    }

    function setRowBool(row, checked) {
        switch (row.key) {
        case "theme/reducedMotion":
            Theme.reducedMotion = checked;
            return;
        case "theme/antialiasedText":
            Theme.antialiasedText = checked;
            return;
        case "shell/diagnostics":
            if (shell)
                shell.diagnosticsVisible = checked;
            return;
        default:
            if (appController && appController.settings)
                appController.settings.setValue(row.key, checked);
        }
    }

    function setRowNumber(row, value) {
        switch (row.key) {
        case "metrics/uiScale":
            Metrics.userUiScale = value;
            return;
        default:
            if (appController && appController.settings)
                appController.settings.setValue(row.key, Math.round(value));
        }
    }

    function activateRow(row, index) {
        if (!row)
            return;
        currentIndex = index;
        settingsList.currentIndex = index;
        switch (row.type) {
        case "action":
            if (row.key === "action/switchUser" && shell)
                shell.switchUser();
            else if (row.key === "action/logout" && appController)
                appController.logout();
            return;
        case "toggle":
            setRowBool(row, !rowBool(row));
            return;
        case "select":
            adjustRow(row, 1);
            return;
        default:
            return;
        }
    }

    function adjustRow(row, direction) {
        if (!row)
            return false;
        if (row.type === "select") {
            const options = rowOptions(row);
            if (options.length <= 0)
                return true;
            setRowChoice(row, (rowCurrentIndex(row) + direction + options.length) % options.length);
            return true;
        }
        if (row.type === "slider") {
            const step = Number(row.step || 1);
            const from = Number(row.from || 0);
            const to = Number(row.to || 100);
            const next = Math.max(from, Math.min(to, rowNumber(row) + step * direction));
            setRowNumber(row, next);
            return true;
        }
        return false;
    }

    function handleKey(key) {
        if (categoryList.activeFocus) {
            if (key === Qt.Key_Right || InputKeys.isAccept(key, false)) {
                activateCategory(categoryIndex);
                return true;
            }
            if (key === Qt.Key_Up && categoryIndex <= 0) {
                if (shell)
                    shell.focusNavBar();
                return true;
            }
            return categoryList.handleKey(key);
        }

        const row = rowAt(settingsList.currentIndex);
        if ((key === Qt.Key_Left || key === Qt.Key_Right) && adjustRow(row, key === Qt.Key_Right ? 1 : -1))
            return true;
        if (key === Qt.Key_Left) {
            focusCategory(categoryIndex);
            return true;
        }
        if (key === Qt.Key_Up && settingsList.currentIndex <= 0) {
            if (shell)
                shell.focusNavBar();
            return true;
        }
        return settingsList.handleKey(key);
    }

    focus: true
    onActiveFocusChanged: if (activeFocus)
        focusRow(currentIndex)
    Keys.onReleased: event => {
        if (handleKey(event.key))
            event.accepted = true;
    }

    Component.onCompleted: Qt.callLater(function () {
        rebuildSettingsRows();
        focusRow(0);
    })

    Connections {
        target: appController ? appController.settings : null
        function onSettingsValuesChanged() {
            settingsList.forceLayout();
        }
        function onSubtitleSettingsChanged() {
            settingsList.forceLayout();
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin(width)
        spacing: 18

        NavList {
            id: categoryList
            Layout.preferredWidth: 260
            Layout.fillHeight: true
            model: root.categories
            spacing: 8
            currentIndex: root.categoryIndex
            clip: true
            onCurrentIndexChanged: if (currentIndex >= 0) {
                root.categoryIndex = currentIndex;
                positionViewAtIndex(currentIndex, ListView.Contain);
            }
            onAccepted: index => root.activateCategory(index)
            onEdgeUp: if (root.shell) root.shell.focusNavBar()
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

                TapHandler { onTapped: root.activateCategory(index) }
            }
        }

        NavList {
            id: settingsList
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: root.settingsRows
            spacing: 10
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            currentIndex: root.currentIndex
            onCurrentIndexChanged: if (currentIndex >= 0) {
                root.currentIndex = currentIndex;
                root.syncCategoryForRow(currentIndex);
                positionViewAtIndex(currentIndex, ListView.Contain);
            }
            onAccepted: index => root.activateRow(root.rowAt(index), index)
            onEdgeUp: if (root.shell) root.shell.focusNavBar()
            FastWheelHandler { flickable: settingsList }

            delegate: Column {
                required property int index
                required property var modelData
                width: settingsList.width
                spacing: 10

                SectionHeader {
                    width: parent.width
                    visible: root.showPreferencesHeader(index)
                    title: "Preferences"
                }
                SectionHeader {
                    width: parent.width
                    visible: root.showGroupHeader(index)
                    title: modelData.group
                }
                Loader {
                    id: rowLoader
                    width: parent.width
                    property var row: modelData
                    property int rowIndex: index
                    sourceComponent: modelData.type === "toggle" ? toggleComponent
                                     : modelData.type === "select" ? selectComponent
                                     : modelData.type === "slider" ? sliderComponent
                                     : settingComponent
                    onLoaded: {
                        item.row = row;
                        item.rowIndex = rowIndex;
                    }
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
            checked: row ? root.rowBool(row) : false
            onToggled: checked => root.setRowBool(row, checked)
        }
    }

    Component {
        id: selectComponent
        SettingsSelectRow {
            property var row
            property int rowIndex: -1
            width: settingsList.width
            rowFocus: settingsList.activeFocus && settingsList.currentIndex === rowIndex
            title: row ? row.title : ""
            description: row ? root.rowDescription(row) : ""
            options: row ? root.rowOptions(row) : []
            currentIndex: row ? root.rowCurrentIndex(row) : 0
            onSelected: (i, v) => root.setRowChoice(row, i)
        }
    }

    Component {
        id: sliderComponent
        SettingsSliderRow {
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
            valueBoxWidth: row && row.valueBoxWidth > 0 ? row.valueBoxWidth : 86
            sliderPreferredWidth: row && row.sliderPreferredWidth > 0 ? row.sliderPreferredWidth : 300
            value: row ? root.rowNumber(row) : 0
            onValueEdited: value => root.setRowNumber(row, value)
        }
    }
}
