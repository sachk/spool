pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "../theme"

FocusScope {
    id: root

    property string title: "Choose an option"
    property var options: []
    property int currentIndex: 0
    property Item anchorItem: null
    property bool spaceRequested: false
    property var inputKeys: InputKeys
    readonly property real edgeMargin: Metrics.scaled(12)
    readonly property real rowHeight: Math.max(Metrics.scaled(44), Metrics.controlHeight(width))
    readonly property real panelWidth: Math.min(width - edgeMargin * 2, Math.max(Metrics.scaled(280), Math.min(Metrics.scaled(
                                                                                                                   520), anchorItem
                                                                                                               ? anchorItem.width
                                                                                                                 * 0.52 : Metrics.scaled(
                                                                                                                     380))))
    readonly property real panelHeight: Math.min(height - edgeMargin * 2, Math.max(rowHeight + Metrics.scaled(16), Math.min(
                                                                                       options.length, 8) * rowHeight
                                                                                   + Metrics.scaled(16)))

    signal selected(int index)
    signal dismissed
    signal spaceBelowRequired(real pixels)

    anchors.fill: parent
    focus: true
    z: 100

    function focusCurrent() {
        const count = optionList.count
        optionList.currentIndex = count > 0 ? Math.max(0, Math.min(root.currentIndex, count - 1)) : -1
        if (optionList.currentIndex >= 0)
            optionList.positionViewAtIndex(optionList.currentIndex, ListView.Contain)
        inputKeys.focus(optionList)
    }

    function positionPopup() {
        if (!visible)
            return
        if (!anchorItem) {
            menuPanel.x = Math.round((width - menuPanel.width) / 2)
            menuPanel.y = Math.round((height - menuPanel.height) / 2)
            return
        }

        const anchor = anchorItem.mapToItem(root, 0, 0)
        const below = anchor.y + anchorItem.height + Metrics.scaled(6)
        const deficit = below + menuPanel.height + edgeMargin - height
        if (deficit > 1 && !spaceRequested) {
            spaceRequested = true
            spaceBelowRequired(deficit)
            return
        }

        const above = anchor.y - menuPanel.height - Metrics.scaled(6)
        const desiredY = below + menuPanel.height <= height - edgeMargin || above < edgeMargin ? below : above
        menuPanel.x = Math.max(edgeMargin, Math.min(width - menuPanel.width - edgeMargin, anchor.x + anchorItem.width
                                                    - menuPanel.width))
        menuPanel.y = Math.max(edgeMargin, Math.min(height - menuPanel.height - edgeMargin, desiredY))
    }

    function routeKey(key, phase, repeat) {
        if (inputKeys.isBack(key, false, false)) {
            if (phase === "release")
                dismissed()
            return true
        }
        if (inputKeys.isDirection(key)) {
            if (phase === "press" && key === Qt.Key_Up)
                optionList.moveSelection(-1)
            else if (phase === "press" && key === Qt.Key_Down)
                optionList.moveSelection(1)
            return true
        }
        return inputKeys.isAccept(key)
    }

    function activate() {
        optionList.activate()
    }

    function back() {
        dismissed()
        return true
    }

    onVisibleChanged: {
        if (!visible)
        return
        spaceRequested = false
        Qt.callLater(function () {
            positionPopup()
            focusCurrent()
        })
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.dismissed()
    }

    PopupMenuPanel {
        id: menuPanel
        width: root.panelWidth
        open: root.visible
        openHeight: root.panelHeight
        baseColor: Theme.floatingPanel

        MouseArea {
            anchors.fill: parent
        }

        MenuListView {
            id: optionList
            anchors.fill: parent
            anchors.margins: Metrics.scaled(8)
            spacing: Metrics.scaled(2)
            model: root.options
            currentIndex: root.currentIndex
            onDismissed: root.dismissed()
            onAccepted: index => root.selected(index)

            delegate: MenuRow {
                required property int index
                required property var modelData
                width: optionList.width
                rowHeight: root.rowHeight
                label: String(modelData)
                checked: index === root.currentIndex
                highlighted: optionList.activeFocus && optionList.currentIndex === index
                metricsWidth: root.width
                onHovered: optionList.currentIndex = index
                onActivated: root.selected(index)
            }
        }
    }
}
