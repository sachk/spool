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
    property bool loadImages: true
    property int imageLoadDelay: 35
    property int cardWidth: 240
    property int cardGap: 16
    property int currentIndex: rowCount > 0 ? 0 : -1
    readonly property int rowCount: countObserver.count
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

    ModelCountObserver {
        id: countObserver
        sourceModel: root.rowModel
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
            cacheBuffer: Math.round(root.cardWidth + root.cardGap)
            model: root.visibleCount
            currentIndex: root.currentIndex
            onCurrentIndexChanged: {
                root.currentIndex = currentIndex
                root.ensureVisible()
            }

            FastWheelHandler {
                flickable: rowList
                horizontal: true
            }

            delegate: Item {
                id: cardDelegate

                required property int index
                readonly property var itemData: root.itemAt(index)
                readonly property bool current: index === rowList.currentIndex && rowList.activeFocus
                readonly property bool loadArtwork: root.loadImages
                                                    && x + width >= rowList.contentX - rowList.cacheBuffer
                                                    && x <= rowList.contentX + rowList.width + rowList.cacheBuffer

                width: root.cardWidth
                height: rowList.height

                function handleAcceptPressed(key) {
                    return mediaCard.visible && mediaCard.handleAcceptPressed(key)
                }

                function handleAcceptReleased(key) {
                    return mediaCard.visible && mediaCard.handleAcceptReleased(key)
                }

                function handleNavigationKey(key) {
                    return mediaCard.visible && mediaCard.handleNavigationKey(key)
                }

                MediaItemCard {
                    id: mediaCard

                    anchors.fill: parent
                    visible: !root.libraryRow
                    item: cardDelegate.itemData
                    shell: root.shell
                    kind: root.rowKind === "poster" ? "poster" : "landscape"
                    useSeriesPoster: root.useSeriesPoster
                    focused: cardDelegate.current
                    loadImage: cardDelegate.loadArtwork
                    imageLoadDelay: root.imageLoadDelay
                    onActivated: root.activated(cardDelegate.index)
                    onFavoriteToggled: (favorite) => root.favoriteToggled(cardDelegate.index, favorite)
                    onPlayedToggled: (played) => root.playedToggled(cardDelegate.index, played)
                    onMediaInfoRequested: root.mediaInfoRequested(cardDelegate.index)
                }

                LandscapeCard {
                    anchors.fill: parent
                    visible: root.libraryRow
                    title: cardDelegate.itemData.name || ""
                    subtitle: cardDelegate.itemData.collectionType || ""
                    imageUrl: cardDelegate.itemData.imageUrl || ""
                    focused: cardDelegate.current
                    loadImage: cardDelegate.loadArtwork
                    imageLoadDelay: root.imageLoadDelay
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: root.libraryRow
                    onClicked: {
                        root.currentIndex = cardDelegate.index
                        root.activated(cardDelegate.index)
                    }
                }
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
