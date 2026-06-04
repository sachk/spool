import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    property var shell
    property string query: appController ? appController.searchQuery : ""
    property int resultCount: appController && appController.searchResults ? appController.searchResults.rowCount() : 0
    property int suggestionCount: appController && appController.searchSuggestions ? appController.searchSuggestions.rowCount() : 0
    readonly property bool showSuggestions: query.length < 2 && suggestionCount > 0
    focus: true

    Component.onCompleted: {
        field.text = root.query
        refreshResultCount()
        refreshSuggestionCount()
        if (appController)
            appController.loadSearchSuggestions()
        field.focusRow()
    }

    Connections {
        target: appController
        function onSearchChanged() {
            root.query = appController.searchQuery
            root.refreshResultCount()
        }
        function onSearchSuggestionsChanged() {
            root.refreshSuggestionCount()
        }
    }

    Connections {
        target: appController ? appController.searchSuggestions : null
        function onModelReset() { root.refreshSuggestionCount() }
        function onRowsInserted() { root.refreshSuggestionCount() }
        function onRowsRemoved() { root.refreshSuggestionCount() }
    }

    Connections {
        target: appController ? appController.searchResults : null
        function onModelReset() { root.refreshResultCount() }
        function onRowsInserted() { root.refreshResultCount() }
        function onRowsRemoved() { root.refreshResultCount() }
    }

    Timer {
        id: searchTimer
        interval: 260
        repeat: false
        onTriggered: appController.search(root.query)
    }

    function refreshResultCount() {
        resultCount = appController && appController.searchResults ? appController.searchResults.rowCount() : 0
        if (results)
            results.currentIndex = resultCount > 0 ? Math.max(0, Math.min(shell.lastSearchIndex, resultCount - 1)) : -1
    }

    function refreshSuggestionCount() {
        suggestionCount = appController && appController.searchSuggestions ? appController.searchSuggestions.rowCount() : 0
    }

    function activateSuggestion(index) {
        if (index < 0 || suggestionCount <= 0)
            return
        shell.openDetails(appController.searchSuggestions, index, "suggestion", "search")
    }

    function setQuery(text) {
        query = String(text || "").trim()
        shell.lastSearchIndex = 0
        if (query.length < 2) {
            searchTimer.stop()
            appController.search(query)
            return
        }
        searchTimer.restart()
    }

    function runSearchNow() {
        searchTimer.stop()
        if (query.length >= 2)
            appController.search(query)
    }

    function activateCurrent() {
        if (results.currentIndex < 0 || resultCount <= 0)
            return
        shell.lastSearchIndex = results.currentIndex
        shell.openDetails(appController.searchResults, results.currentIndex, "search", "search")
    }

    function currentResultRow() {
        return results.currentItem
    }

    function handlePressedKey(key) {
        if (suggestionsRow.activeFocus)
            return suggestionsRow.handlePressedKey ? suggestionsRow.handlePressedKey(key) : false
        if (!results.activeFocus)
            return false
        const row = currentResultRow()
        return row && row.handleAcceptPressed ? row.handleAcceptPressed(key) : false
    }

    function handleNavigationKey(key) {
        const acceptKey = key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select || key === Qt.Key_Space
        if (suggestionsRow.activeFocus) {
            if (suggestionsRow.handleNavigationKey(key))
                return true
            if (key === Qt.Key_Up) { field.focusRow(); return true }
            if (key === Qt.Key_Down) return true
            return false
        }
        if (results.activeFocus && !acceptKey) {
            const menuRow = currentResultRow()
            if (menuRow && menuRow.handleNavigationKey && menuRow.handleNavigationKey(key))
                return true
        }
        if (key === Qt.Key_Up && field.activeFocus && !field.editing) { shell.focusNavBar(); return true }
        if (key === Qt.Key_Down && field.activeFocus) {
            if (resultCount > 0) {
                results.forceActiveFocus()
                results.currentIndex = Math.max(0, results.currentIndex)
            } else if (showSuggestions) {
                suggestionsRow.focusList()
            }
            return true
        }
        if (key === Qt.Key_Up && results.activeFocus && results.currentIndex <= 0) {
            field.focusRow()
            return true
        }
        if (acceptKey && results.activeFocus) {
            const row = currentResultRow()
            if (row && row.handleAcceptReleased && row.handleAcceptReleased(key))
                return true
            if (row && row.handleNavigationKey && row.handleNavigationKey(key))
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
                running: appController && appController.searchBusy
                visible: running
            }

            MonoText {
                visible: root.query.length >= 2 && !appController.searchBusy
                text: root.resultCount + " result" + (root.resultCount === 1 ? "" : "s")
                color: Theme.textMuted
            }
        }

        TextFieldRow {
            id: field
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            label: "Search Jellyfin"
            placeholderText: "Titles, series, episodes"
            inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            onTextEdited: (text) => root.setQuery(text)
            onAccepted: root.runSearchNow()
            KeyNavigation.down: results
        }

        MediaPosterScrollerRow {
            id: suggestionsRow
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            title: "Suggestions"
            rowModel: appController ? appController.searchSuggestions : null
            shell: root.shell
            cardWidth: Math.min(176, Math.max(132, root.width * 0.096))
            enabledRow: root.query.length < 2
            reserveWhenEmpty: root.query.length < 2 && appController && appController.searchSuggestionsBusy
            loading: appController && appController.searchSuggestionsBusy
            emptyText: "Loading suggestions..."
            onActivated: (index) => root.activateSuggestion(index)
        }

        ListView {
            id: results
            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: false
            keyNavigationEnabled: false
            currentIndex: root.resultCount > 0 ? Math.max(0, Math.min(shell.lastSearchIndex, root.resultCount - 1)) : -1
            spacing: 10
            clip: true
            model: appController.searchResults
            KeyNavigation.up: field
            visible: root.resultCount > 0
            onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)

            FastWheelHandler { flickable: results }

            delegate: Surface {
                id: resultDelegate
                required property int index
                required property string title
                required property string displayTitle
                required property string displaySubtitle
                required property string subtitle
                required property string overview
                required property string posterUrl
                required property string itemType
                required property int year
                required property string movieId
                required property bool favorite
                required property bool played
                readonly property var itemData: appController.searchResults.get(index)

                width: results.width
                height: 118
                focused: ListView.isCurrentItem && results.activeFocus

                function handleAcceptPressed(key) { return actions.handleAcceptPressed(key) }
                function handleAcceptReleased(key) { return actions.handleAcceptReleased(key) }
                function handleNavigationKey(key) { return actions.handleNavigationKey(key) }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 14

                    ImageCard {
                        Layout.preferredWidth: 154
                        Layout.preferredHeight: 86
                        imageUrl: posterUrl
                        fallbackText: itemType.length > 0 ? itemType : "Item"
                        aspectRatio: 16 / 9
                        focused: false
                        retainWhileLoading: true
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        AppText {
                            Layout.fillWidth: true
                            text: displayTitle || title
                            font.pixelSize: Metrics.bodyPx(root.width) + 1
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            maximumLineCount: 1
                        }

                        TechMetadataLine {
                            Layout.fillWidth: true
                            metadata: displaySubtitle || subtitle || itemType
                        }

                        AppText {
                            Layout.fillWidth: true
                            visible: overview.length > 0
                            text: overview
                            color: Theme.textMuted
                            font.pixelSize: Metrics.metaPx(root.width)
                            elide: Text.ElideRight
                            maximumLineCount: 1
                        }
                    }

                    MetadataChip {
                        text: year > 0 ? String(year) : itemType
                    }
                }

                MediaItemActions {
                    id: actions
                    anchors.fill: parent
                    item: resultDelegate.itemData
                    focused: resultDelegate.focused
                    onActivated: {
                        results.currentIndex = index
                        root.activateCurrent()
                    }
                    onFavoriteToggled: (favorite) => appController.setFavorite(resultDelegate.movieId || "", favorite)
                    onPlayedToggled: (played) => appController.setPlayed(resultDelegate.movieId || "", played)
                    onMediaInfoRequested: shell.openMediaInfo(resultDelegate.itemData)
                }
            }

            Keys.onReleased: (event) => {
                if (event.key === Qt.Key_Up && currentIndex <= 0) {
                    field.focusRow()
                    event.accepted = true
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Select || event.key === Qt.Key_Space) {
                    const row = root.currentResultRow()
                    if (row && row.handleAcceptReleased && row.handleAcceptReleased(event.key)) {
                        event.accepted = true
                        return
                    }
                    root.activateCurrent()
                    event.accepted = true
                } else if (event.key === Qt.Key_M && currentIndex >= 0) {
                    shell.openMediaInfo(appController.searchResults.get(currentIndex))
                    event.accepted = true
                }
            }
        }

        EmptyPlaceholder {
            Layout.fillWidth: true
            visible: root.resultCount === 0 && !(appController && appController.searchBusy)
                     && !root.showSuggestions
                     && !(root.query.length < 2 && appController && appController.searchSuggestionsBusy)
            title: root.query.length < 2 ? "Start typing" : "No results"
            detail: root.query.length < 2 ? "Enter at least two characters." : "Try another title, series, or episode name."
        }
    }
}
