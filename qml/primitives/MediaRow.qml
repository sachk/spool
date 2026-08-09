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
    property string itemContextSource: ""
    property string itemContextReturnRoute: ""
    property var wheelFlickable: null
    property bool focusVisible: true
    property int modelRevision: 0
    readonly property bool delegatesPresented: presentation.delegatesReady

    readonly property int count: modelCount()
    readonly property bool rowVisible: enabledRow && (count > 0 || reserveWhenEmpty)
    readonly property bool posterCard: cardKind === "poster" || cardKind === "person"
    readonly property int headerHeight: Metrics.scaled(34)
    readonly property int cardHeight: Math.round(cardWidth * (posterCard ? 1.5 : 9 / 16) + Metrics.scaled(60))
    readonly property int focusPadding: Math.max(2, Metrics.scaled(2))

    signal verticalWheelScrolled(var controller)
    signal pointerSelected
    signal activated(int index, var item)

    width: parent ? parent.width : implicitWidth
    height: rowVisible ? headerHeight + Metrics.scaled(10) + cardHeight : 0
    implicitHeight: height
    visible: rowVisible
    focus: true

    Component.onCompleted: resetPresentation()
    onAtomicPopulateChanged: Qt.callLater(resetPresentation)
    onModelChanged: {
        ++modelRevision
        Qt.callLater(resetPresentation)
    }
    onCountChanged: {
        ++modelRevision
        currentIndex = count > 0 ? Math.max(0, Math.min(currentIndex, count - 1)) : -1
        // The view rewrites its currentIndex internally on model changes;
        // re-assert ours once it has processed them.
        Qt.callLater(syncViewCurrentIndex)
        Qt.callLater(resetPresentation)
    }
    onCurrentIndexChanged: syncViewCurrentIndex()

    // listView.currentIndex must never be a declarative binding: the view
    // writes the property itself (model resets, item removal), after which a
    // binding can sit stale — logical index and highlight then disagree until
    // the next property change. One-way imperative sync, re-asserted at every
    // interaction point, keeps the highlight truthful.
    function syncViewCurrentIndex() {
        const target = count > 0 ? Math.max(0, Math.min(currentIndex, count - 1)) : -1
        if (listView.currentIndex !== target)
            listView.currentIndex = target
    }

    Connections {
        target: root.model && root.model.rowCount !== undefined ? root.model : null
        ignoreUnknownSignals: true

        function onDataChanged() {
            ++root.modelRevision
        }
        function onModelReset() {
            ++root.modelRevision
        }
        function onRowsInserted() {
            ++root.modelRevision
        }
        function onRowsMoved() {
            ++root.modelRevision
        }
        function onRowsRemoved() {
            ++root.modelRevision
        }
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

    function itemAt(index, revision) {
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
        syncViewCurrentIndex()
        InputKeys.focus(listView)
        return true
    }

    function topLeftVisibleCandidate(outerViewport) {
        return InputKeys.topLeftVisibleCandidate(listView, outerViewport)
    }

    function focusIndexWithoutScrolling(index) {
        if (!InputKeys.focusIndexWithoutScrolling(listView, index))
            return false
        currentIndex = index
        return true
    }

    function currentCard() {
        return listView.currentItem
    }

    function moveBy(delta) {
        if (count <= 0)
            return false
        currentIndex = Math.max(0, Math.min(count - 1, currentIndex + delta))
        // Covers the clamped no-op case too (already at an edge): the view
        // may still be showing a stale highlight that needs re-asserting.
        syncViewCurrentIndex()
        return true
    }

    function routeKey(key, phase, repeat) {
        if (phase === "release")
            return true
        if (key === Qt.Key_Left)
            return moveBy(-1)
        if (key === Qt.Key_Right)
            return moveBy(1)
        return false
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
        return openItemContext(currentIndex, currentCard(), true)
    }

    function openItemContext(index, anchor, deferBackdropDismissal) {
        if (!shell || index < 0 || index >= count)
            return false
        return Boolean(shell.openItemMenu(itemAt(index), anchor, {
                                              "model": model,
                                              "index": index,
                                              "source": itemContextSource,
                                              "returnRoute": itemContextReturnRoute,
                                              "deferBackdropDismissal": Boolean(deferBackdropDismissal)
                                          }))
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
            readonly property var cardData: root.itemAt(index, root.modelRevision)
            readonly property var cardItem: root.cardKind === "library" ? (cardData.item || ({})) : cardData
            readonly property bool libraryCard: root.cardKind === "library"
            readonly property bool personCard: root.cardKind === "person"

            width: root.cardWidth
            height: listView.height
            shell: libraryCard || personCard ? null : root.shell
            kind: libraryCard ? "landscape" : personCard ? "poster" : root.cardKind
            item: cardItem || ({})
            titleOverride: libraryCard ? String(cardData.name || "") : personCard ? String(cardData.name || "") : ""
            subtitleOverride: libraryCard ? String(cardData.collectionType || "") : personCard ? String(cardData.role
                                                                                                        || cardData.type
                                                                                                        || "") : ""
            imageOverride: libraryCard ? Art.url(cardItem, "landscape") : personCard ? Art.url(cardItem, "poster") : ""
            fallbackOverride: personCard ? String(cardData.type || "Person") : ""
            fallbackIcon: libraryCard ? Theme.libraryIcon(cardData.collectionType) : ""
            fallbackTint: libraryCard ? Theme.libraryTint(cardData.name) : "transparent"
            useSeriesPoster: root.useSeriesPoster
            preferEpisodeTitle: root.preferEpisodeTitle
            focused: root.focusVisible && card.index === listView.currentIndex && listView.activeFocus
            artworkVisible: true

            Component.onCompleted: root.schedulePresentation()
            onArtworkReadyChanged: root.schedulePresentation()
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
        cacheBuffer: root.atomicPopulate ? 0 : Math.round(root.cardWidth + root.cardGap)
        leftMargin: root.focusPadding
        rightMargin: root.focusPadding
        reuseItems: true
        model: root.model
        delegate: cardDelegate

        highlightFollowsCurrentItem: true
        highlightMoveDuration: 16
        highlightResizeDuration: Theme.reducedMotion ? 0 : 75
        highlight: Item {
            Rectangle {
                anchors.fill: parent
                anchors.leftMargin: -root.focusPadding
                anchors.rightMargin: -root.focusPadding
                color: "transparent"
                radius: Theme.radiusMedium
                border.width: Theme.focusBorderWidth
                border.color: root.focusVisible && listView.activeFocus ? Theme.accent : "transparent"
                z: 2
            }
        }

        Component.onCompleted: root.syncViewCurrentIndex()
        onCurrentIndexChanged: if (currentIndex >= 0)
        positionViewAtIndex(currentIndex, ListView.Contain)

        FastWheelHandler {
            id: wheelHandler
            flickable: root.wheelFlickable || listView
            horizontal: root.wheelFlickable === null
            onScrolled: if (root.wheelFlickable)
            root.verticalWheelScrolled(wheelHandler)
        }

        MouseArea {
            property int pressedIndex: -1
            property bool longPressed: false
            anchors.fill: parent
            z: 3
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            pressAndHoldInterval: 520
            onPressed: mouse => {
                longPressed = false
                pressedIndex = listView.indexAt(mouse.x + listView.contentX, mouse.y + listView.contentY)
                if (pressedIndex >= 0) {
                    root.pointerSelected()
                    root.currentIndex = pressedIndex
                }
            }
            onReleased: if (longPressed && root.shell)
            root.shell.finishItemMenuOpeningGesture()
            onCanceled: if (longPressed && root.shell)
            root.shell.finishItemMenuOpeningGesture()
            onClicked: mouse => {
                if (pressedIndex < 0)
                    return
                if (longPressed) {
                    longPressed = false
                    return
                }
                if (mouse.button === Qt.RightButton && root.shell)
                    root.openItemContext(pressedIndex, listView.itemAtIndex(pressedIndex))
                else
                    root.activateIndex(pressedIndex)
            }
            onPressAndHold: if (pressedIndex >= 0 && root.shell) {
                longPressed = true
                root.openItemContext(pressedIndex, listView.itemAtIndex(pressedIndex), true)
            }
        }
    }

    SecondaryText {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: rowHeader.bottom
        anchors.topMargin: Metrics.scaled(18)
        height: root.cardHeight
        visible: root.count <= 0 && root.reserveWhenEmpty
        text: root.loading ? root.emptyText : ""
        color: Theme.textMuted
        font.pixelSize: Metrics.metaSizePx
        verticalAlignment: Text.AlignTop
    }
}
