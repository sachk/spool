import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root

    property var shell
    readonly property var person: shell ? shell.personItem : ({})
    property int itemCount: Content.personItems ? Content.personItems.rowCount() : 0
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
    onActiveFocusChanged: if (activeFocus)
                              focusKnownFor()

    Connections {
        target: Content.personItems
        function onModelReset() {
            root.refreshCount()
        }
        function onRowsInserted() {
            root.refreshCount()
        }
        function onRowsRemoved() {
            root.refreshCount()
        }
    }

    Connections {
        target: Content
        function onPersonItemsChanged() {
            root.refreshCount()
        }
    }

    function loadPerson() {
        currentIndex = 0
        if (Content && person && person.personId)
            Content.loadPersonItems(person.personId)
    }

    function focusKnownFor() {
        if (itemCount > 0) {
            InputKeys.focus(knownFor)
            InputKeys.ensureVisible(personFlick, knownFor)
        }
    }

    function refreshCount() {
        itemCount = Content.personItems ? Content.personItems.rowCount() : 0
        currentIndex = itemCount > 0 ? Math.max(0, Math.min(currentIndex, itemCount - 1)) : -1
        knownFor.currentIndex = currentIndex
        if (activeFocus)
            Qt.callLater(focusKnownFor)
    }

    function routeKey(key, phase, repeat) {
        if (key === Qt.Key_Up && phase === "press") {
            if (shell)
                shell.focusNavBar()
            return true
        }
        if (key === Qt.Key_Down)
            return true
        return knownFor.routeKey(key, phase, repeat)
    }

    function activate() {
        knownFor.activate()
    }

    function longPress() {
        return knownFor.longPress()
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
    }

    Flickable {
        id: personFlick
        anchors.fill: parent
        contentWidth: width
        contentHeight: Math.max(height, contentColumn.implicitHeight + root.contentMargin * 2)
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        FastWheelHandler {
            flickable: personFlick
        }

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
                    imageUrl: Art.url(person, "poster", Math.ceil(root.portraitWidth))
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
                        metadata: [person.type || "", person.role || ""].filter(function (v) {
                            return v.length > 0
                        }).join(" / ")
                    }
                }
            }

            MediaRow {
                id: knownFor
                Layout.fillWidth: true
                title: "Known For"
                model: Content.personItems
                shell: root.shell
                cardWidth: root.knownForCardWidth
                cardGap: Metrics.gap(root.width)
                enabledRow: root.itemCount > 0
                currentIndex: root.currentIndex
                onCurrentIndexChanged: root.currentIndex = knownFor.currentIndex
                onActivated: index => {
                                 root.currentIndex = index
                                 root.openCurrent()
                             }
            }

            EmptyPlaceholder {
                Layout.fillWidth: true
                visible: root.itemCount === 0 && !Content.personItemsBusy
                title: "No items"
                detail: "This person has no matching movies or episodes in your libraries."
            }
        }
    }
}
