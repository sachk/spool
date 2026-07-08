import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root

    property var shell
    readonly property var resumeModel: homeController ? homeController.resumeItems : null
    readonly property var nextUpModel: homeController ? homeController.nextUpItems : null
    readonly property var librariesModel: libraryModel
    property var latestRows: homeController ? homeController.latestLibraryRows : []
    readonly property bool librariesOnly: router && router.route === "libraries"
    property int currentSection: 0

    function modelFor(source, rowIndex) {
        if (source === "libraries")
            return librariesModel
        if (source === "resumeItems")
            return resumeModel
        if (source === "nextUpItems")
            return nextUpModel
        if (source === "latestLibrary")
            return homeController ? homeController.latestLibraryItems(rowIndex) : null
        return null
    }

    focus: true

    Connections {
        target: homeController
        function onLatestLibraryRowsChanged() {
            root.latestRows = homeController ? homeController.latestLibraryRows : []
            root.scheduleFocusRepair()
        }
    }

    Timer {
        id: focusRepairTimer
        interval: 0
        repeat: false
        onTriggered: root.ensureValidFocus()
    }

    function scheduleFocusRepair() {
        focusRepairTimer.restart()
    }

    function rowHeight(kind) {
        if (kind === "library")
            return 276
        if (kind === "poster")
            return 338
        return 304
    }

    function modelItem(source, rowIndex, index) {
        const model = modelFor(source, rowIndex)
        if (!model || typeof model.rowCount !== "function" || index < 0 || index >= model.rowCount())
            return null
        return model.get(index)
    }

    function activateAt(source, index, rowIndex) {
        const model = modelFor(source, rowIndex)
        if (!model || typeof model.rowCount !== "function" || index < 0 || index >= model.rowCount())
            return
        if (source === "resumeItems") {
            appController.playFromModel(model, index)
            return
        }
        if (source === "nextUpItems") {
            shell.openDetailsAt(nextUpModel, index, "nextup", "home")
            return
        }
        if (source === "libraries") {
            shell.lastLibraryIndex = index
            appController.openLibrary(index)
            if (shell)
                shell.replaceRoute("libraryGrid")
            return
        }
        if (source === "latestLibrary")
            shell.openDetailsAt(model, index, "latestLibrary:" + rowIndex, "home")
    }

    function setFavoriteAt(source, rowIndex, index, favorite) {
        const item = modelItem(source, rowIndex, index)
        if (item && item.movieId)
            appController.setFavorite(item.movieId, favorite)
    }

    function setPlayedAt(source, rowIndex, index, played) {
        const item = modelItem(source, rowIndex, index)
        if (item && item.movieId)
            appController.setPlayed(item.movieId, played)
    }

    function openMediaInfoAt(source, rowIndex, index) {
        const item = modelItem(source, rowIndex, index)
        if (item)
            shell.openMediaInfo(item)
    }

    function visibleSections() {
        const rows = []
        if (librariesRow.rowVisible)
            rows.push(librariesRow)
        if (librariesOnly)
            return rows
        if (resumeRow.rowVisible)
            rows.push(resumeRow)
        if (nextUpRow.rowVisible)
            rows.push(nextUpRow)
        for (let i = 0; i < latestRepeater.count; ++i) {
            const row = latestRepeater.itemAt(i)
            if (row && row.rowVisible)
                rows.push(row)
        }
        return rows
    }

    function activeSection() {
        const rows = visibleSections()
        if (rows.length <= 0)
            return null
        currentSection = Math.max(0, Math.min(currentSection, rows.length - 1))
        return rows[currentSection]
    }

    function ensureItemVisible(item) {
        if (!item)
            return
        const margin = Math.round(20 * Math.max(0.78, Math.min(1.0, root.height / 1440)))
        const top = Math.max(0, item.y - margin)
        const bottom = item.y + item.height + margin
        const viewportTop = scroller.contentY
        const viewportBottom = scroller.contentY + scroller.height
        const maxY = Math.max(0, scroller.contentHeight - scroller.height)
        if (top < viewportTop)
            scroller.contentY = Math.max(0, top)
        else if (bottom > viewportBottom)
            scroller.contentY = Math.min(maxY, bottom - scroller.height)
    }

    function focusCurrentSection() {
        const section = activeSection()
        if (!section)
            return false
        section.focusList()
        ensureItemVisible(section)
        return true
    }

    function ensureValidFocus() {
        const rows = visibleSections()
        if (rows.length <= 0)
            return
        currentSection = Math.max(0, Math.min(currentSection, rows.length - 1))
        if (activeFocus)
            focusCurrentSection()
    }

    function focusRelative(section, direction) {
        const rows = visibleSections()
        if (rows.length <= 0)
            return
        const index = rows.indexOf(section)
        currentSection = index >= 0 ? index : Math.max(0, Math.min(currentSection, rows.length - 1))
        const next = currentSection + direction
        if (next < 0) {
            shell.focusNavBar()
            return
        }
        if (next >= rows.length)
            return
        currentSection = next
        focusCurrentSection()
    }

    function handlePressedKey(key) {
        const section = activeSection()
        return section && section.handlePressedKey ? section.handlePressedKey(key) : false
    }

    function handleKey(key) {
        const section = activeSection()
        if (section && section.handleKey && section.handleKey(key))
            return true
        if (key === Qt.Key_Up) {
            focusRelative(section, -1)
            return true
        }
        if (key === Qt.Key_Down) {
            focusRelative(section, 1)
            return true
        }
        return false
    }

    Component.onCompleted: {
        scheduleFocusRepair()
    }
    onActiveFocusChanged: if (activeFocus)
                              focusCurrentSection()

    Flickable {
        id: scroller

        anchors.fill: parent
        anchors.margins: Metrics.pageMargin(width)
        clip: true
        contentWidth: width
        contentHeight: contentColumn.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        maximumFlickVelocity: 7200
        flickDeceleration: 6200

        FastWheelHandler {
            flickable: scroller
        }

        Behavior on contentY {
            enabled: !Theme.reducedMotion
            NumberAnimation {
                duration: 90
                easing.type: Easing.OutCubic
            }
        }

        Column {
            id: contentColumn

            width: scroller.width
            spacing: 14

            SectionHeader {
                width: contentColumn.width
                height: implicitHeight + 18
                title: root.librariesOnly ? "Libraries" : "My Media"
            }

            HomeHorizontalRow {
                id: librariesRow

                width: contentColumn.width
                height: rowVisible ? root.rowHeight("library") : 0
                visible: rowVisible
                title: "Libraries"
                rowModel: root.librariesModel
                shell: root.shell
                rowKind: "library"
                cardWidth: Metrics.homeLandscapeWidth(root.width)
                cardGap: Metrics.gap(root.width)
                onRowVisibleChanged: root.scheduleFocusRepair()
                onMoveVertical: direction => root.focusRelative(librariesRow, direction)
                onActivated: index => root.activateAt("libraries", index, -1)
            }

            HomeHorizontalRow {
                id: resumeRow

                width: contentColumn.width
                height: !root.librariesOnly && rowVisible ? root.rowHeight("landscape") : 0
                visible: !root.librariesOnly && rowVisible
                title: "Continue Watching"
                rowModel: root.resumeModel
                shell: root.shell
                rowKind: "landscape"
                cardWidth: Metrics.homeLandscapeWidth(root.width)
                cardGap: Metrics.gap(root.width)
                onRowVisibleChanged: root.scheduleFocusRepair()
                onMoveVertical: direction => root.focusRelative(resumeRow, direction)
                onActivated: index => root.activateAt("resumeItems", index, -1)
                onFavoriteToggled: (index, favorite) => root.setFavoriteAt("resumeItems", -1, index, favorite)
                onPlayedToggled: (index, played) => root.setPlayedAt("resumeItems", -1, index, played)
                onMediaInfoRequested: index => root.openMediaInfoAt("resumeItems", -1, index)
            }

            HomeHorizontalRow {
                id: nextUpRow

                width: contentColumn.width
                height: !root.librariesOnly && rowVisible ? root.rowHeight("landscape") : 0
                visible: !root.librariesOnly && rowVisible
                title: "Next Up"
                rowModel: root.nextUpModel
                shell: root.shell
                rowKind: "landscape"
                cardWidth: Metrics.homeLandscapeWidth(root.width)
                cardGap: Metrics.gap(root.width)
                onRowVisibleChanged: root.scheduleFocusRepair()
                onMoveVertical: direction => root.focusRelative(nextUpRow, direction)
                onActivated: index => root.activateAt("nextUpItems", index, -1)
                onFavoriteToggled: (index, favorite) => root.setFavoriteAt("nextUpItems", -1, index, favorite)
                onPlayedToggled: (index, played) => root.setPlayedAt("nextUpItems", -1, index, played)
                onMediaInfoRequested: index => root.openMediaInfoAt("nextUpItems", -1, index)
            }

            Repeater {
                id: latestRepeater

                model: root.latestRows || []

                HomeHorizontalRow {
                    id: latestRow

                    required property int index
                    required property var modelData
                    readonly property int sourceRowIndex: Number(modelData && modelData.rowIndex !== undefined
                                                                 ? modelData.rowIndex : index)

                    width: contentColumn.width
                    height: !root.librariesOnly && rowVisible ? root.rowHeight(rowKind) : 0
                    visible: !root.librariesOnly && rowVisible
                    title: modelData && modelData.title ? modelData.title : "Recently Added"
                    rowModel: homeController ? homeController.latestLibraryItems(sourceRowIndex) : null
                    shell: root.shell
                    rowKind: modelData && modelData.kind ? modelData.kind : "poster"
                    useSeriesPoster: true
                    cardWidth: rowKind === "poster" ? Metrics.homePosterWidth(root.width) : Metrics.homeLandscapeWidth(
                                                          root.width)
                    cardGap: Metrics.gap(root.width)
                    onRowVisibleChanged: root.scheduleFocusRepair()
                    onMoveVertical: direction => root.focusRelative(latestRow, direction)
                    onActivated: itemIndex => root.activateAt("latestLibrary", itemIndex, sourceRowIndex)
                    onFavoriteToggled: (itemIndex, favorite) => root.setFavoriteAt("latestLibrary", sourceRowIndex,
                                                                                   itemIndex, favorite)
                    onPlayedToggled: (itemIndex, played) => root.setPlayedAt("latestLibrary", sourceRowIndex, itemIndex,
                                                                             played)
                    onMediaInfoRequested: itemIndex => root.openMediaInfoAt("latestLibrary", sourceRowIndex, itemIndex)
                }
            }
        }
    }
}
