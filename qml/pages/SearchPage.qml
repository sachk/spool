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

    function back() {
        if (!results.activeFocus && !suggestionsRow.activeFocus)
            return false
        focusFieldForTyping()
        return true
    }

    function routeKey(key, phase, repeat) {
        if (suggestionsRow.activeFocus) {
            if (key === Qt.Key_Up && phase === "press") {
                focusFieldForTyping()
                return true
            }
            if (key === Qt.Key_Down)
                return true
            return suggestionsRow.routeKey(key, phase, repeat)
        }
        if (results.activeFocus) {
            if (key === Qt.Key_Up) {
                if (results.currentIndex <= 0)
                    focusFieldForTyping()
                else
                    --results.currentIndex
                return true
            }
            if (key === Qt.Key_Down) {
                results.currentIndex = Math.min(resultCount - 1, results.currentIndex + 1)
                return true
            }
            return InputKeys.isHorizontal(key)
        }
        if (key === Qt.Key_Up && field.activeFocus && !field.editing) {
            if (shell)
                shell.focusNavBar()
            return true
        }
        return key === Qt.Key_Down && field.activeFocus && focusResultsOrSuggestions()
    }

    function activate() {
        if (suggestionsRow.activeFocus)
            suggestionsRow.activate()
        else if (results.activeFocus)
            activateCurrent()
    }

    function longPress() {
        if (suggestionsRow.activeFocus)
            return suggestionsRow.longPress()
        if (!results.activeFocus || results.currentIndex < 0 || !shell)
            return false
        return shell.openItemMenu(search.results.get(results.currentIndex) || ({}), currentResultRow())
    }

    // Height of the on-screen keyboard overlapping this page, so the layout
    // can keep the field and results visible above the panel.
    readonly property real keyboardInset: {
        if (!Qt.inputMethod.visible)
        return 0
        const kb = Qt.inputMethod.keyboardRectangle
        if (kb.height <= 0)
        return 0
        const pageBottom = root.mapToItem(null, 0, root.height).y
        return Math.max(0, Math.min(root.height, pageBottom - kb.y))
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin(width)
        anchors.bottomMargin: Metrics.pageMargin(width) + root.keyboardInset
        spacing: 16

        // The TV keyboard re-maps its panel when the prediction row toggles,
        // briefly reporting hidden -> shown; smooth the inset so the page
        // does not bounce while typing.
        Behavior on anchors.bottomMargin {
            NumberAnimation {
                duration: 120
                easing.type: Easing.OutCubic
            }
        }

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
                // Nested layouts default to fillWidth: true, which makes this
                // column race the results column for surplus width and win
                // ~proportionally to preferred sizes, squeezing the results
                // into a sliver. Pin it to its preferred width instead.
                Layout.fillWidth: false
                Layout.preferredWidth: Math.min(560, Math.max(360, root.width * 0.34))
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignTop
                spacing: 14

                TextFieldRow {
                    id: field
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    label: "Search Jellyfin"
                    placeholderText: "Titles, series, episodes"
                    inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                    enterKeyType: Qt.EnterKeySearch
                    onTextEdited: text => root.setQuery(text)
                    onAccepted: root.runSearchNow()
                }

                MediaRow {
                    id: suggestionsRow
                    Layout.fillWidth: true
                    Layout.preferredHeight: implicitHeight
                    title: "Suggestions"
                    model: root.search ? root.search.suggestions : null
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

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 14

                            ImageCard {
                                Layout.preferredWidth: 154
                                Layout.preferredHeight: 86
                                imageUrl: Art.url(resultDelegate.movie, "landscape", 154)
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
                            item: resultDelegate.movie
                            onActivated: {
                                results.currentIndex = index
                                root.activateCurrent()
                            }
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
