import QtQuick
import "../theme"

// Horizontal row of focusable text chips (genres, studios, …). Mirrors the
// navigation surface of MediaPosterScrollerRow so detail pages can mix the two
// into one D-pad chain: Left/Right move between chips, accept emits activated().
FocusScope {
    id: root

    property string title: ""
    property var values: []
    property int currentIndex: 0
    property int rowGap: 12
    property bool enabledRow: true
    readonly property int rowCount: values ? values.length : 0
    readonly property int headerHeight: 30
    readonly property int chipHeight: 40
    readonly property bool rowVisible: enabledRow && rowCount > 0

    signal activated(string value)

    width: parent ? parent.width : implicitWidth
    height: rowVisible ? headerHeight + 8 + chipHeight : 0
    implicitHeight: height
    visible: rowVisible
    focus: true

    function focusList() {
        if (rowCount <= 0)
            return false
        listView.forceActiveFocus()
        listView.currentIndex = Math.max(0, Math.min(currentIndex, rowCount - 1))
        ensureVisible()
        return true
    }

    function ensureVisible() {
        if (listView.currentIndex >= 0)
            listView.positionViewAtIndex(listView.currentIndex, ListView.Contain)
    }

    function handlePressedKey(key) {
        return false
    }

    function handleNavigationKey(key) {
        if (rowCount <= 0)
            return false
        const acceptKey = InputKeys.isAccept(key)
        if (key === Qt.Key_Left) {
            if (listView.currentIndex > 0)
                listView.currentIndex = listView.currentIndex - 1
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
            if (listView.currentIndex >= 0 && listView.currentIndex < rowCount)
                activated(values[listView.currentIndex])
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
        anchors.topMargin: 8
        height: root.chipHeight
        visible: root.rowCount > 0
        focus: true
        keyNavigationEnabled: false
        clip: true
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds
        spacing: root.rowGap
        model: root.values
        currentIndex: root.rowCount > 0 ? Math.max(0, Math.min(root.currentIndex, root.rowCount - 1)) : -1
        onCurrentIndexChanged: {
            root.currentIndex = currentIndex
            root.ensureVisible()
        }
        FastWheelHandler { flickable: listView; horizontal: true }

        delegate: FocusScope {
            id: chip
            required property int index
            required property var modelData
            readonly property bool current: index === listView.currentIndex && listView.activeFocus
            width: chipLabel.implicitWidth + 28
            height: root.chipHeight
            Rectangle {
                anchors.fill: parent
                radius: height / 2
                color: chip.current ? Theme.accentPanel : Theme.bgRaised
                border.width: chip.current ? 2 : 1
                border.color: chip.current ? Theme.accent : Theme.border
                antialiasing: true
            }

            AppText {
                id: chipLabel
                anchors.centerIn: parent
                text: chip.modelData
                color: chip.current ? Theme.textPrimary : Theme.textSecondary
                font.pixelSize: Metrics.metaPx(root.width) + 1
                font.weight: Font.Medium
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    listView.currentIndex = chip.index
                    root.currentIndex = chip.index
                    root.activated(chip.modelData)
                }
            }
        }

        Keys.onReleased: (event) => {
            if (root.handleNavigationKey(event.key))
                event.accepted = true
        }
    }
}
