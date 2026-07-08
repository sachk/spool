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
    readonly property int resultCount: search && search.results ? search.results.count : 0
    readonly property int suggestionCount: search && search.suggestions ? search.suggestions.count : 0
    readonly property bool searchBusy: search ? search.busy : false
    readonly property bool suggestionsBusy: search ? search.suggestionsBusy : false
    readonly property bool showSuggestions: query.length < 2 && suggestionCount > 0
    focus: true

    Component.onCompleted: {
        if (root.search)
            root.search.loadSuggestions()
        field.text = root.query
        root.clampResultIndex()
        field.focusField()
    }

    onResultCountChanged: clampResultIndex()

    Connections {
        target: root.search
        function onQueryChanged() {
            if (field.text !== root.query)
                field.text = root.query
        }
        function onResultsChanged() {
            root.clampResultIndex()
        }
    }

    function clampResultIndex() {
        if (!results)
            return
        const savedIndex = root.shell ? root.shell.lastSearchIndex : 0
        results.currentIndex = root.resultCount > 0 ? Math.max(0, Math.min(savedIndex, root.resultCount - 1)) : -1
    }

    function activateSuggestion(index) {
        if (!root.search || !root.shell || index < 0 || suggestionCount <= 0)
            return
        root.shell.openDetailsAt(root.search.suggestions, index, "suggestion", "search")
    }

    function setQuery(text) {
        if (root.shell)
            root.shell.lastSearchIndex = 0
        if (root.search)
            root.search.setQuery(text)
    }

    function runSearchNow() {
        if (root.search)
            root.search.submit()
    }

    function activateCurrent() {
        if (!root.search || !root.shell || results.currentIndex < 0 || resultCount <= 0)
            return
        root.shell.lastSearchIndex = results.currentIndex
        root.shell.openDetailsAt(root.search.results, results.currentIndex, "search", "search")
    }

    function currentResultRow() {
        return results.currentItem
    }

    function focusFieldForTyping() {
        field.focusField()
    }

    function focusResultsOrSuggestions() {
        if (resultCount > 0) {
            InputKeys.focus(results)
            results.currentIndex = Math.max(0, results.currentIndex)
            return true
        }
        if (showSuggestions) {
            suggestionsRow.focusList()
            return true
        }
        return false
    }

    function handleBack() {
        if (results.activeFocus || suggestionsRow.activeFocus) {
            focusFieldForTyping()
            return true
        }
        return false
    }

    function handlePressedKey(key) {
        if (suggestionsRow.activeFocus)
            return suggestionsRow.handlePressedKey ? suggestionsRow.handlePressedKey(key) : false
        if (!results.activeFocus)
            return false
        const row = currentResultRow()
        return row && row.handleAcceptPressed ? row.handleAcceptPressed(key) : false
    }

    function handleKey(key) {
        const acceptKey = InputKeys.isAccept(key)
        if (suggestionsRow.activeFocus) {
            if (suggestionsRow.handleKey(key))
                return true
            if (key === Qt.Key_Up) {
                focusFieldForTyping()
                return true
            }
            if (key === Qt.Key_Down)
                return true
            return false
        }
        if (results.activeFocus && !acceptKey) {
            const menuRow = currentResultRow()
            if (menuRow && menuRow.handleKey && menuRow.handleKey(key))
                return true
        }
        if (key === Qt.Key_Up && field.activeFocus && !field.editing) {
            if (root.shell)
                root.shell.focusNavBar()
            return true
        }
        if (key === Qt.Key_Down && field.activeFocus)
            return focusResultsOrSuggestions()
        if (key === Qt.Key_Up && results.activeFocus && results.currentIndex <= 0) {
            focusFieldForTyping()
            return true
        }
        if (acceptKey && results.activeFocus) {
            const row = currentResultRow()
            if (row && row.handleAcceptReleased && row.handleAcceptReleased(key))
                return true
            if (row && row.handleKey && row.handleKey(key))
                return true
            activateCurrent()
            return true
        }
        return false
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin(width)
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            SectionHeader {
                Layout.fillWidth: true
                title: "Search"
            }

            BusyIndicator {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
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
            spacing: 18

            ColumnLayout {
                Layout.preferredWidth: Math.min(560, Math.max(360, root.width * 0.34))
                Layout.fillHeight: true
                spacing: 14

                TextFieldRow {
                    id: field
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    label: "Search Jellyfin"
                    placeholderText: "Titles, series, episodes"
                    inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                    onTextEdited: text => root.setQuery(text)
                    onAccepted: root.runSearchNow()
                    KeyNavigation.down: root.resultCount > 0 ? results : suggestionsRow
                }

                MediaPosterScrollerRow {
                    id: suggestionsRow
                    Layout.fillWidth: true
                    Layout.preferredHeight: implicitHeight
                    title: "Suggestions"
                    rowModel: root.search ? root.search.suggestions : null
                    shell: root.shell
                    cardWidth: Math.min(176, Math.max(132, root.width * 0.096))
                    enabledRow: root.query.length < 2
                    reserveWhenEmpty: root.query.length < 2 && root.suggestionsBusy
                    loading: root.suggestionsBusy
                    emptyText: "Loading suggestions..."
                    visible: root.query.length < 2
                    onActivated: index => root.activateSuggestion(index)
                }

                EmptyPlaceholder {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: root.query.length < 2 && !root.showSuggestions && !root.suggestionsBusy
                    title: "Start typing"
                    detail: "Enter at least two characters."
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                ListView {
                    id: results
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    focus: false
                    keyNavigationEnabled: false
                    currentIndex: root.resultCount > 0 ? Math.max(0, Math.min(root.shell ? root.shell.lastSearchIndex :
                                                                                           0, root.resultCount - 1)) :
                                                         -1
                    spacing: 10
                    clip: true
                    model: root.search ? root.search.results : null
                    KeyNavigation.up: field
                    visible: root.resultCount > 0
                    onCurrentIndexChanged: {
                        if (currentIndex >= 0) {
                            positionViewAtIndex(currentIndex, ListView.Contain)
                            if (activeFocus && root.shell)
                                root.shell.lastSearchIndex = currentIndex
                        }
                    }

                    FastWheelHandler {
                        flickable: results
                    }

                    delegate: Surface {
                        id: resultDelegate
                        required property int index
                        required property var item
                        required property string displayTitle
                        required property string displaySubtitle
                        readonly property var movie: item || ({})

                        width: results.width
                        height: 118
                        focused: ListView.isCurrentItem && results.activeFocus
                        function handleAcceptPressed(key) {
                            return actions.handleAcceptPressed(key)
                        }
                        function handleAcceptReleased(key) {
                            return actions.handleAcceptReleased(key)
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 14

                            ImageCard {
                                Layout.preferredWidth: 154
                                Layout.preferredHeight: 86
                                imageUrl: resultDelegate.movie.posterUrl || ""
                                fallbackText: (resultDelegate.movie.itemType || "").length > 0
                                              ? resultDelegate.movie.itemType : "Item"
                                aspectRatio: 16 / 9
                                focused: false
                                retainWhileLoading: true
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4

                                AppText {
                                    Layout.fillWidth: true
                                    text: displayTitle || resultDelegate.movie.title || ""
                                    font.pixelSize: Metrics.bodyPx(root.width) + 1
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                }

                                TechMetadataLine {
                                    Layout.fillWidth: true
                                    metadata: displaySubtitle || resultDelegate.movie.subtitle
                                              || resultDelegate.movie.itemType || ""
                                }

                                AppText {
                                    Layout.fillWidth: true
                                    visible: (resultDelegate.movie.overview || "").length > 0
                                    text: resultDelegate.movie.overview || ""
                                    color: Theme.textMuted
                                    font.pixelSize: Metrics.metaPx(root.width)
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                }
                            }

                            MetadataChip {
                                text: Number(resultDelegate.movie.year || 0) > 0 ? String(resultDelegate.movie.year) :
                                                                                   resultDelegate.movie.itemType || ""
                            }
                        }

                        MediaItemActions {
                            id: actions
                            anchors.fill: parent
                            shell: root.shell
                            focused: resultDelegate.focused
                            item: resultDelegate.movie
                            itemProvider: function () {
                                return root.search && root.search.results ? root.search.results.get(index) || ({}) : (
                                                                                {})
                            }
                            onActivated: {
                                results.currentIndex = index
                                root.activateCurrent()
                            }
                            onFavoriteToggled: favorite => App.setFavorite(resultDelegate.movie.movieId || "", favorite)
                            onPlayedToggled: played => App.setPlayed(resultDelegate.movie.movieId || "", played)
                            onMediaInfoRequested: root.shell.openMediaInfo(root.search.results.get(index) || ({}))
                        }
                    }

                    Keys.onReleased: event => {
                                         if (event.key === Qt.Key_Up && currentIndex <= 0) {
                                             root.focusFieldForTyping()
                                             event.accepted = true
                                         } else if (InputKeys.isAccept(event.key)) {
                                             const row = root.currentResultRow()
                                             if (row && row.handleAcceptReleased && row.handleAcceptReleased(
                                                     event.key)) {
                                                 event.accepted = true
                                                 return
                                             }
                                             root.activateCurrent()
                                             event.accepted = true
                                         } else if (event.key === Qt.Key_M && currentIndex >= 0) {
                                             root.shell.openMediaInfo(root.search.results.get(currentIndex))
                                             event.accepted = true
                                         }
                                     }
                }

                EmptyPlaceholder {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: root.query.length >= 2 && root.resultCount === 0 && !root.searchBusy
                    title: "No results"
                    detail: "Try another title, series, or episode name."
                }
            }
        }
    }
}
