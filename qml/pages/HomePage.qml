import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    property var shell
    readonly property var resumeModel: appController ? appController.resumeItems : null
    readonly property var nextUpModel: appController ? appController.nextUpItems : null
    readonly property var latestModel: appController ? appController.latestItems : null
    property var latestRows: appController ? appController.latestLibraryRows : []
    readonly property var libraryModel: appController ? appController.libraries : null
    readonly property int resumeCount: resumeCountObserver.count
    readonly property int nextUpCount: nextUpCountObserver.count
    readonly property int latestCount: latestCountObserver.count
    property int latestLibraryRowCount: latestRows ? latestRows.length : 0
    readonly property int libraryCount: libraryCountObserver.count
    readonly property int spotlightIndex: latestCount > 0 ? 0 : -1
    property var spotlight: spotlightIndex >= 0 && latestModel ? latestModel.get(spotlightIndex) : ({})
    focus: true

    ModelCountObserver {
        id: latestCountObserver
        sourceModel: root.latestModel
        onCountChanged: root.rebuildSections()
    }
    ModelCountObserver {
        id: libraryCountObserver
        sourceModel: root.libraryModel
        onCountChanged: root.rebuildSections()
    }
    ModelCountObserver {
        id: resumeCountObserver
        sourceModel: root.resumeModel
        onCountChanged: root.rebuildSections()
    }
    ModelCountObserver {
        id: nextUpCountObserver
        sourceModel: root.nextUpModel
        onCountChanged: root.rebuildSections()
    }

    Connections {
        target: appController
        function onLatestLibraryRowsChanged() {
            root.latestRows = appController ? appController.latestLibraryRows : []
            root.latestLibraryRowCount = root.latestRows ? root.latestRows.length : 0
            root.rebuildSections()
        }
    }
    ListModel { id: sectionModel }

    function appendSection(title, kind, source, rowIndex) {
        sectionModel.append({ title: title, kind: kind, source: source, rowIndex: rowIndex === undefined ? -1 : rowIndex })
    }

    function rebuildSections() {
        if (!sectionModel)
            return
        sectionModel.clear()
        if (libraryCount > 0) appendSection("Libraries", "library", "libraries")
        if (resumeCount > 0) appendSection("Continue Watching", "landscape", "resumeItems")
        if (nextUpCount > 0) appendSection("Next Up", "landscape", "nextUpItems")
        for (let i = 0; i < latestLibraryRowCount; ++i) {
            const row = latestRows[i]
            if (row && row.count > 0)
                appendSection(row.title || ("Recently Added in " + (row.libraryName || "Library")),
                              row.kind || "poster", "latestLibrary", Number(row.rowIndex || i))
        }
    }

    function countFor(source, rowIndex) {
        if (source === "resumeItems") return resumeCount
        if (source === "nextUpItems") return nextUpCount
        if (source === "libraries") return libraryCount
        if (source === "latestItems") return latestCount
        if (source === "latestLibrary") {
            const model = appController ? appController.latestLibraryItems(rowIndex) : null
            return model && model.rowCount ? model.rowCount() : 0
        }
        return 0
    }

    function handlePressedKey(key) {
        if (sections.currentItem && sections.currentItem.handlePressedKey)
            return sections.currentItem.handlePressedKey(key)
        return false
    }

    function handleNavigationKey(key) {
        if (sections.currentItem && sections.currentItem.handleNavigationKey)
            return sections.currentItem.handleNavigationKey(key)
        if (key === Qt.Key_Up) {
            focusSectionOrBar(-1)
            return true
        }
        if (key === Qt.Key_Down) {
            focusSectionOrBar(1)
            return true
        }
        return false
    }

    Component.onCompleted: {
        rebuildSections()
        sections.forceActiveFocus()
    }
    onActiveFocusChanged: if (activeFocus) sections.forceActiveFocus()

    function modelFor(source, rowIndex) {
        if (source === "resumeItems") return resumeModel
        if (source === "nextUpItems") return nextUpModel
        if (source === "libraries") return libraryModel
        if (source === "latestItems") return latestModel
        if (source === "latestLibrary") return appController ? appController.latestLibraryItems(rowIndex) : null
        return null
    }

    function activateAt(source, index, rowIndex) {
        const m = modelFor(source, rowIndex)
        if (!m || index < 0 || index >= m.rowCount())
            return
        if (source === "resumeItems") { appController.playResumeItem(index); return }
        if (source === "nextUpItems") {
            shell.openDetailsAt(root.nextUpModel, index, "nextUp", "home")
            return
        }
        if (source === "libraries") {
            shell.lastLibraryIndex = index
            appController.openLibrary(index)
            // openLibrary will set page=movies → controllerRoute returns libraryGrid.
            return
        }
        if (source === "latestItems") {
            shell.openDetailsAt(root.latestModel, index, "latest", "home")
            return
        }
        if (source === "latestLibrary") {
            shell.openDetailsAt(m, index, "latestLibrary:" + rowIndex, "home")
            return
        }
    }

    function nextVisibleSection(from, dir) {
        const last = sections.count - 1
        let i = from + dir
        while (i >= 0 && i <= last) {
            const it = sections.itemAtIndex(i)
            if (!it || it.sectionVisible) return i
            i += dir
        }
        return from
    }

    function focusSection(index) {
        if (index < 0 || index >= sections.count)
            return
        sections.currentIndex = index
        sections.scrollCurrentSectionIntoView()
    }

    // Move between sections; pressing Up on the topmost section returns to the
    // top navigation bar.
    function focusSectionOrBar(dir) {
        const next = nextVisibleSection(sections.currentIndex, dir)
        if (next === sections.currentIndex) {
            if (dir < 0) shell.focusNavBar()
            return
        }
        focusSection(next)
    }

    ListView {
        id: sections
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin(width)
        focus: true
        clip: true
        keyNavigationEnabled: false
        spacing: 14
        model: sectionModel
        maximumFlickVelocity: 7200
        flickDeceleration: 6200
        onCurrentIndexChanged: scrollCurrentSectionIntoView()

        FastWheelHandler { flickable: sections }

        Behavior on contentY {
            enabled: !Theme.reducedMotion
            NumberAnimation {
                duration: 90
                easing.type: Easing.OutCubic
            }
        }

        function scrollCurrentSectionIntoView() {
            const item = currentItem
            if (!item)
                return

            const scale = Math.max(0.78, Math.min(1.0, root.height / 1440))
            const margin = Math.round(20 * scale)
            const top = Math.max(0, item.y - margin)
            const bottom = item.y + item.height + margin
            const viewportTop = contentY
            const viewportBottom = contentY + height
            const maxY = Math.max(0, contentHeight - height)

            if (top < viewportTop)
                contentY = Math.max(0, top)
            else if (bottom > viewportBottom)
                contentY = Math.min(maxY, bottom - height)
        }

        header: SectionHeader { width: sections.width; height: implicitHeight + 18; title: "My Media" }

        delegate: FocusScope {
            id: section
            required property int index
            required property string title
            required property string kind
            required property string source
            required property int rowIndex
            readonly property string rowSource: source
            readonly property int sectionCount: kind === "spotlight" ? 1 : root.countFor(rowSource, rowIndex)
            readonly property bool sectionVisible: sectionCount > 0
            width: sections.width
            height: !sectionVisible ? 0
                  : kind === "spotlight" ? 238
                  : kind === "library" ? 276
                  : kind === "poster" ? 338
                  : 304
            visible: sectionVisible
            focus: ListView.isCurrentItem && sectionVisible
            onActiveFocusChanged: if (activeFocus && contentLoader.item) contentLoader.item.forceActiveFocus()

            function handleNavigationKey(key) {
                if (contentLoader.item && contentLoader.item.handleNavigationKey)
                    return contentLoader.item.handleNavigationKey(key)
                return false
            }

            function handlePressedKey(key) {
                if (contentLoader.item && contentLoader.item.handlePressedKey)
                    return contentLoader.item.handlePressedKey(key)
                return false
            }

            Loader {
                id: contentLoader
                anchors.fill: parent
                sourceComponent: section.kind === "spotlight" ? spotlightComponent : rowComponent
                onLoaded: if (section.ListView.isCurrentItem) item.forceActiveFocus()
            }

            Component {
                id: spotlightComponent
                Surface {
                    focus: true
                    focused: section.ListView.isCurrentItem
                    function handleNavigationKey(key) {
                        if (key === Qt.Key_Up) {
                            root.focusSectionOrBar(-1)
                            return true
                        }
                        if (key === Qt.Key_Down) {
                            root.focusSectionOrBar(1)
                            return true
                        }
                        if (InputKeys.isAccept(key, false) && root.spotlightIndex >= 0) {
                            shell.openDetailsAt(root.latestModel, root.spotlightIndex, "latest", "home")
                            return true
                        }
                        return false
                    }
                    baseColor: Theme.bgRaised
                    RowLayout { anchors.fill: parent; anchors.margins: 20; spacing: 22
                        ImageCard { Layout.preferredWidth: 300; Layout.fillHeight: true; imageUrl: root.spotlight.posterUrl || ""; aspectRatio: 16/9; focused: section.ListView.isCurrentItem; fallbackText: "Jellyfin"; retainWhileLoading: true }
                        ColumnLayout { Layout.fillWidth: true; spacing: 8
                            AppText { text: root.spotlight.title || "Jellyfin"; font.pixelSize: Math.min(34, Metrics.titlePx(root.width)); font.weight: Font.DemiBold }
                            AppText { text: root.spotlight.year > 0 ? String(root.spotlight.year) : (root.spotlight.itemType || ""); color: Theme.textSecondary; font.pixelSize: Metrics.bodyPx(root.width) }
                            TechMetadataLine { Layout.fillWidth: true; visible: Boolean(root.spotlight.subtitle); metadata: root.spotlight.subtitle || "" }
                            AppText { Layout.fillWidth: true; visible: Boolean(root.spotlight.overview); text: root.spotlight.overview || ""; color: Theme.textSecondary; wrapMode: Text.Wrap; maximumLineCount: 3 }
                            Row { spacing: 10; visible: root.spotlightIndex >= 0
                                ActionButton { id: spotlightPlay; text: "Details"; kind: "primary"; onClicked: if (root.spotlightIndex >= 0) shell.openDetailsAt(root.latestModel, root.spotlightIndex, "latest", "home") }
                                ActionButton { text: "Media info"; onClicked: shell.openMediaInfo(root.spotlight) }
                            }
                        }
                    }
                    Keys.onReleased: (event) => {
                        if (InputKeys.isAccept(event.key) && root.spotlightIndex >= 0) {
                            shell.openDetailsAt(root.latestModel, root.spotlightIndex, "latest", "home")
                            event.accepted = true
                        }
                    }
                }
            }

            Component {
                id: rowComponent
                HomeHorizontalRow {
                    title: section.title
                    rowModel: root.modelFor(section.rowSource, section.rowIndex)
                    shell: root.shell
                    rowKind: section.kind
                    useSeriesPoster: section.rowSource === "latestLibrary"
                    cardWidth: section.kind === "poster"
                               ? Metrics.homePosterWidth(root.width)
                               : Metrics.homeLandscapeWidth(root.width)
                    cardGap: Metrics.gap(root.width)
                    focus: section.ListView.isCurrentItem
                    onMoveVertical: (direction) => root.focusSectionOrBar(direction)
                    onActivated: (index) => root.activateAt(section.rowSource,
                                                            index,
                                                            section.rowIndex)
                    onFavoriteToggled: (index, favorite) => {
                        const model = root.modelFor(section.rowSource,
                                                    section.rowIndex)
                        if (model && index >= 0 && index < model.rowCount()) {
                            const item = model.get(index)
                            appController.setFavorite(item.movieId || "", favorite)
                        }
                    }
                    onPlayedToggled: (index, played) => {
                        const model = root.modelFor(section.rowSource,
                                                    section.rowIndex)
                        if (model && index >= 0 && index < model.rowCount()) {
                            const item = model.get(index)
                            appController.setPlayed(item.movieId || "", played)
                        }
                    }
                    onMediaInfoRequested: (index) => {
                        const model = root.modelFor(section.rowSource,
                                                    section.rowIndex)
                        if (model && index >= 0 && index < model.rowCount())
                            shell.openMediaInfo(model.get(index))
                    }
                }
            }
        }
    }

}
