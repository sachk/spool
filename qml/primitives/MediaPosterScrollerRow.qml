import QtQuick
import "../theme"

FocusScope {
    id: root

    property string title: ""
    property var rowModel
    property var shell
    property int cardWidth: 156
    property int currentIndex: 0
    property int rowGap: 16
    property bool enabledRow: true
    property bool reserveWhenEmpty: false
    property bool loading: false
    property string emptyText: "Loading..."
    property string cardKind: "poster"
    property bool useSeriesPoster: true
    property bool preferEpisodeTitle: false
    readonly property int rowCount: modelCount()
    readonly property int headerHeight: 34
    readonly property int cardHeight: Math.round(cardWidth * (cardKind === "landscape" ? 9 / 16 : 1.5) + 60)
    readonly property bool rowVisible: enabledRow && (rowCount > 0 || reserveWhenEmpty)

    signal activated(int index)

    width: parent ? parent.width : implicitWidth
    height: rowVisible ? headerHeight + 10 + cardHeight : 0
    implicitHeight: height
    visible: rowVisible
    focus: true
    clip: false

    function focusList() {
        if (rowCount <= 0)
            return false
        InputKeys.focus(listView)
        listView.currentIndex = rowCount > 0 ? Math.max(0, Math.min(currentIndex, rowCount - 1)) : -1
        ensureVisible()
        return true
    }

    function modelCount() {
        if (!rowModel)
            return 0
        if (rowModel.count !== undefined)
            return rowModel.count
        if (rowModel.rowCount)
            return rowModel.rowCount()
        return 0
    }

    function ensureVisible() {
        if (listView.currentIndex >= 0)
            listView.positionViewAtIndex(listView.currentIndex, ListView.Contain)
    }

    function currentCard() {
        return listView.currentItem
    }

    function handlePressedKey(key) {
        const card = currentCard()
        return card && card.handleAcceptPressed ? card.handleAcceptPressed(key) : false
    }

    function handleKey(key) {
        if (rowCount <= 0)
            return false
        const acceptKey = InputKeys.isAccept(key)
        const card = currentCard()
        if (key === Qt.Key_Left) {
            if (listView.currentIndex > 0)
                listView.currentIndex = listView.currentIndex - 1
            currentIndex = listView.currentIndex
            ensureVisible()
            return true
        }
        if (key === Qt.Key_Right) {
            listView.currentIndex = Math.min(rowCount - 1, listView.currentIndex + 1)
            currentIndex = listView.currentIndex
            ensureVisible()
            return true
        }
        if (acceptKey) {
            currentIndex = listView.currentIndex
            if (card && card.handleAcceptReleased && card.handleAcceptReleased(key))
                return true
            activated(listView.currentIndex)
            return true
        }
        return false
    }

    SectionHeader {
        id: rowHeader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.headerHeight
        title: root.title
    }

    ListView {
        id: listView
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: rowHeader.bottom
        anchors.topMargin: 10
        height: root.cardHeight
        visible: root.rowCount > 0
        focus: true
        keyNavigationEnabled: false
        clip: true
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds
        spacing: root.rowGap
        model: root.rowModel
        currentIndex: root.rowCount > 0 ? Math.max(0, Math.min(root.currentIndex, root.rowCount - 1)) : -1
        onCurrentIndexChanged: {
            root.currentIndex = currentIndex
            root.ensureVisible()
        }
        FastWheelHandler {
            flickable: listView
            horizontal: true
        }

        delegate: Item {
            id: posterDelegate
            required property int index
            required property var item
            required property string displayTitle
            required property string displaySubtitle
            required property real progress
            readonly property var movie: item || ({})
            width: root.cardWidth
            height: listView.height

            function handleAcceptPressed(key) {
                return card.handleAcceptPressed(key)
            }
            function handleAcceptReleased(key) {
                return card.handleAcceptReleased(key)
            }

            MediaItemCard {
                id: card
                anchors.fill: parent
                shell: root.shell
                kind: root.cardKind
                useSeriesPoster: root.useSeriesPoster
                preferEpisodeTitle: root.preferEpisodeTitle
                focused: posterDelegate.index === listView.currentIndex && listView.activeFocus
                itemProvider: function () {
                    return root.rowModel.get(posterDelegate.index) || ({})
                }
                item: posterDelegate.movie
                displayTitle: posterDelegate.displayTitle
                displaySubtitle: posterDelegate.displaySubtitle
                progress: posterDelegate.progress
                onActivated: {
                    listView.currentIndex = posterDelegate.index
                    root.currentIndex = posterDelegate.index
                    root.activated(posterDelegate.index)
                }
                onFavoriteToggled: favorite => App.setFavorite(posterDelegate.movie.movieId || "", favorite)
                onPlayedToggled: played => App.setPlayed(posterDelegate.movie.movieId || "", played)
                onMediaInfoRequested: {
                    if (root.shell)
                        root.shell.openMediaInfo(root.rowModel.get(posterDelegate.index) || ({}))
                }
            }
        }

        // Direction keys are dispatched on press by the shell; handling them
        // here too moved the selection twice per key tap. Only accept lands here.
        Keys.onReleased: event => {
                             if (!InputKeys.isAccept(event.key))
                             return
                             event.accepted = event.isAutoRepeat || root.handleKey(event.key)
                         }
    }

    MonoText {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: rowHeader.bottom
        anchors.topMargin: 18
        height: root.cardHeight
        visible: root.rowCount <= 0 && root.reserveWhenEmpty
        text: root.loading ? root.emptyText : ""
        color: Theme.textMuted
        font.pixelSize: Metrics.metaPx(root.Window.window ? root.Window.window.width : 1920)
        verticalAlignment: Text.AlignTop
    }
}
