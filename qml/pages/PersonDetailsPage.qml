pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root

    property var shell
    readonly property var person: shell ? shell.personItem : ({})
    readonly property var sections: Content.personItemRows || []
    readonly property int contentMargin: Metrics.pageMarginPx
    readonly property int portraitWidth: Math.min(176, Math.max(128, width * 0.1))
    readonly property bool contentReady: !Content.personItemsBusy
    focus: true

    Component.onCompleted: rebuildSections()
    onSectionsChanged: rebuildSections()
    onActiveFocusChanged: if (activeFocus)
    Qt.callLater(focusCurrentSection)

    function modelCount(model) {
        if (!model)
            return 0
        if (model.count !== undefined)
            return Number(model.count)
        return model.rowCount ? Number(model.rowCount()) : 0
    }

    function itemAt(model, index) {
        if (!model || index < 0 || index >= modelCount(model))
            return ({})
        return model.get ? (model.get(index) || ({})) : ({})
    }

    function firstPopulatedSection(start, direction) {
        for (let index = start; index >= 0 && index < sections.length; index += direction) {
            if (modelCount(sections[index].model) > 0)
                return index
        }
        return -1
    }

    function rebuildSections() {
        sectionList.currentIndex = firstPopulatedSection(0, 1)
        Qt.callLater(focusCurrentSection)
    }

    function currentRow() {
        return sectionList.currentIndex >= 0 ? sectionList.itemAtIndex(sectionList.currentIndex) : null
    }

    function focusCurrentSection() {
        const row = currentRow()
        if (!row || !row.focusList())
            return false
        sectionList.positionViewAtIndex(sectionList.currentIndex, ListView.Contain)
        return true
    }

    function moveSection(direction) {
        const next = firstPopulatedSection(sectionList.currentIndex + direction, direction)
        if (next < 0) {
            if (direction < 0) {
                revealHeader()
            }
            if (direction < 0 && shell)
                shell.focusNavBar()
            return true
        }
        sectionList.currentIndex = next
        sectionList.positionViewAtIndex(next, ListView.Contain)
        Qt.callLater(focusCurrentSection)
        return true
    }

    function revealHeader() {
        sectionList.positionViewAtBeginning()
    }

    function routeKey(key, phase, repeat) {
        const row = currentRow()
        if (!row)
            return false
        if (key === Qt.Key_Up || key === Qt.Key_Down)
            return moveSection(key === Qt.Key_Down ? 1 : -1)
        return row.routeKey(key, phase, repeat)
    }

    function openAt(descriptor, index) {
        if (shell && descriptor && descriptor.model)
            shell.openDetailsAt(descriptor.model, index, "person", "personDetails")
    }

    function activate() {
        const row = currentRow()
        if (row)
            openAt(sections[sectionList.currentIndex], row.currentIndex)
    }

    function longPress() {
        const row = currentRow()
        return Boolean(row && row.longPress && row.longPress())
    }

    function currentMediaItem() {
        const row = currentRow()
        return row ? itemAt(sections[sectionList.currentIndex].model, row.currentIndex) : ({})
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
    }

    ListView {
        id: sectionList

        anchors.fill: parent
        anchors.margins: root.contentMargin
        model: root.sections
        spacing: Metrics.sectionGapPx
        clip: true
        reuseItems: true
        cacheBuffer: 0
        boundsBehavior: Flickable.StopAtBounds
        keyNavigationEnabled: false
        focus: true

        FastWheelHandler {
            flickable: sectionList
        }

        header: RowLayout {
            width: sectionList.width
            height: Math.max(172, root.portraitWidth * 1.22) + Metrics.scaled(22)
            spacing: Metrics.scaled(22)

            ImageCard {
                Layout.preferredWidth: root.portraitWidth
                Layout.preferredHeight: Math.round(root.portraitWidth * 1.22)
                Layout.alignment: Qt.AlignTop
                imageUrl: Art.url(root.person, "poster")
                fallbackIcon: "person"
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: Metrics.scaled(9)

                AppText {
                    Layout.fillWidth: true
                    text: root.person.name || "Person"
                    font.pixelSize: Math.min(46, Metrics.titleSizePx + 8)
                    font.weight: Font.DemiBold
                    maximumLineCount: 2
                    wrapMode: Text.Wrap
                }

                TechMetadataLine {
                    Layout.fillWidth: true
                    metadata: [root.person.type || "", root.person.role || ""].filter(function (value) {
                        return value.length > 0
                    }).join(" / ")
                }
            }
        }

        delegate: MediaRow {
            id: mediaRow

            required property int index
            required property var modelData
            readonly property bool episodeRow: String(modelData.kind || "") === "landscape"

            width: sectionList.width
            title: String(modelData.title || "")
            model: modelData.model
            shell: root.shell
            cardKind: episodeRow ? "landscape" : "poster"
            preferEpisodeTitle: episodeRow
            cardWidth: episodeRow ? Metrics.landscapeCardWidth(root.width) : Metrics.cardWidth(root.width)
            cardGap: Metrics.gapPx
            itemContextSource: "person"
            itemContextReturnRoute: "personDetails"
            onActivated: itemIndex => root.openAt(modelData, itemIndex)
        }

        footer: EmptyPlaceholder {
            width: sectionList.width
            height: sectionList.height
            visible: root.sections.length === 0 && !Content.personItemsBusy
            title: "No items"
            detail: "No matching credits in your libraries."
        }
    }
}
