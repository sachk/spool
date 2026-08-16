pragma ComponentBehavior: Bound

import QtQuick
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
        },
        {
            "key": "other",
            "title": "More",
            "model": search ? search.otherResults : null
        }
    ]
    property int currentSection: 0
    // Result kind the user last interacted with; picks the row to focus
    // when results rebuild. Page-local — the page is resident.
    property string preferredKind: "movies"

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
        for (let index = 0; index < resultsScroller.count; ++index) {
            const row = resultsScroller.itemAtIndex(index)
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
        for (let index = 0; index < rows.length; ++index) {
            if (rows[index].resultKind === root.preferredKind)
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
        preferredKind = row.resultKind
        const item = row.itemAt(row.currentIndex) || ({})
        if (String(item.itemType || "") === "Person") {
            shell.openPerson({
                                 "id": String(item.movieId || ""),
                                 "name": String(item.title || ""),
                                 "type": "Person"
                             })
            return
        }
        if (String(item.itemType || "") === "MusicAlbum") {
            shell.openDetailsAt(row.model, row.currentIndex, "album", "search")
            return
        }
        if (["Playlist", "Folder", "PhotoAlbum", "MusicArtist"].indexOf(String(item.itemType || "")) >= 0) {
            App.playFromModel(row.model, row.currentIndex)
            shell.pushRoute("libraryGrid")
            return
        }
        shell.openDetailsAt(row.model, row.currentIndex, "search", "search")
    }

    function setQuery(text) {
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
        if (key !== Qt.Key_Down || !(field.activeFocus || field.editing))
            return false
        if (query.length >= 2)
            return focusPreferredResult()
        return showSuggestions && suggestionsRow.focusList()
    }

    function activate() {
        if (field.activeFocus && !field.editing) {
            field.activate()
            return
        }
        if (suggestionsRow.activeFocus) {
            if (shell && suggestionsRow.currentIndex >= 0)
                shell.openDetailsAt(search.suggestions, suggestionsRow.currentIndex, "suggestion", "search")
            return
        }
        activateResult(activeResultRow())
    }

    function currentMediaItem() {
        if (suggestionsRow.activeFocus && suggestionsRow.currentIndex >= 0)
            return suggestionsRow.itemAt(suggestionsRow.currentIndex)
        const row = activeResultRow()
        if (row && row.activeFocus && row.currentIndex >= 0)
            return row.itemAt(row.currentIndex)
        return ({})
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
        anchors.margins: Metrics.pageMarginPx
        anchors.bottomMargin: Metrics.pageMarginPx + root.keyboardInset
        spacing: Metrics.gapPx

        Behavior on anchors.bottomMargin {
            enabled: !Theme.reducedMotion
            NumberAnimation {
                duration: 120
                easing.type: Easing.OutCubic
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Metrics.scaled(68)

            TextFieldRow {
                id: field

                anchors.fill: parent
                placeholderText: "Search everything"
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                enterKeyType: Qt.EnterKeySearch
                onTextEdited: text => root.setQuery(text)
                onAccepted: if (root.search)
                root.search.submit()
            }

            BusySpinner {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: Metrics.scaled(18)
                width: Metrics.scaled(28)
                height: width
                running: root.searchBusy
                visible: running
            }
        }

        MediaRow {
            id: suggestionsRow

            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            title: "Suggestions"
            model: root.search ? root.search.suggestions : null
            shell: root.shell
            cardWidth: Metrics.homePosterWidth(root.width)
            cardGap: Metrics.gapPx
            enabledRow: root.query.length < 2
            reserveWhenEmpty: root.query.length < 2 && root.suggestionsBusy
            loading: root.suggestionsBusy
            emptyText: "Loading suggestions..."
            visible: root.query.length < 2
            onActivated: index => {
                if (root.shell)
                    root.shell.openDetailsAt(model, index, "suggestion", "search")
            }
        }

        EmptyPlaceholder {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.query.length < 2 && !root.showSuggestions && !root.suggestionsBusy
            title: "Start typing"
            detail: "Enter at least two characters."
        }

        ListView {
            id: resultsScroller

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.query.length >= 2
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            spacing: Metrics.sectionGapPx
            reuseItems: true
            cacheBuffer: 0
            model: root.resultSections

            FastWheelHandler {
                flickable: resultsScroller
            }

            delegate: MediaRow {
                required property var modelData
                readonly property string resultKind: modelData.key

                width: resultsScroller.width
                title: modelData.title
                model: modelData.model
                shell: root.shell
                cardKind: "poster"
                useSeriesPoster: resultKind === "episodes"
                preferEpisodeTitle: resultKind === "episodes"
                cardWidth: Metrics.homePosterWidth(root.width)
                cardGap: Metrics.gapPx
                onCurrentIndexChanged: if (activeFocus)
                root.preferredKind = resultKind
                onActivated: root.activateResult(this)
            }

            footer: Item {
                width: resultsScroller.width
                height: resultsScroller.height
                visible: root.resultCount === 0 && !root.searchBusy

                AppText {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    text: "No Results"
                    font.pixelSize: Metrics.bodySizePx + 10
                    font.weight: Font.DemiBold
                }
            }
        }
    }
}
