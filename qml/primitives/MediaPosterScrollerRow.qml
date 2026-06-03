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
    property bool useSeriesPoster: true
    readonly property int rowCount: modelCount()
    readonly property int headerHeight: 34
    readonly property int cardHeight: Math.round(cardWidth * 1.5 + 60)
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
        listView.forceActiveFocus()
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

    function handleNavigationKey(key) {
        if (rowCount <= 0)
            return false
        const acceptKey = key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select || key === Qt.Key_Space
        const card = currentCard()
        if (!acceptKey && card && card.handleNavigationKey && card.handleNavigationKey(key))
            return true
        if (key === Qt.Key_Left) {
            if (listView.currentIndex <= 0) {
                if (shell)
                    shell.focusRail()
            } else {
                listView.currentIndex = listView.currentIndex - 1
            }
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
            if (card && card.handleNavigationKey && card.handleNavigationKey(key))
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

        delegate: MediaItemCard {
            id: posterDelegate
            required property int index
            required property string movieId
            readonly property var itemData: root.rowModel.get(index)
            width: root.cardWidth
            height: listView.height
            item: itemData
            kind: "poster"
            useSeriesPoster: root.useSeriesPoster
            focused: posterDelegate.index === listView.currentIndex && listView.activeFocus
            onActivated: {
                listView.currentIndex = posterDelegate.index
                root.currentIndex = posterDelegate.index
                root.activated(posterDelegate.index)
            }
            onFavoriteToggled: (favorite) => appController.setFavorite(posterDelegate.movieId || "", favorite)
            onPlayedToggled: (played) => appController.setPlayed(posterDelegate.movieId || "", played)
            onMediaInfoRequested: {
                if (root.shell)
                    root.shell.openMediaInfo(posterDelegate.itemData)
            }
        }

        Keys.onReleased: (event) => {
            if (root.handleNavigationKey(event.key))
                event.accepted = true
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
