import QtQuick
import "../theme"

FocusScope {
    id: root

    property string title: "Cast & Crew"
    property var peopleModel: []
    property var shell
    property int currentIndex: 0
    property int rowGap: 16
    property bool enabledRow: true
    readonly property int rowCount: peopleModel ? peopleModel.length : 0
    readonly property int screenWidth: root.Window.window ? root.Window.window.width : 1920
    readonly property int density: Metrics.densityForWidth(screenWidth)
    readonly property int cardWidth: Math.min([156, 180, 220, 280][density], Math.max([124, 136, 152, 180][density], width * 0.13))
    readonly property int headerHeight: 34
    readonly property int cardHeight: Math.round(cardWidth * 1.5 + Metrics.metaPx(screenWidth) * 3 + 20)

    signal activated(var person)

    width: parent ? parent.width : implicitWidth
    height: visible ? headerHeight + 10 + cardHeight : 0
    implicitHeight: height
    visible: enabledRow && rowCount > 0
    focus: true
    clip: false

    function focusList() {
        InputKeys.focus(peopleList)
        peopleList.currentIndex = rowCount > 0 ? Math.max(0, Math.min(currentIndex, rowCount - 1)) : -1
        ensureVisible()
    }

    function ensureVisible() {
        if (peopleList.currentIndex >= 0)
            peopleList.positionViewAtIndex(peopleList.currentIndex, ListView.Contain)
    }

    function handlePressedKey(key) {
        return false
    }

    function handleKey(key) {
        if (rowCount <= 0)
            return false
        if (key === Qt.Key_Left) {
            if (peopleList.currentIndex > 0)
                peopleList.currentIndex = peopleList.currentIndex - 1
            currentIndex = peopleList.currentIndex
            ensureVisible()
            return true
        }
        if (key === Qt.Key_Right) {
            peopleList.currentIndex = Math.min(rowCount - 1, peopleList.currentIndex + 1)
            currentIndex = peopleList.currentIndex
            ensureVisible()
            return true
        }
        if (InputKeys.isAccept(key)) {
            currentIndex = peopleList.currentIndex
            activated(peopleModel[peopleList.currentIndex] || ({}))
            return true
        }
        return false
    }

    SectionHeader {
        id: peopleHeader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.headerHeight
        title: root.title
    }

    ListView {
        id: peopleList
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: peopleHeader.bottom
        anchors.topMargin: 10
        height: root.cardHeight
        focus: true
        keyNavigationEnabled: false
        clip: true
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds
        spacing: root.rowGap
        model: root.peopleModel
        currentIndex: root.rowCount > 0 ? Math.max(0, Math.min(root.currentIndex, root.rowCount - 1)) : -1
        onCurrentIndexChanged: {
            root.currentIndex = currentIndex
            root.ensureVisible()
        }
        FastWheelHandler { flickable: peopleList; horizontal: true }

        delegate: Item {
            id: personDelegate
            required property int index
            required property var modelData
            width: root.cardWidth
            height: peopleList.height
            ImageCard {
                id: personImage
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: width * 1.5
                imageUrl: modelData.imageUrl || ""
                fallbackText: modelData.type || "Person"
                focused: personDelegate.index === peopleList.currentIndex && peopleList.activeFocus
                retainWhileLoading: true
            }

            AppText {
                id: personName
                anchors.top: personImage.bottom
                anchors.topMargin: 8
                anchors.left: parent.left
                anchors.right: parent.right
                text: modelData.name || ""
                font.pixelSize: Metrics.metaPx(root.Window.window ? root.Window.window.width : 1920) + 1
                font.weight: Font.Medium
                color: personDelegate.index === peopleList.currentIndex && peopleList.activeFocus ? Theme.textPrimary : Theme.textSecondary
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            MonoText {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: personName.bottom
                anchors.topMargin: 2
                text: modelData.role || modelData.type || ""
                color: Theme.textMuted
                font.pixelSize: Metrics.metaPx(root.Window.window ? root.Window.window.width : 1920) - 1
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    peopleList.currentIndex = personDelegate.index
                    root.currentIndex = personDelegate.index
                    root.activated(personDelegate.modelData)
                }
            }
        }

        Keys.onReleased: (event) => {
            if (root.handleKey(event.key))
                event.accepted = true
        }
    }
}
