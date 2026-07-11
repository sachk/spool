pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root

    property var shell
    readonly property var search: Search
    readonly property string query: search ? search.query : ""
    readonly property int resultCount: search ? search.resultCount : 0
    readonly property int suggestionCount: search && search.suggestions ? search.suggestions.count : 0
    readonly property bool searchBusy: search ? search.busy : false
    readonly property bool suggestionsBusy: search ? search.suggestionsBusy : false
    readonly property bool showSuggestions: query.length < 2 && suggestionCount > 0
    readonly property var resultSections: [
        {
            "key": "movies",
            "title": "Movies",
            "model": search ? search.movieResults : null
        },
        {
            "key": "series",
            "title": "Series",
            "model": search ? search.seriesResults : null
        },
        {
            "key": "episodes",
            "title": "Episodes",
            "model": search ? search.episodeResults : null
        }
    ]
    property int currentSection: 0

    focus: true

    Component.onCompleted: {
        if (search)
        search.loadSuggestions()
        field.text = query
        field.focusField()
    }

    Connections {
        target: root.search

        function onQueryChanged() {
            if (field.text !== root.query)
                field.text = root.query
        }
        function onResultsChanged() {
            root.repairResultFocus()
        }
    }

    function resultRows() {
        const rows = []
        for (let index = 0; index < resultRepeater.count; ++index) {
            const row = resultRepeater.itemAt(index)
            if (row && row.rowVisible)
                rows.push(row)
        }
        return rows
    }

    function activeResultRow() {
        const rows = resultRows()
        if (rows.length <= 0)
            return null
        currentSection = Math.max(0, Math.min(currentSection, rows.length - 1))
        return rows[currentSection]
    }

    function focusResultRow(row) {
        const rows = resultRows()
        const index = rows.indexOf(row)
        if (index < 0 || !row.focusList())
            return false
        currentSection = index
        InputKeys.positionChild(resultsScroller, row)
        return true
    }

    function focusPreferredResult() {
        const rows = resultRows()
        if (rows.length <= 0)
            return false
        const preferredKind = shell ? shell.lastSearchKind : ""
        for (let index = 0; index < rows.length; ++index) {
            if (rows[index].resultKind === preferredKind)
                return focusResultRow(rows[index])
        }
        return focusResultRow(rows[0])
    }

    function repairResultFocus() {
        const row = activeResultRow()
        if (row && row.activeFocus)
            focusResultRow(row)
        else if (!row && resultsScroller.activeFocus)
            field.focusField()
    }

    function focusRelativeResult(direction) {
        const rows = resultRows()
        const current = activeResultRow()
        const index = rows.indexOf(current)
        const next = index + direction
        if (next < 0) {
            field.focusField()
            return true
        }
        if (next >= rows.length)
            return true
        return focusResultRow(rows[next])
    }

    function activateResult(row) {
        if (!row || !shell || row.currentIndex < 0)
            return
        shell.lastSearchKind = row.resultKind
        shell.lastSearchIndex = row.currentIndex
        shell.openDetailsAt(row.model, row.currentIndex, "search", "search")
    }

    function setQuery(text) {
        if (shell)
            shell.lastSearchIndex = 0
        if (search)
            search.setQuery(text)
    }

    function routeKey(key, phase, repeat) {
        if (suggestionsRow.activeFocus) {
            if (key === Qt.Key_Up) {
                field.focusField()
                return true
            }
            if (key === Qt.Key_Down)
                return true
            return suggestionsRow.routeKey(key, phase, repeat)
        }

        const row = activeResultRow()
        if (row && row.activeFocus) {
            if (key === Qt.Key_Up || key === Qt.Key_Down)
                return focusRelativeResult(key === Qt.Key_Down ? 1 : -1)
            return row.routeKey(key, phase, repeat)
        }

        if (key === Qt.Key_Up && field.activeFocus && !field.editing) {
            if (shell)
                shell.focusNavBar()
            return true
        }
        if (key !== Qt.Key_Down || !field.activeFocus)
            return false
        if (query.length >= 2)
            return focusPreferredResult()
        return showSuggestions && suggestionsRow.focusList()
    }

    function activate() {
        if (suggestionsRow.activeFocus) {
            if (shell && suggestionsRow.currentIndex >= 0) {
                shell.lastSearchKind = "suggestions"
                shell.lastSearchIndex = suggestionsRow.currentIndex
                shell.openDetailsAt(search.suggestions, suggestionsRow.currentIndex, "suggestion", "search")
            }
            return
        }
        activateResult(activeResultRow())
    }

    function longPress() {
        const row = activeResultRow()
        return Boolean(row && row.activeFocus && row.longPress())
    }

    function back() {
        const row = activeResultRow()
        if (!suggestionsRow.activeFocus && !(row && row.activeFocus))
            return false
        field.focusField()
        return true
    }

    readonly property real keyboardInset: {
        if (!Qt.inputMethod.visible)
        return 0
        const keyboard = Qt.inputMethod.keyboardRectangle
        if (keyboard.height <= 0)
        return 0
        const pageBottom = root.mapToItem(null, 0, root.height).y
        return Math.max(0, Math.min(root.height, pageBottom - keyboard.y))
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin(root.width)
        anchors.bottomMargin: Metrics.pageMargin(root.width) + root.keyboardInset
        spacing: Metrics.gap(root.width)

        Behavior on anchors.bottomMargin {
            enabled: !Theme.reducedMotion
            NumberAnimation {
                duration: 120
                easing.type: Easing.OutCubic
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.gap(root.width)

            SectionHeader {
                Layout.fillWidth: true
                title: "Search"
            }

            BusyIndicator {
                Layout.preferredWidth: Metrics.scaled(28)
                Layout.preferredHeight: width
                running: root.searchBusy
                visible: running
            }

            MonoText {
                visible: root.query.length >= 2 && !root.searchBusy
                text: root.resultCount + " result" + (root.resultCount === 1 ? "" : "s")
                color: Theme.textMuted
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Metrics.gap(root.width)

            ColumnLayout {
                Layout.fillWidth: false
                Layout.preferredWidth: Math.min(Metrics.scaled(520), Math.max(Metrics.scaled(320), root.width * 0.3))
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignTop
                spacing: Metrics.gap(root.width)

                TextFieldRow {
                    id: field

                    Layout.fillWidth: true
                    Layout.preferredHeight: Metrics.scaled(64)
                    label: "Search Jellyfin"
                    placeholderText: "Movies, series, and episodes"
                    inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                    enterKeyType: Qt.EnterKeySearch
                    onTextEdited: text => root.setQuery(text)
                    onAccepted: if (root.search)
                    root.search.submit()
                }

                MediaRow {
                    id: suggestionsRow

                    Layout.fillWidth: true
                    Layout.preferredHeight: implicitHeight
                    title: "Suggestions"
                    model: root.search ? root.search.suggestions : null
                    shell: root.shell
                    cardWidth: Metrics.homePosterWidth(root.width)
                    cardGap: Metrics.gap(root.width)
                    enabledRow: root.query.length < 2
                    reserveWhenEmpty: root.query.length < 2 && root.suggestionsBusy
                    loading: root.suggestionsBusy
                    emptyText: "Loading suggestions..."
                    visible: root.query.length < 2
                    onActivated: index => {
                        if (root.shell) {
                            root.shell.lastSearchKind = "suggestions"
                            root.shell.lastSearchIndex = index
                            root.shell.openDetailsAt(model, index, "suggestion", "search")
                        }
                    }
                }

                EmptyPlaceholder {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: root.query.length < 2 && !root.showSuggestions && !root.suggestionsBusy
                    title: "Start typing"
                    detail: "Enter at least two characters."
                }
            }

            Flickable {
                id: resultsScroller

                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: width
                contentHeight: resultColumn.implicitHeight
                boundsBehavior: Flickable.StopAtBounds

                FastWheelHandler {
                    flickable: resultsScroller
                }

                Column {
                    id: resultColumn

                    width: resultsScroller.width
                    spacing: Metrics.sectionGap(root.width)

                    Repeater {
                        id: resultRepeater

                        model: root.resultSections

                        MediaRow {
                            required property var modelData
                            readonly property string resultKind: modelData.key

                            width: resultColumn.width
                            title: modelData.title
                            model: modelData.model
                            shell: root.shell
                            cardKind: "poster"
                            useSeriesPoster: resultKind === "episodes"
                            preferEpisodeTitle: resultKind === "episodes"
                            cardWidth: Metrics.homePosterWidth(root.width)
                            cardGap: Metrics.gap(root.width)
                            onCurrentIndexChanged: if (activeFocus && root.shell) {
                                root.shell.lastSearchKind = resultKind
                                root.shell.lastSearchIndex = currentIndex
                            }
                            onActivated: root.activateResult(this)
                        }
                    }

                    EmptyPlaceholder {
                        width: resultColumn.width
                        height: resultsScroller.height
                        visible: root.query.length >= 2 && root.resultCount === 0 && !root.searchBusy
                        title: "No results"
                        detail: "Try another movie, series, or episode title."
                    }
                }
            }
        }
    }
}
