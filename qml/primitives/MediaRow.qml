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
    readonly property bool delegatesPresented: presentation.delegatesReady
    readonly property bool artworkPresented: presentation.artworkReady

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
        presentation.reset()
    }

    function schedulePresentation() {
        presentation.schedule()
    }

    AtomicViewReveal {
        id: presentation

        view: listView
        latencyMonitor: InputLatency
        enabled: root.atomicPopulate && root.count > 0 && listView.width > 0
        firstIndex: 0
        lastIndex: Math.min(root.count, Math.max(1, Math.ceil((listView.width + root.cardGap) / (root.cardWidth
                                                                                                 + root.cardGap)))) - 1
    }

    Component {
        id: cardDelegate

        MediaItemCard {
            id: card

            required property int index
            required property var model
            readonly property var cardItem: model.item !== undefined ? model.item : model.modelData
            readonly property bool libraryCard: root.cardKind === "library"
            readonly property bool personCard: root.cardKind === "person"

            width: root.cardWidth
            height: listView.height
            shell: card.libraryCard || card.personCard ? null : root.shell
            kind: card.libraryCard ? "landscape" : card.personCard ? "poster" : root.cardKind
            item: card.cardItem || ({})
            titleOverride: card.libraryCard ? String(card.model.name || "") : card.personCard ? String(
                                                                                                    card.cardItem.name
                                                                                                    || "") : ""
            subtitleOverride: card.libraryCard ? String(card.model.collectionType || "") : card.personCard ? String(
                                                                                                                 card.cardItem.role
                                                                                                                 || card.cardItem.type
                                                                                                                 || "") : ""
            imageOverride: card.libraryCard ? Art.url(card.cardItem, "landscape", Math.ceil(root.cardWidth)) :
                                              card.personCard ? Art.url(card.cardItem, "poster", Math.ceil(
                                                                            root.cardWidth)) : ""
            fallbackOverride: card.personCard ? String(card.cardItem.type || "Person") : ""
            useSeriesPoster: root.useSeriesPoster
            preferEpisodeTitle: root.preferEpisodeTitle
            focused: card.index === listView.currentIndex && listView.activeFocus
            artworkVisible: !root.atomicPopulate || root.artworkPresented

            Component.onCompleted: root.schedulePresentation()
            onArtworkReadyChanged: root.schedulePresentation()
            onActivated: root.activateIndex(card.index)
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
