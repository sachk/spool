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
    property int resumeCount: resumeModel ? resumeModel.rowCount() : 0
    property int nextUpCount: nextUpModel ? nextUpModel.rowCount() : 0
    property int latestCount: latestModel ? latestModel.rowCount() : 0
    property int latestLibraryRowCount: latestRows ? latestRows.length : 0
    property int libraryCount: libraryModel ? libraryModel.rowCount() : 0
    readonly property int spotlightIndex: latestCount > 0 ? 0 : -1
    property var spotlight: spotlightIndex >= 0 && latestModel ? latestModel.get(spotlightIndex) : ({})
    focus: true

    Connections {
        target: root.latestModel
        function onModelReset() {
            root.latestCount = root.latestModel ? root.latestModel.rowCount() : 0
            root.rebuildSections()
        }
        function onRowsInserted() {
            root.latestCount = root.latestModel ? root.latestModel.rowCount() : 0
            root.rebuildSections()
        }
        function onRowsRemoved() {
            root.latestCount = root.latestModel ? root.latestModel.rowCount() : 0
            root.rebuildSections()
        }
    }
    Connections {
        target: appController
        function onLatestLibraryRowsChanged() {
            root.latestRows = appController ? appController.latestLibraryRows : []
            root.latestLibraryRowCount = root.latestRows ? root.latestRows.length : 0
            root.rebuildSections()
        }
    }
    Connections {
        target: root.libraryModel
        function onModelReset() { root.libraryCount = root.libraryModel ? root.libraryModel.rowCount() : 0; root.rebuildSections() }
        function onRowsInserted() { root.libraryCount = root.libraryModel ? root.libraryModel.rowCount() : 0; root.rebuildSections() }
        function onRowsRemoved() { root.libraryCount = root.libraryModel ? root.libraryModel.rowCount() : 0; root.rebuildSections() }
    }
    Connections {
        target: root.resumeModel
        function onModelReset() { root.resumeCount = root.resumeModel ? root.resumeModel.rowCount() : 0; root.rebuildSections() }
        function onRowsInserted() { root.resumeCount = root.resumeModel ? root.resumeModel.rowCount() : 0; root.rebuildSections() }
        function onRowsRemoved() { root.resumeCount = root.resumeModel ? root.resumeModel.rowCount() : 0; root.rebuildSections() }
    }
    Connections {
        target: root.nextUpModel
        function onModelReset() { root.nextUpCount = root.nextUpModel ? root.nextUpModel.rowCount() : 0; root.rebuildSections() }
        function onRowsInserted() { root.nextUpCount = root.nextUpModel ? root.nextUpModel.rowCount() : 0; root.rebuildSections() }
        function onRowsRemoved() { root.nextUpCount = root.nextUpModel ? root.nextUpModel.rowCount() : 0; root.rebuildSections() }
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
            focusSection(nextVisibleSection(sections.currentIndex, -1))
            return true
        }
        if (key === Qt.Key_Down) {
            focusSection(nextVisibleSection(sections.currentIndex, 1))
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
        if (source === "nextUpItems") { appController.playNextUpItem(index); return }
        if (source === "libraries") {
            shell.lastLibraryIndex = index
            appController.openLibrary(index)
            // openLibrary will set page=movies → controllerRoute returns libraryGrid.
            return
        }
        if (source === "latestItems") {
            shell.openDetails(root.latestModel, index, "latest", "home")
            return
        }
        if (source === "latestLibrary") {
            shell.openDetails(m, index, "latestLibrary:" + rowIndex, "home")
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

    ListView {
        id: sections
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin(width)
        focus: true
        clip: true
        keyNavigationEnabled: false
        spacing: 22
        model: sectionModel
        maximumFlickVelocity: 7200
        flickDeceleration: 6200
        onCurrentIndexChanged: scrollCurrentSectionIntoView()

        Behavior on contentY {
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

        header: SectionHeader { width: sections.width; height: implicitHeight + 18; title: "Home" }

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
                  : kind === "poster" ? 360
                  : 330
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
                            root.focusSection(root.nextVisibleSection(sections.currentIndex, -1))
                            return true
                        }
                        if (key === Qt.Key_Down) {
                            root.focusSection(root.nextVisibleSection(sections.currentIndex, 1))
                            return true
                        }
                        if ((key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select) && root.spotlightIndex >= 0) {
                            shell.openDetails(root.latestModel, root.spotlightIndex, "latest", "home")
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
                                ActionButton { id: spotlightPlay; text: "Details"; kind: "primary"; onClicked: if (root.spotlightIndex >= 0) shell.openDetails(root.latestModel, root.spotlightIndex, "latest", "home") }
                                ActionButton { text: "Media info"; onClicked: shell.openMediaInfo(root.spotlight) }
                            }
                        }
                    }
                    Keys.onReleased: (event) => {
                        if (event.key === Qt.Key_Left) {
                            shell.focusRail()
                            event.accepted = true
                        } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Select || event.key === Qt.Key_Space) && root.spotlightIndex >= 0) {
                            shell.openDetails(root.latestModel, root.spotlightIndex, "latest", "home")
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
                    readonly property int rowCount: root.countFor(section.rowSource, section.rowIndex)
                    readonly property int visibleCount: Math.min(rowCount, 24)
                    readonly property int cardWidth: section.kind === "poster" ? Metrics.homePosterWidth(root.width) : Metrics.homeLandscapeWidth(root.width)
                    readonly property int cardGap: Metrics.gap(root.width)
                    focus: section.ListView.isCurrentItem
                    onActiveFocusChanged: if (activeFocus) rowFlick.forceActiveFocus()
                    onCurrentIndexChanged: ensureVisible()
                    onRowCountChanged: currentIndex = rowCount > 0 ? Math.max(0, Math.min(currentIndex, visibleCount - 1)) : -1

                    function itemAt(index) {
                        const m = root.modelFor(section.rowSource, section.rowIndex)
                        return m && index >= 0 && index < m.rowCount() ? m.get(index) : ({})
                    }

                    function currentCard() {
                        if (currentIndex < 0 || currentIndex >= rowContent.children.length)
                            return null
                        return rowContent.children[currentIndex]
                    }

                    function handlePressedKey(key) {
                        const card = currentCard()
                        return card && card.handleAcceptPressed ? card.handleAcceptPressed(key) : false
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
                        const acceptKey = key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select || key === Qt.Key_Space
                        const card = currentCard()
                        if (!acceptKey && card && card.handleNavigationKey && card.handleNavigationKey(key))
                            return true
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
                            root.focusSection(root.nextVisibleSection(sections.currentIndex, -1))
                            return true
                        }
                        if (key === Qt.Key_Down) {
                            root.focusSection(root.nextVisibleSection(sections.currentIndex, 1))
                            return true
                        }
                        if (acceptKey) {
                            if (currentIndex < 0)
                                return true
                            if (card && card.handleAcceptReleased && card.handleAcceptReleased(key))
                                return true
                            if (card && card.handleNavigationKey && card.handleNavigationKey(key))
                                return true
                            activateCard(currentIndex)
                            return true
                        }
                        return false
                    }

                    function activateCard(index) {
                        if (index < 0 || index >= visibleCount)
                            return
                        currentIndex = index
                        root.activateAt(section.rowSource, index, section.rowIndex)
                    }
                    ColumnLayout { anchors.fill: parent; spacing: 10
                        SectionHeader { id: rowHeader; Layout.fillWidth: true; title: section.title }
                        Flickable {
                            id: rowFlick
                            Layout.fillWidth: true
                            Layout.preferredHeight: section.kind === "poster" ? 304 : 266
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
                                    delegate: MediaItemCard {
                                id: mediaDelegate
                                required property int index
                                readonly property var itemData: rowScope.itemAt(index)
                                item: itemData
                                kind: section.kind === "poster" ? "poster" : "landscape"
                                useSeriesPoster: section.rowSource === "latestLibrary"
                                focused: mediaDelegate.index === rowScope.currentIndex && section.ListView.isCurrentItem
                                width: rowScope.cardWidth
                                height: rowFlick.height
                                onActivated: rowScope.activateCard(mediaDelegate.index)
                                onFavoriteToggled: (favorite) => appController.setFavorite(mediaDelegate.itemData.movieId || "", favorite)
                                onPlayedToggled: (played) => appController.setPlayed(mediaDelegate.itemData.movieId || "", played)
                                onMediaInfoRequested: shell.openMediaInfo(mediaDelegate.itemData)
                                    }
                                }
                            }
                            Keys.onReleased: (event) => {
                                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Select || event.key === Qt.Key_Space) {
                                    const card = rowScope.currentCard()
                                    if (card && card.handleAcceptReleased && card.handleAcceptReleased(event.key)) {
                                        event.accepted = true
                                        return
                                    }
                                    rowScope.activateCard(rowScope.currentIndex)
                                    event.accepted = true
                                } else if (event.key === Qt.Key_M) {
                                    const m2 = root.modelFor(section.rowSource, section.rowIndex)
                                    if (m2 && rowScope.currentIndex >= 0 && rowScope.currentIndex < m2.rowCount())
                                        shell.openMediaInfo(m2.get(rowScope.currentIndex))
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
                    readonly property int cardWidth: Metrics.homeLandscapeWidth(root.width)
                    readonly property int cardGap: Metrics.gap(root.width)
                    focus: section.ListView.isCurrentItem
                    onActiveFocusChanged: if (activeFocus) libRowFlick.forceActiveFocus()
                    onCurrentIndexChanged: ensureVisible()
                    onVisibleCountChanged: currentIndex = visibleCount > 0 ? Math.max(0, Math.min(currentIndex, visibleCount - 1)) : -1

                    function libraryAt(index) {
                        return root.libraryModel && index >= 0 && index < root.libraryModel.rowCount() ? root.libraryModel.get(index) : ({})
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
                        if (key === Qt.Key_Up) { root.focusSection(root.nextVisibleSection(sections.currentIndex, -1)); return true }
                        if (key === Qt.Key_Down) { root.focusSection(root.nextVisibleSection(sections.currentIndex, 1)); return true }
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
