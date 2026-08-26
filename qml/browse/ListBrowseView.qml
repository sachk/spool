pragma ComponentBehavior: Bound

import QtQuick
import "../theme"
import "../primitives"
import "../primitives/ModelAccess.js" as ModelAccess

// A vertical list of media rows, each with its own trailing actions.
//
// Rows are walked with Up and Down; Right steps into the actions at the end
// of the row being looked at and Left steps back out of them, which is how a
// remote reaches a per-item action without a menu and without an extra focus
// stop on every row it is not using.
FocusScope {
    id: root

    property var model: null
    // [{ action, icon, label }] offered on every row.
    property var rowActions: []
    property string cardKind: "landscape"
    property string emptyTitle: "Nothing here"
    property string emptyDetail: ""
    property bool busy: false
    property int currentIndex: 0
    // Item id of the row that is playing, if any, so it can be marked.
    property string currentItemId: ""
    property bool showChevrons: false
    // Optional overrides, for lists whose rows are not media items -- the
    // sheet's top level lists places rather than things.
    property var rowTitle: null
    property var rowSubtitle: null
    property var rowIcon: null

    signal activated(int index, var item)
    signal actionTriggered(int index, var item, string action)
    signal edgeUp

    readonly property int count: ModelAccess.count(model)

    function itemAt(index) {
        return ModelAccess.at(model, index)
    }

    function currentItem() {
        return itemAt(currentIndex)
    }

    function focusList() {
        if (count <= 0)
            return false
        currentIndex = Math.max(0, Math.min(currentIndex, count - 1))
        list.currentIndex = currentIndex
        InputKeys.focus(list)
        return true
    }

    function currentRow() {
        return list.currentItem
    }

    function moveRow(delta) {
        if (count <= 0)
            return false
        const next = currentIndex + delta
        if (next < 0) {
            root.edgeUp()
            return true
        }
        if (next >= count)
            return true
        // Stepping to a new row leaves its actions untouched, so a run down
        // the list does not drag a chosen action along with it.
        const row = currentRow()
        if (row)
            row.actionIndex = -1
        currentIndex = next
        list.currentIndex = next
        list.positionViewAtIndex(next, ListView.Contain)
        return true
    }

    function routeKey(key, phase, repeat) {
        if (phase === "release")
            return true
        if (key === Qt.Key_Up)
            return moveRow(-1)
        if (key === Qt.Key_Down)
            return moveRow(1)
        const row = currentRow()
        if (!row || root.rowActions.length <= 0)
            return false
        if (key === Qt.Key_Right)
            return row.moveAction(1)
        if (key === Qt.Key_Left)
            return row.moveAction(-1)
        return false
    }

    function activate() {
        const row = currentRow()
        if (row)
            row.activate()
    }

    onCountChanged: if (currentIndex >= count)
    currentIndex = Math.max(0, count - 1)

    MenuListView {
        id: list

        anchors.fill: parent
        model: root.model
        spacing: Metrics.scaled(4)
        dismissOnBack: false
        dismissOnHorizontal: false
        currentIndex: root.currentIndex
        focus: true

        delegate: MediaListRow {
            required property int index
            required property var model

            width: ListView.view.width
            item: model.item !== undefined ? model.item : model
            kind: root.cardKind
            titleOverride: root.rowTitle ? String(root.rowTitle(item, index)) : ""
            subtitleOverride: root.rowSubtitle ? String(root.rowSubtitle(item, index)) : ""
            fallbackIcon: root.rowIcon ? String(root.rowIcon(item, index)) : ""
            actions: root.rowActions
            showChevron: root.showChevrons
            highlighted: index === root.currentIndex && list.activeFocus
            current: root.currentItemId.length > 0 && String(item.movieId || item.id || "") === root.currentItemId
            progress: model.progress !== undefined ? model.progress : -1

            onActivated: {
                root.currentIndex = index
                root.activated(index, item)
            }
            onActionTriggered: action => {
                root.currentIndex = index
                root.actionTriggered(index, item, action)
            }
        }
    }

    EmptyPlaceholder {
        anchors.fill: parent
        visible: root.count === 0 && !root.busy
        title: root.emptyTitle
        detail: root.emptyDetail
    }

    BusySpinner {
        anchors.centerIn: parent
        width: Metrics.scaled(32)
        height: width
        running: root.busy && root.count === 0
        visible: running
    }
}
