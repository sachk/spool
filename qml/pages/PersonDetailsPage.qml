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
        if (itemCount > 0) {
            knownFor.forceActiveFocus()
            ensurePersonItemVisible(knownFor)
        }
    }

    function ensurePersonItemVisible(item) {
        if (!item || !personFlick || !contentColumn)
            return
        Qt.callLater(function() {
            const mapped = item.mapToItem(contentColumn, 0, 0)
            const top = Math.max(0, mapped.y - 18)
            const bottom = mapped.y + item.height + 18
            const maxY = Math.max(0, personFlick.contentHeight - personFlick.height)
            if (top < personFlick.contentY)
                personFlick.contentY = Math.max(0, top)
            else if (bottom > personFlick.contentY + personFlick.height)
                personFlick.contentY = Math.min(maxY, bottom - personFlick.height)
        })
    }

    function refreshCount() {
        itemCount = appController && appController.personItems ? appController.personItems.rowCount() : 0
        currentIndex = itemCount > 0 ? Math.max(0, Math.min(currentIndex, itemCount - 1)) : -1
        knownFor.currentIndex = currentIndex
        if (activeFocus)
            Qt.callLater(focusKnownFor)
    }

    function currentCard() {
        return knownFor.currentCard ? knownFor.currentCard() : null
    }

    function handlePressedKey(key) {
        const card = currentCard()
        return card && card.handleAcceptPressed ? card.handleAcceptPressed(key) : false
    }

    function openCurrent() {
        if (currentIndex >= 0)
            shell.openDetailsAt(appController.personItems, currentIndex, "person", "personDetails")
    }

    function handleNavigationKey(key) {
        if (itemCount <= 0)
            return false
        return knownFor.handleNavigationKey ? knownFor.handleNavigationKey(key) : false
    }

    Rectangle { anchors.fill: parent; color: Theme.bg }

    Flickable {
        id: personFlick
        anchors.fill: parent
        contentWidth: width
        contentHeight: Math.max(height, contentColumn.implicitHeight + root.contentMargin * 2)
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        FastWheelHandler { flickable: personFlick }

        ColumnLayout {
            id: contentColumn
            x: root.contentMargin
            y: root.contentMargin
            width: personFlick.width - root.contentMargin * 2
            height: implicitHeight
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

            MediaPosterScrollerRow {
                id: knownFor
                Layout.fillWidth: true
                title: "Known For"
                rowModel: appController ? appController.personItems : null
                shell: root.shell
                cardWidth: root.knownForCardWidth
                rowGap: Metrics.gap(root.width)
                enabledRow: root.itemCount > 0
                currentIndex: root.currentIndex
                onCurrentIndexChanged: root.currentIndex = knownFor.currentIndex
                onActivated: (index) => {
                    root.currentIndex = index
                    root.openCurrent()
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
