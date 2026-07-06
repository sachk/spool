import QtQuick
import QtQuick.Layouts
import "../theme"

FocusScope {
    id: root

    property string title: ""
    property var rowModel
    property var shell
    property string rowKind: "landscape"
    property bool useSeriesPoster: false
    property int cardWidth: 240
    property int cardGap: 16
    property int currentIndex: rowCount > 0 ? 0 : -1
    readonly property int rowCount: rowModel ? rowModel.count : 0
    readonly property int visibleCount: rowCount
    readonly property bool libraryRow: rowKind === "library"
    readonly property bool rowVisible: visibleCount > 0

    signal activated(int index)
    signal moveVertical(int direction)
    signal favoriteToggled(int index, bool favorite)
    signal playedToggled(int index, bool played)
    signal mediaInfoRequested(int index)

    focus: true
    onActiveFocusChanged: if (activeFocus) rowList.forceActiveFocus()
    onCurrentIndexChanged: {
        if (rowList.currentIndex !== currentIndex)
            rowList.currentIndex = currentIndex
        ensureVisible()
    }
    onVisibleCountChanged: {
        currentIndex = visibleCount > 0 ? Math.max(0, Math.min(currentIndex, visibleCount - 1)) : -1
    }

    function itemAt(index) {
        return rowModel && index >= 0 && index < rowCount ? (rowModel.get(index) || ({})) : ({})
    }

    function currentCard() {
        return rowList.currentItem
    }

    function focusList() {
        rowList.forceActiveFocus()
    }

    function ensureVisible() {
        if (rowList.currentIndex >= 0)
            rowList.positionViewAtIndex(rowList.currentIndex, ListView.Contain)
    }

    function activateCurrent() {
        if (currentIndex >= 0 && currentIndex < visibleCount)
            activated(currentIndex)
    }

    function handlePressedKey(key) {
        const card = currentCard()
        return card && card.handleAcceptPressed ? card.handleAcceptPressed(key) : false
    }

    function handleNavigationKey(key) {
        const acceptKey = InputKeys.isAccept(key, !libraryRow)
        const card = currentCard()
        if (!libraryRow && !acceptKey && card && card.handleNavigationKey
                && card.handleNavigationKey(key))
            return true
        if (key === Qt.Key_Left) {
            if (currentIndex > 0)
                currentIndex = currentIndex - 1
            return true
        }
        if (key === Qt.Key_Right) {
            if (visibleCount > 0)
                currentIndex = Math.min(visibleCount - 1, currentIndex + 1)
            return true
        }
        if (key === Qt.Key_Up) {
            moveVertical(-1)
            return true
        }
        if (key === Qt.Key_Down) {
            moveVertical(1)
            return true
        }
        if (acceptKey) {
            if (!libraryRow && card && card.handleAcceptReleased
                    && card.handleAcceptReleased(key))
                return true
            if (!libraryRow && card && card.handleNavigationKey
                    && card.handleNavigationKey(key))
                return true
            activateCurrent()
            return true
        }
        return false
    }

    Component {
        id: mediaCardDelegate

        Item {
            id: mediaDelegate

            required property int index
            required property string movieId
            required property string displayTitle
            required property string displaySubtitle
            required property string posterUrl
            required property string seriesPosterUrl
            required property string thumbUrl
            required property string landscapeCardUrl
            required property string backdropUrl
            required property real progress
            required property string itemType
            required property string seriesId
            required property string seasonId
            required property bool favorite
            required property bool played
            required property real resumeTicks
            required property bool playable
            required property int year
            required property string subtitle
            required property string title
            required property string seriesName
            required property int seasonNumber

            width: root.cardWidth
            height: rowList.height

            function handleAcceptPressed(key) { return mediaCard.handleAcceptPressed(key) }
            function handleAcceptReleased(key) { return mediaCard.handleAcceptReleased(key) }
            function handleNavigationKey(key) { return mediaCard.handleNavigationKey(key) }

            MediaItemCard {
                id: mediaCard
                anchors.fill: parent
                shell: root.shell
                kind: root.rowKind === "poster" ? "poster" : "landscape"
                useSeriesPoster: root.useSeriesPoster
                focused: mediaDelegate.index === rowList.currentIndex && rowList.activeFocus
                snapshotProvider: function() { return root.itemAt(mediaDelegate.index) }
                movieId: mediaDelegate.movieId
                displayTitle: mediaDelegate.displayTitle
                displaySubtitle: mediaDelegate.displaySubtitle
                posterUrl: mediaDelegate.posterUrl
                seriesPosterUrl: mediaDelegate.seriesPosterUrl
                thumbUrl: mediaDelegate.thumbUrl
                landscapeCardUrl: mediaDelegate.landscapeCardUrl
                backdropUrl: mediaDelegate.backdropUrl
                progress: mediaDelegate.progress
                itemType: mediaDelegate.itemType
                seriesId: mediaDelegate.seriesId
                seasonId: mediaDelegate.seasonId
                favorite: mediaDelegate.favorite
                played: mediaDelegate.played
                resumeTicks: mediaDelegate.resumeTicks
                playable: mediaDelegate.playable
                year: mediaDelegate.year
                subtitle: mediaDelegate.subtitle
                title: mediaDelegate.title
                seriesName: mediaDelegate.seriesName
                seasonNumber: mediaDelegate.seasonNumber
                onActivated: root.activated(mediaDelegate.index)
                onFavoriteToggled: (favorite) => root.favoriteToggled(mediaDelegate.index, favorite)
                onPlayedToggled: (played) => root.playedToggled(mediaDelegate.index, played)
                onMediaInfoRequested: root.mediaInfoRequested(mediaDelegate.index)
            }
        }
    }

    Component {
        id: libraryCardDelegate

        Item {
            id: libraryDelegate
            required property int index
            required property string name
            required property string collectionType
            required property string imageUrl
            readonly property bool current: index === rowList.currentIndex && rowList.activeFocus

            width: root.cardWidth
            height: rowList.height

            LandscapeCard {
                anchors.fill: parent
                title: libraryDelegate.name || ""
                subtitle: libraryDelegate.collectionType || ""
                imageUrl: libraryDelegate.imageUrl || ""
                focused: libraryDelegate.current
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    root.currentIndex = libraryDelegate.index
                    root.activated(libraryDelegate.index)
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        SectionHeader {
            Layout.fillWidth: true
            title: root.title
        }

        ListView {
            id: rowList

            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: root.focus
            clip: true
            orientation: ListView.Horizontal
            keyNavigationEnabled: false
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
            spacing: root.cardGap
            cacheBuffer: Math.round(2 * (root.cardWidth + root.cardGap))
            model: root.rowModel
            delegate: root.libraryRow ? libraryCardDelegate : mediaCardDelegate
            currentIndex: root.currentIndex
            onCurrentIndexChanged: {
                root.currentIndex = currentIndex
                root.ensureVisible()
            }

            FastWheelHandler {
                flickable: rowList
                horizontal: true
            }

            Keys.onReleased: (event) => {
                if (InputKeys.isAccept(event.key, !root.libraryRow)) {
                    if (!root.libraryRow) {
                        const card = root.currentCard()
                        if (card && card.handleAcceptReleased
                                && card.handleAcceptReleased(event.key)) {
                            event.accepted = true
                            return
                        }
                    }
                    root.activateCurrent()
                    event.accepted = true
                } else if (event.key === Qt.Key_M && !root.libraryRow) {
                    root.mediaInfoRequested(root.currentIndex)
                    event.accepted = true
                }
            }
        }
    }
}
