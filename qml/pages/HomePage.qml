pragma ComponentBehavior: Bound

import QtQuick
import "../theme"
import "../primitives"

FocusScope {
    id: root

    property var shell
    property var uiTransitionToken: 0
    property var sections: []
    property bool firstRowReady: false
    readonly property bool contentReady: firstRowReady
    property bool navigationFocusVisible: true
    property bool pointerNavigationPending: false
    property var pendingScrollController: null

    focus: true

    function buildSections() {
        const rows = [
                  {
                      "source": "libraries",
                      "title": "Libraries",
                      "model": Libraries,
                      "kind": "library"
                  }
              ]
        rows.push({
                      "source": "resumeItems",
                      "title": "Continue Watching",
                      "model": Home.resumeItems,
                      "kind": "landscape"
                  }, {
                      "source": "nextUpItems",
                      "title": "Next Up",
                      "model": Home.nextUpItems,
                      "kind": "landscape"
                  })
        const latest = Home.latestLibraryRows || []
        for (let index = 0; index < latest.length; ++index) {
            const row = latest[index]
            rows.push({
                          "source": "latestLibrary",
                          "title": row && row.title ? row.title : "Recently Added",
                          "model": row && row.model ? row.model : null,
                          "kind": row && row.kind ? row.kind : "poster",
                          "sourceRowIndex": Number(row && row.rowIndex !== undefined ? row.rowIndex : index)
                      })
        }
        return rows
    }

    function modelCount(model) {
        if (!model)
            return 0
        if (model.count !== undefined)
            return Number(model.count)
        if (model.length !== undefined)
            return Number(model.length)
        return model.rowCount ? Number(model.rowCount()) : 0
    }

    function itemAt(model, index) {
        if (!model || index < 0 || index >= modelCount(model))
            return ({})
        return model.get ? (model.get(index) || ({})) : (model[index] || ({}))
    }

    function rebuildSections() {
        firstRowReady = false
        sections = buildSections()
        sectionList.currentIndex = firstPopulatedSection(0, 1)
        Qt.callLater(focusCurrentSection)
    }

    function firstPopulatedSection(start, direction) {
        for (let index = start; index >= 0 && index < sections.length; index += direction) {
            if (modelCount(sections[index].model) > 0)
                return index
        }
        return -1
    }

    function currentRow() {
        return sectionList.currentIndex >= 0 ? sectionList.itemAtIndex(sectionList.currentIndex) : null
    }

    function focusCurrentSection() {
        const row = currentRow()
        if (!row)
            return false
        if (!row.focusList())
            return false
        sectionList.positionViewAtIndex(sectionList.currentIndex, ListView.Contain)
        return true
    }

    function moveSection(direction) {
        const next = firstPopulatedSection(sectionList.currentIndex + direction, direction)
        if (next < 0) {
            if (direction < 0 && shell)
                shell.focusNavBar()
            return true
        }
        sectionList.currentIndex = next
        sectionList.positionViewAtIndex(next, ListView.Contain)
        Qt.callLater(focusCurrentSection)
        return true
    }

    function clearPendingPointerNavigation() {
        pointerNavigationPending = false
        pendingScrollController = null
    }

    function beginPointerNavigation(controller) {
        if (controller)
            pendingScrollController = controller
        pointerNavigationPending = true
        navigationFocusVisible = false
    }

    function topLeftVisibleCard() {
        sectionList.forceLayout()
        let best = null
        for (let rowIndex = 0; rowIndex < sectionList.count; ++rowIndex) {
            const row = sectionList.itemAtIndex(rowIndex)
            if (!row || !row.topLeftVisibleCandidate)
                continue
            const card = row.topLeftVisibleCandidate(sectionList)
            if (!card)
                continue
            const candidate = {
                "index": card.index,
                "top": card.top,
                "left": card.left,
                "rowIndex": rowIndex,
                "row": row
            }
            if (!best || InputKeys.earlierVisibleCandidate(candidate, best))
                best = candidate
        }
        return best
    }

    function recoverPointerNavigation() {
        if (pendingScrollController && pendingScrollController.stopScrolling)
            pendingScrollController.stopScrolling()
        sectionList.cancelFlick()
        const candidate = topLeftVisibleCard()
        if (!candidate)
            return true
        const contentY = sectionList.contentY
        if (!InputKeys.focusIndexWithoutScrolling(sectionList, candidate.rowIndex))
            return true
        if (!candidate.row.focusIndexWithoutScrolling(candidate.index))
            return true
        sectionList.contentY = contentY
        navigationFocusVisible = true
        clearPendingPointerNavigation()
        return true
    }

    function routeKey(key, phase, repeat) {
        if (phase !== "release" && InputKeys.isDirection(key) && pointerNavigationPending)
            return recoverPointerNavigation()
        const row = currentRow()
        if (!row)
            return false
        if (key === Qt.Key_Up || key === Qt.Key_Down)
            return moveSection(key === Qt.Key_Down ? 1 : -1)
        return row.routeKey(key, phase, repeat)
    }

    function activateAt(descriptor, index) {
        const model = descriptor.model
        if (!model || index < 0 || index >= modelCount(model))
            return
        if (descriptor.source === "resumeItems") {
            App.playFromModel(model, index)
        } else if (descriptor.source === "libraries") {
            App.openLibrary(index)
            if (shell)
                shell.replaceRoute("libraryGrid", {
                                       libraryId: String(model.get(index).libraryId || ""),
                                       focusIndex: 0
                                   })
        } else if (shell) {
            const source = descriptor.source === "latestLibrary" ? "latestLibrary:" + descriptor.sourceRowIndex :
                                                                   "nextup"
            shell.openDetailsAt(model, index, source, "home")
        }
    }

    function activate() {
        const row = currentRow()
        if (row)
            activateAt(sections[sectionList.currentIndex], row.currentIndex)
    }

    function longPress() {
        const row = currentRow()
        return Boolean(row && row.longPress && row.longPress())
    }

    function currentMediaItem() {
        const row = currentRow()
        return row ? itemAt(sections[sectionList.currentIndex].model, row.currentIndex) : ({})
    }

    Component.onCompleted: rebuildSections()
    onActiveFocusChanged: {
        clearPendingPointerNavigation()
        if (activeFocus) {
            navigationFocusVisible = true
            Qt.callLater(focusCurrentSection)
        }
    }

    Connections {
        target: Home
        function onLatestLibraryRowsChanged() {
            root.rebuildSections()
        }
    }

    ListView {
        id: sectionList

        anchors.fill: parent
        anchors.leftMargin: Metrics.pageMarginPx
        anchors.rightMargin: Metrics.pageMarginPx
        anchors.topMargin: Metrics.pageMarginPx
        anchors.bottomMargin: 0
        model: root.sections
        spacing: Metrics.scaled(14)
        clip: true
        reuseItems: true
        cacheBuffer: 0
        boundsBehavior: Flickable.StopAtBounds
        keyNavigationEnabled: false
        focus: true

        FastWheelHandler {
            id: sectionWheelHandler
            flickable: sectionList
            onScrolled: root.beginPointerNavigation(sectionWheelHandler)
        }

        onMovementStarted: root.beginPointerNavigation(null)

        delegate: MediaRow {
            id: mediaRow

            required property int index
            required property var modelData

            width: sectionList.width
            title: modelData.title
            model: modelData.model
            shell: root.shell
            cardKind: modelData.kind
            useSeriesPoster: modelData.source === "latestLibrary"
            preferEpisodeTitle: modelData.source === "latestLibrary"
            focusVisible: root.navigationFocusVisible
            cardWidth: cardKind === "poster" ? Metrics.homePosterWidth(root.width) : Metrics.homeLandscapeWidth(
                                                   root.width)
            cardGap: Metrics.gapPx
            wheelFlickable: sectionList
            onVerticalWheelScrolled: controller => root.beginPointerNavigation(controller)
            onPointerSelected: root.clearPendingPointerNavigation()
            atomicPopulate: index === 0
            itemContextSource: String(modelData.source || "")
            itemContextReturnRoute: "home"
            onDelegatesPresentedChanged: if (index === 0 && delegatesPresented) {
                root.firstRowReady = true
                InputLatency.mark(root.uiTransitionToken, "first_delegate")
            }
            onActivated: itemIndex => root.activateAt(modelData, itemIndex)
        }
    }
}
