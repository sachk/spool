import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

OverlayDialog {
    id: root

    property string mode: ""
    property var item: ({})
    property var targets: []
    property string currentViewKind: ""
    property string nameDraft: ""
    property int targetIndex: 0

    signal createRequested(string name)
    signal targetRequested(int index)
    signal confirmRequested

    function itemTitle(value) {
        return String(value && (value.displayTitle || value.title || value.seriesName || value.name) || "Selected item")
    }

    function title() {
        if (mode === "playlist")
            return "Add to playlist"
        if (mode === "collection")
            return "Add to collection"
        if (mode === "rename")
            return "Rename"
        if (mode === "delete")
            return "Delete item"
        return currentViewKind === "playlist" ? "Remove from playlist" : "Remove from collection"
    }

    function focusNameField() {
        InputKeys.focus(managementNameField)
        managementNameField.focusField()
    }

    function handleKey(event, released) {
        if (!released)
            return true
        if (InputKeys.isBackEvent(event, true)) {
            dismissed()
            return true
        }
        if (mode === "playlist" || mode === "collection") {
            const lastIndex = targets.length
            if (event.key === Qt.Key_Up) {
                targetIndex = Math.max(0, targetIndex - 1)
                return true
            }
            if (event.key === Qt.Key_Down) {
                targetIndex = Math.min(lastIndex, targetIndex + 1)
                return true
            }
            if (InputKeys.isAccept(event.key)) {
                targetRequested(targetIndex)
                return true
            }
        } else if (mode === "rename") {
            if (InputKeys.isAccept(event.key)) {
                createRequested(nameDraft.trim())
                return true
            }
        } else if (mode === "delete" || mode === "remove") {
            if (InputKeys.isAccept(event.key)) {
                confirmRequested()
                return true
            }
        }
        return true
    }

    onDismissed: root.forceActiveFocus()

    AppText {
        Layout.fillWidth: true
        text: root.title()
        color: Theme.textPrimary
        font.pixelSize: Metrics.titlePx(root.width)
        font.weight: Font.DemiBold
    }

    AppText {
        Layout.fillWidth: true
        text: root.itemTitle(root.item)
        color: Theme.textMuted
        font.pixelSize: Metrics.bodyPx(root.width)
        elide: Text.ElideRight
        visible: root.mode.length > 0
    }

    TextFieldRow {
        id: managementNameField
        Layout.fillWidth: true
        visible: root.mode === "playlist" || root.mode === "collection" || root.mode === "rename"
        label: root.mode === "rename" ? "Name" : (root.mode === "collection" ? "New collection name" :
                                                                               "New playlist name")
        text: root.nameDraft
        onTextEdited: value => root.nameDraft = value
        onAccepted: root.createRequested(root.nameDraft.trim())
    }

    AppText {
        Layout.fillWidth: true
        visible: root.mode === "delete" || root.mode === "remove"
        text: root.mode === "delete" ? "This permanently deletes the item from the server." :
                                       "This removes the item from the current playlist or collection."
        color: Theme.textMuted
        font.pixelSize: Metrics.bodyPx(root.width)
        wrapMode: Text.Wrap
    }

    Repeater {
        model: root.mode === "playlist" || root.mode === "collection" ? root.targets.length + 1 : 0
        delegate: Rectangle {
            required property int index
            Layout.fillWidth: true
            height: Metrics.controlHeight(root.width)
            radius: Theme.radiusMedium
            color: root.targetIndex === index ? Theme.focusedFill : "transparent"
            border.width: root.targetIndex === index ? Theme.focusBorderWidth : 1
            border.color: root.targetIndex === index ? Theme.accent : Theme.border

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 12
                MaterialIcon {
                    name: index === 0 ? "add" : (root.mode === "collection" ? "collections_bookmark" : "playlist_play")
                    iconColor: Theme.textPrimary
                    iconSize: Metrics.iconPx(root.width)
                }
                AppText {
                    Layout.fillWidth: true
                    text: index === 0 ? "Create new" : root.itemTitle(root.targets[index - 1] || ({}))
                    color: Theme.textPrimary
                    font.pixelSize: Metrics.bodyPx(root.width)
                    elide: Text.ElideRight
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    root.targetIndex = index
                    root.targetRequested(index)
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: root.mode === "delete" || root.mode === "remove"
        Item {
            Layout.fillWidth: true
        }
        ActionButton {
            text: "Cancel"
            onClicked: root.dismissed()
        }
        ActionButton {
            text: root.mode === "delete" ? "Delete" : "Remove"
            kind: "primary"
            onClicked: root.confirmRequested()
        }
    }
}
