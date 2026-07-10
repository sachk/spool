pragma ComponentBehavior: Bound

import QtQuick
import "../theme"
import "../primitives"

FocusScope {
    id: root

    property var shell
    readonly property var resumeModel: Home.resumeItems
    readonly property var nextUpModel: Home.nextUpItems
    readonly property var librariesModel: Libraries
    property var latestRows: Home.latestLibraryRows
    readonly property bool librariesOnly: Router.route === "libraries"
    property int currentSection: 0

    function modelFor(source, rowIndex) {
        if (source === "libraries")
            return librariesModel
        if (source === "resumeItems")
            return resumeModel
        if (source === "nextUpItems")
            return nextUpModel
        if (source === "latestLibrary")
            return Home.latestLibraryItems(rowIndex)
        return null
    }

    focus: true

    Connections {
        target: Home
        function onLatestLibraryRowsChanged() {
            root.latestRows = Home.latestLibraryRows
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

    function activateAt(source, index, rowIndex) {
        const model = modelFor(source, rowIndex)
        if (!model || typeof model.rowCount !== "function" || index < 0 || index >= model.rowCount())
            return
        if (source === "resumeItems") {
            App.playFromModel(model, index)
            return
        }
        if (source === "nextUpItems") {
            shell.openDetailsAt(nextUpModel, index, "nextup", "home")
            return
        }
        if (source === "libraries") {
            shell.lastLibraryIndex = index
            App.openLibrary(index)
            if (shell)
                shell.replaceRoute("libraryGrid")
            return
        }
        if (source === "latestLibrary")
            shell.openDetailsAt(model, index, "latestLibrary:" + rowIndex, "home")
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

    function focusCurrentSection() {
        const section = activeSection()
        if (!section)
            return false
        section.focusList()
        InputKeys.positionChild(scroller, section)
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

    function routeKey(key, phase, repeat) {
        const section = activeSection()
        if (!section)
            return false
        if (key === Qt.Key_Up || key === Qt.Key_Down) {
            focusRelative(section, key === Qt.Key_Down ? 1 : -1)
            return true
        }
        return section.routeKey(key, phase, repeat)
    }

    function activate() {
        const section = activeSection()
        if (section)
            section.activate()
    }

    function longPress() {
        const section = activeSection()
        return Boolean(section && section.longPress && section.longPress())
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

            MediaRow {
                id: librariesRow

                width: contentColumn.width
                title: "Libraries"
                model: root.librariesModel
                shell: root.shell
                cardKind: "library"
                cardWidth: Metrics.homeLandscapeWidth(root.width)
                cardGap: Metrics.gap(root.width)
                onRowVisibleChanged: root.scheduleFocusRepair()
                onActivated: index => root.activateAt("libraries", index, -1)
            }

            MediaRow {
                id: resumeRow

                width: contentColumn.width
                title: "Continue Watching"
                model: root.resumeModel
                shell: root.shell
                cardKind: "landscape"
                cardWidth: Metrics.homeLandscapeWidth(root.width)
                cardGap: Metrics.gap(root.width)
                enabledRow: !root.librariesOnly
                onRowVisibleChanged: root.scheduleFocusRepair()
                onActivated: index => root.activateAt("resumeItems", index, -1)
            }

            MediaRow {
                id: nextUpRow

                width: contentColumn.width
                title: "Next Up"
                model: root.nextUpModel
                shell: root.shell
                cardKind: "landscape"
                cardWidth: Metrics.homeLandscapeWidth(root.width)
                cardGap: Metrics.gap(root.width)
                enabledRow: !root.librariesOnly
                onRowVisibleChanged: root.scheduleFocusRepair()
                onActivated: index => root.activateAt("nextUpItems", index, -1)
            }

            Repeater {
                id: latestRepeater

                model: root.latestRows || []

                MediaRow {
                    id: latestRow

                    required property int index
                    required property var modelData
                    readonly property int sourceRowIndex: Number(modelData && modelData.rowIndex !== undefined
                                                                 ? modelData.rowIndex : index)

                    width: contentColumn.width
                    title: modelData && modelData.title ? modelData.title : "Recently Added"
                    model: Home.latestLibraryItems(sourceRowIndex)
                    shell: root.shell
                    cardKind: modelData && modelData.kind ? modelData.kind : "poster"
                    useSeriesPoster: true
                    cardWidth: cardKind === "poster" ? Metrics.homePosterWidth(root.width) : Metrics.homeLandscapeWidth(
                                                           root.width)
                    cardGap: Metrics.gap(root.width)
                    enabledRow: !root.librariesOnly
                    onRowVisibleChanged: root.scheduleFocusRepair()
                    onActivated: itemIndex => root.activateAt("latestLibrary", itemIndex, sourceRowIndex)
                }
            }
        }
    }
}
