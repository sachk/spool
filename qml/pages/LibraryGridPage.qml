import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    property var shell
    property int columns: Metrics.columns(width)
    focus: true
    Component.onCompleted: grid.forceActiveFocus()
    onActiveFocusChanged: if (activeFocus) grid.forceActiveFocus()

    function activateCurrent() {
        if (grid.currentIndex < 0)
            return
        shell.lastGridIndex = grid.currentIndex
        const item = appController.movies.get(grid.currentIndex)
        if (item.itemType === "Season") {
            appController.playMovie(grid.currentIndex)
            return
        }
        shell.openDetails(appController.movies, grid.currentIndex, "movies", "libraryGrid")
    }

    function currentCard() {
        return grid.currentItem
    }

    function handlePressedKey(key) {
        const card = currentCard()
        return card && card.handleAcceptPressed ? card.handleAcceptPressed(key) : false
    }

    function handleNavigationKey(key) {
        if (grid.count <= 0)
            return false
        const acceptKey = key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select || key === Qt.Key_Space
        const card = currentCard()
        if (!acceptKey && card && card.handleNavigationKey && card.handleNavigationKey(key))
            return true
        if (key === Qt.Key_Left) {
            if (grid.currentIndex % columns === 0) shell.focusRail()
            else grid.currentIndex = Math.max(0, grid.currentIndex - 1)
            return true
        }
        if (key === Qt.Key_Right) {
            grid.currentIndex = Math.min(grid.count - 1, grid.currentIndex + 1)
            return true
        }
        if (key === Qt.Key_Up) {
            grid.currentIndex = Math.max(0, grid.currentIndex - columns)
            return true
        }
        if (key === Qt.Key_Down) {
            grid.currentIndex = Math.min(grid.count - 1, grid.currentIndex + columns)
            return true
        }
        if (acceptKey) {
            if (card && card.handleAcceptReleased && card.handleAcceptReleased(key))
                return true
            if (card && card.handleNavigationKey && card.handleNavigationKey(key))
                return true
            activateCurrent()
            return true
        }
        return false
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin(width)
        spacing: 12
        SectionHeader { Layout.fillWidth: true; title: appController.currentLibraryName.length > 0 ? appController.currentLibraryName : "Movies" }
        TechMetadataLine { Layout.fillWidth: true; metadata: grid.currentIndex >= 0 ? "Title · Year · Runtime · Rating · H.265 · HDR10 · DTS-HD MA 5.1" : "Technical metadata unavailable" }
        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: true
            clip: true
            keyNavigationEnabled: false
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
            model: appController.movies
            cellWidth: Math.floor((width - Metrics.gap(root.width) * (columns - 1)) / columns)
            cellHeight: cellWidth * 1.5 + 64
            Component.onCompleted: {
                restoreIndex()
                requestMoreIfNeeded()
            }
            onCountChanged: {
                restoreIndex()
                requestMoreIfNeeded()
            }
            onContentYChanged: loadMoreDebounce.restart()

            function restoreIndex() {
                currentIndex = count > 0 ? Math.max(0, Math.min(shell.lastGridIndex, count - 1)) : -1
            }

            function lastLikelyVisibleIndex() {
                if (count <= 0 || cellHeight <= 0 || columns <= 0)
                    return -1
                const firstRow = Math.max(0, Math.floor(contentY / cellHeight))
                const visibleRows = Math.ceil(height / cellHeight) + 3
                return Math.min(count - 1, (firstRow + visibleRows) * columns - 1)
            }

            function requestMoreIfNeeded() {
                if (!appController || count <= 0)
                    return
                const visibleTail = Math.max(currentIndex, lastLikelyVisibleIndex())
                appController.maybeLoadMoreCurrentItems(visibleTail)
            }

            Timer {
                id: loadMoreDebounce
                interval: 80
                repeat: false
                onTriggered: grid.requestMoreIfNeeded()
            }

            delegate: Item {
                id: gridDelegate
                required property int index
                required property string title
                required property string posterUrl
                required property string seriesPosterUrl
                required property int year
                required property string subtitle
                required property string displayTitle
                required property string displaySubtitle
                required property string movieId
                required property bool favorite
                required property bool played
                width: grid.cellWidth
                height: grid.cellHeight

                function handleAcceptPressed(key) { return card.handleAcceptPressed(key) }
                function handleAcceptReleased(key) { return card.handleAcceptReleased(key) }
                function handleNavigationKey(key) { return card.handleNavigationKey(key) }

                readonly property var itemData: appController.movies.get(index)

                MediaItemCard {
                    id: card
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.rightMargin: Metrics.gap(root.width)
                    height: parent.height
                    item: gridDelegate.itemData
                    kind: "poster"
                    focused: gridDelegate.GridView.isCurrentItem
                    onActivated: {
                        grid.currentIndex = index
                        shell.lastGridIndex = index
                        shell.lastGridY = grid.contentY
                        root.activateCurrent()
                    }
                    onFavoriteToggled: (favorite) => appController.setFavorite(gridDelegate.movieId || "", favorite)
                    onPlayedToggled: (played) => appController.setPlayed(gridDelegate.movieId || "", played)
                    onMediaInfoRequested: shell.openMediaInfo(gridDelegate.itemData)
                }
            }
            onCurrentIndexChanged: {
                shell.lastGridIndex = currentIndex
                loadMoreDebounce.restart()
            }
            Keys.onReleased: (event) => {
                if (event.key === Qt.Key_Left && currentIndex % columns === 0) { shell.focusRail(); event.accepted = true }
                else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Select || event.key === Qt.Key_Space) {
                    const card = root.currentCard()
                    if (card && card.handleAcceptReleased && card.handleAcceptReleased(event.key)) {
                        event.accepted = true
                        return
                    }
                    root.activateCurrent()
                    event.accepted = true
                }
                else if (event.key === Qt.Key_M) { shell.lastGridIndex = currentIndex; shell.openMediaInfo(appController.movies.get(currentIndex)); event.accepted = true }
            }
        }
    }
}
