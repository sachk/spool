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
    property int cardWidth: 156
    property int cardGap: 16
    property int currentIndex: 0
    property bool enabledRow: true
    property bool reserveWhenEmpty: false
    property bool loading: false
    property string emptyText: "Loading..."

    readonly property int count: modelCount()
    readonly property bool rowVisible: enabledRow && (count > 0 || reserveWhenEmpty)
    readonly property bool posterCard: cardKind === "poster" || cardKind === "person"
    readonly property int headerHeight: 34
    readonly property int cardHeight: Math.round(cardWidth * (posterCard ? 1.5 : 9 / 16) + 60)

    signal activated(int index, var item)

    width: parent ? parent.width : implicitWidth
    height: rowVisible ? headerHeight + 10 + cardHeight : 0
    implicitHeight: height
    visible: rowVisible
    focus: true

    onCountChanged: currentIndex = count > 0 ? Math.max(0, Math.min(currentIndex, count - 1)) : -1

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

    Component {
        id: mediaDelegate

        Item {
            id: delegateRoot
            required property int index
            required property var item

            width: root.cardWidth
            height: listView.height

            MediaItemCard {
                anchors.fill: parent
                shell: root.shell
                kind: root.cardKind
                useSeriesPoster: root.useSeriesPoster
                preferEpisodeTitle: root.preferEpisodeTitle
                focused: delegateRoot.index === listView.currentIndex && listView.activeFocus
                item: delegateRoot.item
                onActivated: root.activateIndex(delegateRoot.index)
            }
        }
    }

    Component {
        id: libraryDelegate

        Item {
            id: delegateRoot
            required property int index
            required property string name
            required property string collectionType
            required property string imageUrl

            width: root.cardWidth
            height: listView.height

            MediaItemCard {
                anchors.fill: parent
                kind: "landscape"
                focused: delegateRoot.index === listView.currentIndex && listView.activeFocus
                titleOverride: delegateRoot.name
                subtitleOverride: delegateRoot.collectionType
                imageOverride: delegateRoot.imageUrl
                onActivated: root.activateIndex(delegateRoot.index)
            }
        }
    }

    Component {
        id: personDelegate

        Item {
            id: delegateRoot
            required property int index
            required property var modelData

            width: root.cardWidth
            height: listView.height

            MediaItemCard {
                anchors.fill: parent
                kind: "poster"
                focused: delegateRoot.index === listView.currentIndex && listView.activeFocus
                titleOverride: String(delegateRoot.modelData.name || "")
                subtitleOverride: String(delegateRoot.modelData.role || delegateRoot.modelData.type || "")
                imageOverride: String(delegateRoot.modelData.imageUrl || "")
                fallbackOverride: String(delegateRoot.modelData.type || "Person")
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
        anchors.topMargin: 10
        height: root.cardHeight
        visible: root.count > 0
        focus: true
        keyNavigationEnabled: false
        clip: true
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds
        spacing: root.cardGap
        cacheBuffer: Math.round(2 * (root.cardWidth + root.cardGap))
        reuseItems: true
        model: root.model
        delegate: root.cardKind === "library" ? libraryDelegate : root.cardKind === "person" ? personDelegate : mediaDelegate
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
        anchors.topMargin: 18
        height: root.cardHeight
        visible: root.count <= 0 && root.reserveWhenEmpty
        text: root.loading ? root.emptyText : ""
        color: Theme.textMuted
        font.pixelSize: Metrics.metaPx(root.Window.window ? root.Window.window.width : 1920)
        verticalAlignment: Text.AlignTop
    }
}
