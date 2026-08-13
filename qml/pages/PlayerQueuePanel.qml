import QtQuick
import JellyfinWebOS
import "../primitives"
import "../theme"

// The queue as a right-edge sheet over live video, replacing the dropdown that
// borrowed the playback-settings menu. Built on the SubtitleSettingsPanel
// shape: a Surface anchored to the list rather than the parent, so the panel
// hugs the edge and leaves the picture it is queueing against on screen.
FocusScope {
    id: root

    required property var overlay

    readonly property var queue: PlayQueue
    // Reorder and removal are local edits today, so a SyncPlay group has to sit
    // this out until the server-backed queue calls land.
    readonly property bool editable: !SyncPlay.enabled

    // A picked-up row, whether by remote or by pointer. Both funnel through the
    // same preview/commit pair so there is one reorder path to reason about.
    property int grabbedIndex: -1
    property int grabbedOrigin: -1
    property int dragIndex: -1
    property int dragOrigin: -1
    readonly property bool reordering: grabbedIndex >= 0 || dragIndex >= 0

    signal dismissed

    readonly property real panelWidth: Platform.isTV ? Math.min(parent ? parent.width * 0.38 : 0, overlay.dp(560)) : Math.min(
                                                           parent ? parent.width * 0.42 : 0, overlay.dp(430))

    function focusRow(index) {
        list.currentIndex = Math.max(0, Math.min(index, list.count - 1))
        InputKeys.focus(list)
    }

    function cancelReorder() {
        if (grabbedIndex < 0)
            return false
        if (grabbedIndex !== grabbedOrigin)
            App.previewQueueMove(grabbedIndex, grabbedOrigin)
        list.currentIndex = grabbedOrigin
        grabbedIndex = -1
        grabbedOrigin = -1
        return true
    }

    function dropReorder() {
        if (grabbedIndex < 0)
            return false
        App.commitQueueMove(grabbedOrigin, grabbedIndex)
        list.currentIndex = grabbedIndex
        grabbedIndex = -1
        grabbedOrigin = -1
        return true
    }

    // Live reorder while the pointer moves. That only became safe once the list
    // bound the model: beginMoveRows moves the delegate the pointer is holding
    // instead of destroying it, which is what defeated the old drag.
    function dragTo(sceneY) {
        if (dragIndex < 0)
            return
        const local = list.mapFromItem(null, 0, sceneY)
        const target = list.indexAt(list.width / 2, local.y + list.contentY)
        if (target < 0 || target === dragIndex)
            return
        if (App.previewQueueMove(dragIndex, target))
            dragIndex = target
    }

    function stepGrabbed(delta) {
        const target = grabbedIndex + delta
        if (target < 0 || target >= list.count)
            return true
        if (App.previewQueueMove(grabbedIndex, target)) {
            grabbedIndex = target
            list.currentIndex = target
            list.positionViewAtIndex(target, ListView.Contain)
        }
        return true
    }

    // KeyRouter defers activation to key release for any target that owns a
    // longPress member, so VideoSurface only advertises one while this panel is
    // open. Returning false here leaves the press behaving normally.
    function longPress() {
        if (!editable || grabbedIndex >= 0 || list.count < 2)
            return false
        if (list.currentIndex < 0 || list.currentIndex >= list.count)
            return false
        grabbedIndex = list.currentIndex
        grabbedOrigin = list.currentIndex
        return true
    }

    // Reached on the release that completed the grab gesture. Dropping here
    // would undo the pick-up the user just made.
    function finishOpeningGesture() {
    }

    function activate() {
        if (grabbedIndex >= 0) {
            dropReorder()
            return
        }
        if (list.currentIndex >= 0 && list.currentIndex < list.count)
            App.playQueueItem(list.currentIndex)
    }

    function back() {
        if (cancelReorder())
            return true
        root.dismissed()
        return true
    }

    function routeKey(key, phase, repeat) {
        if (phase === "release")
            return true

        if (grabbedIndex >= 0) {
            if (key === Qt.Key_Up)
                return stepGrabbed(-1)
            if (key === Qt.Key_Down)
                return stepGrabbed(1)
            // Nothing else should escape a grab: leaving the panel mid-move
            // would strand the row somewhere the user did not choose.
            return true
        }

        // Left returns to the video and its transport; Right has nowhere to go
        // from the rightmost thing on screen.
        if (key === Qt.Key_Left) {
            root.dismissed()
            return true
        }
        if (key === Qt.Key_Right)
            return true

        return list.routeKey(key, phase, repeat)
    }

    Surface {
        anchors.left: list.left
        anchors.right: list.right
        anchors.top: heading.top
        anchors.bottom: list.bottom
        anchors.margins: -overlay.dp(14)
        baseColor: Theme.floatingPanel
        elevated: true
    }

    Item {
        id: heading

        anchors.top: parent.top
        anchors.left: list.left
        anchors.right: list.right
        anchors.topMargin: list.inset
        height: Math.max(closeButton.height, headingText.implicitHeight + hint.height)

        AppText {
            id: headingText
            anchors.left: parent.left
            anchors.right: closeButton.visible ? closeButton.left : parent.right
            anchors.rightMargin: Metrics.scaled(12)
            anchors.top: parent.top
            text: "Queue"
            font.pixelSize: Metrics.titleSizePx
            font.weight: Font.DemiBold
            maximumLineCount: 1
            elide: Text.ElideRight
        }

        // A picked-up row is the one state with no obvious way out, so it says
        // so rather than leaving the user to guess at the remote.
        SecondaryText {
            id: hint
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: headingText.bottom
            height: visible ? implicitHeight : 0
            visible: root.grabbedIndex >= 0 || (!root.editable && list.count > 0)
            text: root.grabbedIndex >= 0 ? "Moving — Up/Down to place, OK to drop, Back to cancel" :
                                           "Leave SyncPlay to change the queue"
            color: root.grabbedIndex >= 0 ? Theme.accent : Theme.textMuted
            font.pixelSize: Metrics.metaSizePx
            maximumLineCount: 1
            elide: Text.ElideRight
        }

        // Pointer-only, matching the subtitle panel: a remote closes with Back,
        // and a focusable button here is just one more stop before the rows.
        IconButton {
            id: closeButton
            visible: !Platform.isTV
            anchors.right: parent.right
            anchors.top: parent.top
            focusPolicy: Qt.NoFocus
            chromeless: true
            iconName: "close"
            accessibleName: "Close queue"
            onClicked: root.dismissed()
        }
    }

    MenuListView {
        id: list

        readonly property real inset: Metrics.pageMarginPx

        width: root.panelWidth
        anchors.top: heading.bottom
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.topMargin: Metrics.scaled(12)
        anchors.bottomMargin: inset
        anchors.rightMargin: inset
        spacing: overlay.dp(4)

        // The panel owns Back and the horizontals; the list only walks rows.
        dismissOnBack: false
        dismissOnHorizontal: false

        // Binding the model is the fix for the drag that never worked: the old
        // list rebuilt a JS array on every queueChanged, so moving a row
        // destroyed the very delegate the pointer was holding. A real model
        // moves the delegate instead.
        model: root.queue

        delegate: PlayerQueueRow {
            required property int index
            required property var model

            width: ListView.view.width
            overlay: root.overlay
            entry: model.item
            entryType: model.itemType
            title: model.displayTitle
            subtitle: model.displaySubtitle
            episodeCode: model.episodeCode
            genericEpisodeTitle: model.genericEpisodeTitle
            playable: model.playable
            progress: model.progress
            position: index + 1
            current: index === root.queue.currentIndex
            highlighted: index === list.currentIndex && list.activeFocus
            grabbed: index === root.grabbedIndex || index === root.dragIndex
            reordering: root.reordering
            pointerEnabled: !Platform.isTV && root.editable

            onActivated: {
                list.currentIndex = index
                App.playQueueItem(index)
            }
            onRemoveRequested: App.removeQueueItem(index)

            onDragStarted: {
                root.dragOrigin = index
                root.dragIndex = index
                list.interactive = false
            }
            onDragMovedTo: sceneY => root.dragTo(sceneY)
            onDragEnded: {
                if (root.dragIndex >= 0)
                    App.commitQueueMove(root.dragOrigin, root.dragIndex)
                root.dragIndex = -1
                root.dragOrigin = -1
                list.interactive = true
            }
        }
    }
}
