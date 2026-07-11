pragma ComponentBehavior: Bound

import QtQuick
import "../theme"

FocusScope {
    id: root

    property string title: ""
    property var model
    property var shell
    property string cardKind: "poster" // poster, landscape, library, or person
    property bool useSeriesPoster: false
    property bool preferEpisodeTitle: false
    property int cardWidth: Metrics.scaled(156)
    property int cardGap: Metrics.scaled(16)
    property int currentIndex: 0
    property bool enabledRow: true
    property bool reserveWhenEmpty: false
    property bool loading: false
    property string emptyText: "Loading..."
    property bool atomicPopulate: false
    property bool delegatesPresented: !atomicPopulate
    property bool artworkPresented: !atomicPopulate

    readonly property int count: modelCount()
    readonly property bool rowVisible: enabledRow && (count > 0 || reserveWhenEmpty)
    readonly property bool posterCard: cardKind === "poster" || cardKind === "person"
    readonly property int headerHeight: Metrics.scaled(34)
    readonly property int cardHeight: Math.round(cardWidth * (posterCard ? 1.5 : 9 / 16) + Metrics.scaled(60))

    signal activated(int index, var item)

    width: parent ? parent.width : implicitWidth
    height: rowVisible ? headerHeight + Metrics.scaled(10) + cardHeight : 0
    implicitHeight: height
    visible: rowVisible
    focus: true

    Component.onCompleted: resetPresentation()
    onAtomicPopulateChanged: Qt.callLater(resetPresentation)
    onModelChanged: Qt.callLater(resetPresentation)
    onCountChanged: {
        currentIndex = count > 0 ? Math.max(0, Math.min(currentIndex, count - 1)) : -1
        Qt.callLater(resetPresentation)
    }

    function modelCount() {
        if (!model)
            return 0
        if (model.count !== undefined)
            return Number(model.count)
        if (model.length !== undefined)
            return Number(model.length)
        return model.rowCount ? Number(model.rowCount()) : 0
    }

    function itemAt(index) {
        if (!model || index < 0 || index >= count)
            return ({})
        if (model.get)
            return model.get(index) || ({})
        return model[index] || ({})
    }

    function focusList() {
        if (count <= 0)
            return false
        currentIndex = Math.max(0, Math.min(currentIndex, count - 1))
        InputKeys.focus(listView)
        return true
    }

    function currentCard() {
        return listView.currentItem
    }

    function routeKey(key, phase, repeat) {
        if (count <= 0)
            return false
        if (key === Qt.Key_Left)
            currentIndex = Math.max(0, currentIndex - 1)
        else if (key === Qt.Key_Right)
            currentIndex = Math.min(count - 1, currentIndex + 1)
        else
            return false
        return true
    }

    function activateIndex(index) {
        if (index < 0 || index >= count)
            return
        currentIndex = index
        activated(index, itemAt(index))
    }

    function activate() {
        activateIndex(currentIndex)
    }

    function longPress() {
        if (cardKind === "library" || cardKind === "person" || currentIndex < 0 || !shell)
            return false
        return Boolean(shell.openItemMenu(itemAt(currentIndex), currentCard()))
    }

    function resetPresentation() {
        presentationTimer.stop()
        const immediate = !atomicPopulate || count <= 0
        delegatesPresented = immediate
        artworkPresented = immediate
        if (immediate)
            return
        presentationTimer.restart()
    }

    function schedulePresentation() {
        if (atomicPopulate && count > 0 && (!delegatesPresented || !artworkPresented))
            presentationTimer.restart()
    }

    function updatePresentation() {
        listView.forceLayout()
        const stride = cardWidth + cardGap
        const required = Math.min(count, Math.max(1, Math.ceil((listView.width + cardGap) / stride)))
        let delegatesReady = required > 0
        let imagesReady = delegatesReady
        for (let index = 0; index < required; ++index) {
            const delegate = listView.itemAtIndex(index)
            if (!delegate) {
                delegatesReady = false
                imagesReady = false
                break
            }
            if (!delegate.artworkReady)
                imagesReady = false
        }
        if (delegatesReady)
            delegatesPresented = true
        if (imagesReady) {
            artworkPresented = true
        }
    }

    Timer {
        id: presentationTimer
        interval: 0
        repeat: false
        onTriggered: root.updatePresentation()
    }

    Component {
        id: cardDelegate

        Item {
            id: delegateRoot

            required property int index
            required property var model
            readonly property var cardItem: model.item !== undefined ? model.item : model.modelData
            readonly property bool libraryCard: root.cardKind === "library"
            readonly property bool personCard: root.cardKind === "person"
            readonly property bool artworkReady: card.artworkReady

            width: root.cardWidth
            height: listView.height

            Component.onCompleted: root.schedulePresentation()
            onArtworkReadyChanged: root.schedulePresentation()

            MediaItemCard {
                id: card

                anchors.fill: parent
                shell: delegateRoot.libraryCard || delegateRoot.personCard ? null : root.shell
                kind: delegateRoot.libraryCard ? "landscape" : delegateRoot.personCard ? "poster" : root.cardKind
                item: delegateRoot.cardItem || ({})
                titleOverride: delegateRoot.libraryCard ? String(delegateRoot.model.name || "") :
                                                          delegateRoot.personCard ? String(delegateRoot.cardItem.name
                                                                                           || "") : ""
                subtitleOverride: delegateRoot.libraryCard ? String(delegateRoot.model.collectionType || "") :
                                                             delegateRoot.personCard ? String(
                                                                                           delegateRoot.cardItem.role
                                                                                           || delegateRoot.cardItem.type
                                                                                           || "") : ""
                imageOverride: delegateRoot.libraryCard ? Art.url(delegateRoot.cardItem, "landscape", Math.ceil(
                                                                      root.cardWidth)) : delegateRoot.personCard
                                                          ? Art.url(delegateRoot.cardItem, "poster", Math.ceil(
                                                                        root.cardWidth)) : ""
                fallbackOverride: delegateRoot.personCard ? String(delegateRoot.cardItem.type || "Person") : ""
                useSeriesPoster: root.useSeriesPoster
                preferEpisodeTitle: root.preferEpisodeTitle
                focused: delegateRoot.index === listView.currentIndex && listView.activeFocus
                artworkVisible: !root.atomicPopulate || root.artworkPresented
                onActivated: root.activateIndex(delegateRoot.index)
            }
        }
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
        anchors.topMargin: Metrics.scaled(10)
        height: root.cardHeight
        visible: root.count > 0
        opacity: root.delegatesPresented ? 1 : 0
        focus: true
        keyNavigationEnabled: false
        clip: true
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds
        spacing: root.cardGap
        cacheBuffer: Math.round(2 * (root.cardWidth + root.cardGap))
        reuseItems: true
        model: root.model
        delegate: cardDelegate

        currentIndex: root.count > 0 ? Math.max(0, Math.min(root.currentIndex, root.count - 1)) : -1
        onCurrentIndexChanged: if (currentIndex >= 0)
        positionViewAtIndex(currentIndex, ListView.Contain)

        FastWheelHandler {
            flickable: listView
            horizontal: true
        }
    }

    MonoText {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: rowHeader.bottom
        anchors.topMargin: Metrics.scaled(18)
        height: root.cardHeight
        visible: root.count <= 0 && root.reserveWhenEmpty
        text: root.loading ? root.emptyText : ""
        color: Theme.textMuted
        font.pixelSize: Metrics.metaPx(root.Window.window ? root.Window.window.width : 1920)
        verticalAlignment: Text.AlignTop
    }
}
