import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root

    property var shell
    readonly property var person: shell ? shell.personItem : ({})
    property int itemCount: appController && appController.personItems ? appController.personItems.rowCount() : 0
    property int currentIndex: itemCount > 0 ? 0 : -1
    readonly property int contentMargin: Metrics.pageMargin(width)
    readonly property int portraitWidth: Math.min(176, Math.max(128, width * 0.1))
    readonly property int knownForCardWidth: width >= 1600 ? 170 : 145
    readonly property int knownForCardHeight: Math.round(knownForCardWidth * 1.5 + 68)
    focus: true

    Component.onCompleted: {
        loadPerson()
        Qt.callLater(focusKnownFor)
    }
    onPersonChanged: Qt.callLater(loadPerson)
    onActiveFocusChanged: if (activeFocus) focusKnownFor()

    Connections {
        target: appController ? appController.personItems : null
        function onModelReset() { root.refreshCount() }
        function onRowsInserted() { root.refreshCount() }
        function onRowsRemoved() { root.refreshCount() }
    }

    Connections {
        target: appController
        function onPersonItemsChanged() { root.refreshCount() }
    }

    function loadPerson() {
        currentIndex = 0
        if (appController && person && person.personId)
            appController.loadPersonItems(person.personId)
    }

    function focusKnownFor() {
        if (itemCount > 0)
            knownFor.forceActiveFocus()
    }

    function refreshCount() {
        itemCount = appController && appController.personItems ? appController.personItems.rowCount() : 0
        currentIndex = itemCount > 0 ? Math.max(0, Math.min(currentIndex, itemCount - 1)) : -1
        knownFor.currentIndex = currentIndex
        if (activeFocus)
            Qt.callLater(focusKnownFor)
    }

    function itemAt(index) {
        return appController && appController.personItems && index >= 0 && index < appController.personItems.rowCount()
             ? appController.personItems.get(index)
             : ({})
    }

    function currentCard() {
        return knownFor.currentItem
    }

    function handlePressedKey(key) {
        const card = currentCard()
        return card && card.handleAcceptPressed ? card.handleAcceptPressed(key) : false
    }

    function openCurrent() {
        if (currentIndex >= 0)
            shell.openDetails(appController.personItems, currentIndex, "person", "personDetails")
    }

    function ensureVisible() {
        if (knownFor.currentIndex >= 0)
            knownFor.positionViewAtIndex(knownFor.currentIndex, ListView.Contain)
    }

    function handleNavigationKey(key) {
        const acceptKey = key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select || key === Qt.Key_Space
        const card = currentCard()
        if (!acceptKey && card && card.handleNavigationKey && card.handleNavigationKey(key))
            return true
        if (key === Qt.Key_Left) {
            if (currentIndex <= 0) shell.focusRail()
            else currentIndex = currentIndex - 1
            knownFor.currentIndex = currentIndex
            ensureVisible()
            return true
        }
        if (key === Qt.Key_Right) {
            currentIndex = Math.min(itemCount - 1, currentIndex + 1)
            knownFor.currentIndex = currentIndex
            ensureVisible()
            return true
        }
        if (acceptKey) {
            if (card && card.handleAcceptReleased && card.handleAcceptReleased(key))
                return true
            if (card && card.handleNavigationKey && card.handleNavigationKey(key))
                return true
            openCurrent()
            return true
        }
        return false
    }

    Rectangle { anchors.fill: parent; color: Theme.bg }

    Flickable {
        id: personFlick
        anchors.fill: parent
        contentWidth: width
        contentHeight: Math.max(height, contentColumn.implicitHeight + root.contentMargin * 2)
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: contentColumn
            x: root.contentMargin
            y: root.contentMargin
            width: personFlick.width - root.contentMargin * 2
            spacing: 22

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(172, root.portraitWidth * 1.22)
                spacing: 22

                ImageCard {
                    Layout.preferredWidth: root.portraitWidth
                    Layout.preferredHeight: Math.round(root.portraitWidth * 1.22)
                    Layout.alignment: Qt.AlignTop
                    imageUrl: person.imageUrl || ""
                    fallbackText: person.type || "Person"
                    focused: false
                    retainWhileLoading: true
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 9

                    AppText {
                        Layout.fillWidth: true
                        text: person.name || "Person"
                        font.pixelSize: Math.min(46, Metrics.titlePx(root.width) + 8)
                        font.weight: Font.DemiBold
                        maximumLineCount: 2
                        wrapMode: Text.Wrap
                    }

                    TechMetadataLine {
                        Layout.fillWidth: true
                        metadata: [person.type || "", person.role || ""].filter(function(v) { return v.length > 0 }).join(" / ")
                    }
                }
            }

            SectionHeader {
                Layout.fillWidth: true
                title: "Known For"
            }

            ListView {
                id: knownFor
                Layout.fillWidth: true
                Layout.preferredHeight: root.itemCount > 0 ? root.knownForCardHeight : 0
                visible: root.itemCount > 0
                focus: true
                keyNavigationEnabled: false
                clip: true
                orientation: ListView.Horizontal
                boundsBehavior: Flickable.StopAtBounds
                spacing: Metrics.gap(root.width)
                model: appController ? appController.personItems : null
                currentIndex: root.currentIndex

                delegate: MediaItemCard {
                    required property int index
                    required property string movieId
                    readonly property var itemData: root.itemAt(index)
                    width: root.knownForCardWidth
                    height: root.knownForCardHeight
                    item: itemData
                    kind: "poster"
                    useSeriesPoster: true
                    focused: ListView.isCurrentItem && knownFor.activeFocus
                    onActivated: {
                        knownFor.currentIndex = index
                        root.currentIndex = index
                        root.openCurrent()
                    }
                    onFavoriteToggled: (favorite) => appController.setFavorite(movieId || "", favorite)
                    onPlayedToggled: (played) => appController.setPlayed(movieId || "", played)
                    onMediaInfoRequested: shell.openMediaInfo(itemData)
                }

                Keys.onReleased: (event) => {
                    if (root.handleNavigationKey(event.key))
                        event.accepted = true
                }
            }

            EmptyPlaceholder {
                Layout.fillWidth: true
                visible: root.itemCount === 0 && !(appController && appController.personItemsBusy)
                title: "No items"
                detail: "This person has no matching movies or episodes in your libraries."
            }
        }
    }
}
