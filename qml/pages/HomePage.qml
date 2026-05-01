import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    property var shell
    property int resumeCount: appController.resumeItems.rowCount()
    property int nextUpCount: appController.nextUpItems.rowCount()
    property int latestCount: appController.latestItems.rowCount()
    property int libraryCount: appController.libraries.rowCount()
    readonly property int spotlightIndex: latestCount > 0 ? 0 : -1
    property var spotlight: spotlightIndex >= 0 ? appController.latestItems.get(spotlightIndex) : ({})
    focus: true

    Connections {
        target: appController.latestItems
        function onModelReset() {
            root.latestCount = appController.latestItems.rowCount()
            root.rebuildSections()
        }
        function onRowsInserted() {
            root.latestCount = appController.latestItems.rowCount()
            root.rebuildSections()
        }
        function onRowsRemoved() {
            root.latestCount = appController.latestItems.rowCount()
            root.rebuildSections()
        }
    }
    Connections {
        target: appController.libraries
        function onModelReset() { root.libraryCount = appController.libraries.rowCount(); root.rebuildSections() }
        function onRowsInserted() { root.libraryCount = appController.libraries.rowCount(); root.rebuildSections() }
        function onRowsRemoved() { root.libraryCount = appController.libraries.rowCount(); root.rebuildSections() }
    }
    Connections {
        target: appController.resumeItems
        function onModelReset() { root.resumeCount = appController.resumeItems.rowCount(); root.rebuildSections() }
        function onRowsInserted() { root.resumeCount = appController.resumeItems.rowCount(); root.rebuildSections() }
        function onRowsRemoved() { root.resumeCount = appController.resumeItems.rowCount(); root.rebuildSections() }
    }
    Connections {
        target: appController.nextUpItems
        function onModelReset() { root.nextUpCount = appController.nextUpItems.rowCount(); root.rebuildSections() }
        function onRowsInserted() { root.nextUpCount = appController.nextUpItems.rowCount(); root.rebuildSections() }
        function onRowsRemoved() { root.nextUpCount = appController.nextUpItems.rowCount(); root.rebuildSections() }
    }

    ListModel { id: sectionModel }

    function appendSection(title, kind, source) {
        sectionModel.append({ title: title, kind: kind, source: source })
    }

    function rebuildSections() {
        if (!sectionModel)
            return
        sectionModel.clear()
        if (libraryCount > 0) appendSection("Libraries", "library", "libraries")
        if (resumeCount > 0) appendSection("Continue Watching", "landscape", "resumeItems")
        if (nextUpCount > 0) appendSection("Next Up", "landscape", "nextUpItems")
        if (latestCount > 0) appendSection("Recently Added", "poster", "latestItems")
    }

    function countFor(source) {
        if (source === "resumeItems") return resumeCount
        if (source === "nextUpItems") return nextUpCount
        if (source === "libraries") return libraryCount
        if (source === "latestItems") return latestCount
        return 0
    }

    function handleNavigationKey(key) {
        if (sections.currentItem && sections.currentItem.handleNavigationKey)
            return sections.currentItem.handleNavigationKey(key)
        if (key === Qt.Key_Up) {
            sections.currentIndex = nextVisibleSection(sections.currentIndex, -1)
            return true
        }
        if (key === Qt.Key_Down) {
            sections.currentIndex = nextVisibleSection(sections.currentIndex, 1)
            return true
        }
        return false
    }

    Component.onCompleted: {
        rebuildSections()
        sections.forceActiveFocus()
    }
    onActiveFocusChanged: if (activeFocus) sections.forceActiveFocus()

    function modelFor(source) {
        if (source === "resumeItems") return appController.resumeItems
        if (source === "nextUpItems") return appController.nextUpItems
        if (source === "libraries") return appController.libraries
        if (source === "latestItems") return appController.latestItems
        return null
    }

    function activateAt(source, index) {
        const m = modelFor(source)
        if (!m || index < 0 || index >= m.rowCount())
            return
        if (source === "resumeItems") { appController.playResumeItem(index); return }
        if (source === "nextUpItems") { appController.playNextUpItem(index); return }
        if (source === "libraries") {
            shell.lastLibraryIndex = index
            appController.openLibrary(index)
            // openLibrary will set page=movies → controllerRoute returns libraryGrid.
            return
        }
        if (source === "latestItems") {
            // Latest items are playable directly (Movie/Episode) or browsable (Series).
            appController.playLatestItem(index)
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

    ListView {
        id: sections
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin(width)
        focus: true
        clip: true
        keyNavigationEnabled: false
        spacing: 22
        model: sectionModel

        header: SectionHeader { width: sections.width; height: implicitHeight + 18; title: "Home" }

        delegate: FocusScope {
            id: section
            required property int index
            required property string title
            required property string kind
            required property string source
            readonly property string rowSource: source
            readonly property int sectionCount: kind === "spotlight" ? 1 : root.countFor(rowSource)
            readonly property bool sectionVisible: sectionCount > 0
            width: sections.width
            height: !sectionVisible ? 0
                  : kind === "spotlight" ? 238
                  : kind === "poster" ? 360
                  : 292
            visible: sectionVisible
            focus: ListView.isCurrentItem && sectionVisible
            onActiveFocusChanged: if (activeFocus && contentLoader.item) contentLoader.item.forceActiveFocus()

            function handleNavigationKey(key) {
                if (contentLoader.item && contentLoader.item.handleNavigationKey)
                    return contentLoader.item.handleNavigationKey(key)
                return false
            }

            Loader {
                id: contentLoader
                anchors.fill: parent
                sourceComponent: section.kind === "spotlight" ? spotlightComponent
                                : section.kind === "library" ? libraryRowComponent
                                : rowComponent
                onLoaded: if (section.ListView.isCurrentItem) item.forceActiveFocus()
            }

            Component {
                id: spotlightComponent
                Surface {
                    focus: true
                    focused: section.ListView.isCurrentItem
                    function handleNavigationKey(key) {
                        if (key === Qt.Key_Left) {
                            shell.focusRail()
                            return true
                        }
                        if (key === Qt.Key_Up) {
                            sections.currentIndex = root.nextVisibleSection(sections.currentIndex, -1)
                            return true
                        }
                        if (key === Qt.Key_Down) {
                            sections.currentIndex = root.nextVisibleSection(sections.currentIndex, 1)
                            return true
                        }
                        if ((key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select) && root.spotlightIndex >= 0) {
                            appController.playLatestItem(root.spotlightIndex)
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
                                ActionButton { id: spotlightPlay; text: root.spotlight.playActionLabel || "Play"; kind: "primary"; onClicked: if (root.spotlightIndex >= 0) appController.playLatestItem(root.spotlightIndex) }
                                ActionButton { text: "Media info"; onClicked: shell.openMediaInfo(root.spotlight) }
                            }
                        }
                    }
                    Keys.onReleased: (event) => {
                        if (event.key === Qt.Key_Left) {
                            shell.focusRail()
                            event.accepted = true
                        } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Select || event.key === Qt.Key_Space) && root.spotlightIndex >= 0) {
                            appController.playLatestItem(root.spotlightIndex)
                            event.accepted = true
                        }
                    }
                }
            }

            Component {
                id: rowComponent
                FocusScope {
                    id: rowScope
                    property int currentIndex: rowCount > 0 ? 0 : -1
                    readonly property int rowCount: root.countFor(section.rowSource)
                    readonly property int visibleCount: Math.min(rowCount, 24)
                    readonly property int cardWidth: section.kind === "poster" ? Metrics.basePosterWidth(root.width) : 330
                    readonly property int cardGap: Metrics.gap(root.width)
                    focus: section.ListView.isCurrentItem
                    onActiveFocusChanged: if (activeFocus) rowFlick.forceActiveFocus()
                    onRowCountChanged: currentIndex = rowCount > 0 ? Math.max(0, Math.min(currentIndex, visibleCount - 1)) : -1

                    function itemAt(index) {
                        const m = root.modelFor(section.rowSource)
                        return m && index >= 0 && index < m.rowCount() ? m.get(index) : ({})
                    }

                    function ensureVisible() {
                        if (currentIndex < 0)
                            return
                        const targetX = currentIndex * (cardWidth + cardGap)
                        const left = rowFlick.contentX
                        const right = left + rowFlick.width - cardWidth
                        if (targetX < left)
                            rowFlick.contentX = targetX
                        else if (targetX > right)
                            rowFlick.contentX = Math.min(rowFlick.contentWidth - rowFlick.width, targetX - rowFlick.width + cardWidth)
                    }

                    function handleNavigationKey(key) {
                        if (key === Qt.Key_Left) {
                            if (visibleCount <= 0 || currentIndex <= 0) shell.focusRail()
                            else currentIndex = currentIndex - 1
                            ensureVisible()
                            return true
                        }
                        if (key === Qt.Key_Right) {
                            if (visibleCount <= 0)
                                return true
                            currentIndex = Math.min(visibleCount - 1, currentIndex + 1)
                            ensureVisible()
                            return true
                        }
                        if (key === Qt.Key_Up) {
                            sections.currentIndex = root.nextVisibleSection(sections.currentIndex, -1)
                            return true
                        }
                        if (key === Qt.Key_Down) {
                            sections.currentIndex = root.nextVisibleSection(sections.currentIndex, 1)
                            return true
                        }
                        if (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select) {
                            if (currentIndex < 0)
                                return true
                            root.activateAt(section.rowSource, currentIndex)
                            return true
                        }
                        return false
                    }
                    ColumnLayout { anchors.fill: parent; spacing: 10
                        SectionHeader { id: rowHeader; Layout.fillWidth: true; title: section.title }
                        Flickable {
                            id: rowFlick
                            Layout.fillWidth: true
                            Layout.preferredHeight: section.kind === "poster" ? 304 : 228
                            Layout.minimumHeight: Layout.preferredHeight
                            focus: rowScope.focus
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            contentWidth: rowContent.width
                            contentHeight: height
                            Row {
                                id: rowContent
                                height: rowFlick.height
                                spacing: rowScope.cardGap
                                Repeater {
                                    model: rowScope.visibleCount
                                    delegate: Item {
                                id: mediaDelegate
                                required property int index
                                readonly property var itemData: rowScope.itemAt(index)
                                width: rowScope.cardWidth
                                height: rowFlick.height
                                PosterCard {
                                    anchors.fill: parent
                                    visible: section.kind === "poster"
                                    title: mediaDelegate.itemData.title || ""
                                    posterUrl: mediaDelegate.itemData.posterUrl || ""
                                    year: mediaDelegate.itemData.year || 0
                                    metadata: mediaDelegate.itemData.subtitle || ""
                                    focused: mediaDelegate.index === rowScope.currentIndex && section.ListView.isCurrentItem
                                }
                                LandscapeCard {
                                    anchors.fill: parent
                                    visible: section.kind !== "poster"
                                    title: mediaDelegate.itemData.displayTitle || mediaDelegate.itemData.title || ""
                                    subtitle: mediaDelegate.itemData.displaySubtitle || mediaDelegate.itemData.subtitle || ""
                                    imageUrl: mediaDelegate.itemData.posterUrl || ""
                                    progress: mediaDelegate.itemData.progress || 0
                                    focused: mediaDelegate.index === rowScope.currentIndex && section.ListView.isCurrentItem
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        rowScope.currentIndex = mediaDelegate.index
                                        root.activateAt(section.rowSource, mediaDelegate.index)
                                    }
                                }
                                    }
                                }
                            }
                            Keys.onReleased: (event) => {
                                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Select) {
                                    root.activateAt(section.rowSource, rowScope.currentIndex)
                                    event.accepted = true
                                } else if (event.key === Qt.Key_Space) {
                                    const src = section.rowSource
                                    if (src === "resumeItems") appController.playResumeItem(rowScope.currentIndex)
                                    else if (src === "nextUpItems") appController.playNextUpItem(rowScope.currentIndex)
                                    else if (src === "latestItems") appController.playLatestItem(rowScope.currentIndex)
                                    event.accepted = true
                                } else if (event.key === Qt.Key_M) {
                                    const m2 = root.modelFor(section.rowSource)
                                    if (m2) shell.openMediaInfo(m2.get(rowScope.currentIndex))
                                    event.accepted = true
                                }
                            }
                        }
                    }
                }
            }

            Component {
                id: libraryRowComponent
                FocusScope {
                    id: libRowScope
                    property int currentIndex: libraryCount > 0 ? 0 : -1
                    readonly property int visibleCount: Math.min(libraryCount, 24)
                    readonly property int cardWidth: 330
                    readonly property int cardGap: Metrics.gap(root.width)
                    focus: section.ListView.isCurrentItem
                    onActiveFocusChanged: if (activeFocus) libRowFlick.forceActiveFocus()
                    onVisibleCountChanged: currentIndex = visibleCount > 0 ? Math.max(0, Math.min(currentIndex, visibleCount - 1)) : -1

                    function libraryAt(index) {
                        return index >= 0 && index < appController.libraries.rowCount() ? appController.libraries.get(index) : ({})
                    }

                    function ensureVisible() {
                        if (currentIndex < 0)
                            return
                        const targetX = currentIndex * (cardWidth + cardGap)
                        const left = libRowFlick.contentX
                        const right = left + libRowFlick.width - cardWidth
                        if (targetX < left)
                            libRowFlick.contentX = targetX
                        else if (targetX > right)
                            libRowFlick.contentX = Math.min(libRowFlick.contentWidth - libRowFlick.width, targetX - libRowFlick.width + cardWidth)
                    }

                    function handleNavigationKey(key) {
                        if (key === Qt.Key_Left) {
                            if (visibleCount <= 0 || currentIndex <= 0) shell.focusRail()
                            else currentIndex = currentIndex - 1
                            ensureVisible()
                            return true
                        }
                        if (key === Qt.Key_Right) {
                            if (visibleCount <= 0) return true
                            currentIndex = Math.min(visibleCount - 1, currentIndex + 1)
                            ensureVisible()
                            return true
                        }
                        if (key === Qt.Key_Up) { sections.currentIndex = root.nextVisibleSection(sections.currentIndex, -1); return true }
                        if (key === Qt.Key_Down) { sections.currentIndex = root.nextVisibleSection(sections.currentIndex, 1); return true }
                        if (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select) {
                            if (currentIndex >= 0) root.activateAt("libraries", currentIndex)
                            return true
                        }
                        return false
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10
                        SectionHeader { id: libHeader; Layout.fillWidth: true; title: section.title }
                        Flickable {
                            id: libRowFlick
                            Layout.fillWidth: true
                            Layout.preferredHeight: 228
                            Layout.minimumHeight: Layout.preferredHeight
                            focus: libRowScope.focus
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            contentWidth: libRowContent.width
                            contentHeight: height
                            Row {
                                id: libRowContent
                                height: libRowFlick.height
                                spacing: libRowScope.cardGap
                                Repeater {
                                    model: libRowScope.visibleCount
                                    delegate: Item {
                                id: libDelegate
                                required property int index
                                readonly property var itemData: libRowScope.libraryAt(index)
                                width: libRowScope.cardWidth
                                height: libRowFlick.height
                                LandscapeCard {
                                    anchors.fill: parent
                                    title: libDelegate.itemData.name || ""
                                    subtitle: libDelegate.itemData.collectionType || ""
                                    imageUrl: libDelegate.itemData.imageUrl || ""
                                    focused: libDelegate.index === libRowScope.currentIndex && section.ListView.isCurrentItem
                                }
                                MouseArea { anchors.fill: parent; onClicked: { libRowScope.currentIndex = libDelegate.index; root.activateAt("libraries", libDelegate.index) } }
                                    }
                                }
                            }
                            Keys.onReleased: (event) => {
                                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Select) {
                                    if (libRowScope.currentIndex >= 0) root.activateAt("libraries", libRowScope.currentIndex)
                                    event.accepted = true
                                }
                            }
                        }
                    }
                }
            }
        }
    }

}
