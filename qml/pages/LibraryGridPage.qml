import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    property var shell
    readonly property bool episodeGrid: Browse.viewKind === "episodes"
    property int columns: episodeGrid ? Math.max(2, Math.floor(width / Math.max(260, Metrics.homeLandscapeWidth(width)))) :
                                        Metrics.columns(width)
    property bool sortOpen: false
    property bool filtersOpen: false
    property bool libraryOpen: false
    property int sortIndex: 0
    property int filterIndex: 0
    property int libraryIndex: 0
    property var sortEntries: []
    property var filterEntries: []
    readonly property string collectionType: Browse.libraryCollectionType
    readonly property var libraryQuery: Browse.query
    readonly property var filterOptions: Browse.filterOptions
    readonly property int activeFilterCount: Browse.filterActiveCount
    readonly property bool browseLoading: Browse.loadingMore
    // Fixed descriptor-backed pages reuse this grid but have no library
    // switcher, sort, or filter controls.
    readonly property bool isFixedBrowseView: ["genre", "studio", "playlist", "boxset", "folder"].indexOf(
        Browse.viewKind) >= 0
    focus: true
    Component.onCompleted: InputKeys.focus(grid)
    onActiveFocusChanged: if (activeFocus)
                              InputKeys.focus(grid)

    component ToolbarButton: FocusScope {
        id: buttonRoot
        property string iconName: "filter_list"
        property string label: ""
        property bool checked: false
        property bool badge: false
        signal activated

        width: Math.max(118, labelText.implicitWidth + 58)
        height: 42
        focus: true

        HoverHandler {
            id: hover
        }

        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: buttonRoot.checked ? Theme.accentPanel : (buttonRoot.activeFocus || hover.hovered) ? Theme.bgHover :
                                                                                                        Theme.bgRaised
            border.width: buttonRoot.activeFocus ? 2 : 1
            border.color: buttonRoot.activeFocus ? Theme.textPrimary : buttonRoot.checked ? Theme.accent : Theme.border
            antialiasing: true
        }

        Row {
            anchors.centerIn: parent
            spacing: 8
            MaterialIcon {
                anchors.verticalCenter: parent.verticalCenter
                name: buttonRoot.iconName
                iconSize: 21
                iconColor: buttonRoot.checked ? Theme.accent : Theme.textPrimary
            }
            AppText {
                id: labelText
                anchors.verticalCenter: parent.verticalCenter
                text: buttonRoot.label
                font.pixelSize: Metrics.metaPx(root.width) + 1
                font.weight: Font.Medium
                maximumLineCount: 1
                elide: Text.ElideRight
            }
        }

        Rectangle {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 5
            width: 8
            height: 8
            radius: 4
            color: Theme.accent
            visible: buttonRoot.badge
        }

        MouseArea {
            anchors.fill: parent
            onClicked: buttonRoot.activated()
        }
    }

    function libraryCount() {
        return Libraries.rowCount()
    }

    function currentLibraryModelIndex() {
        const currentId = String(Browse.libraryId || "")
        const count = libraryCount()
        for (let i = 0; i < count; ++i) {
            const library = Libraries.get(i)
            if (library && String(library.libraryId || "") === currentId)
                return i
        }
        return count > 0 ? 0 : -1
    }

    function headerDetail() {
        const total = Browse.totalCount
        const count = Browse.items ? Browse.items.count : 0
        const parts = []
        if (count > 0)
            parts.push(count + (total > count ? " of " + total : "") + " items")
        if (activeFilterCount > 0)
            parts.push(activeFilterCount + " filter" + (activeFilterCount === 1 ? "" : "s"))
        return parts.join(" · ")
    }

    function listForKey(key) {
        const value = libraryQuery ? libraryQuery[key] : undefined
        if (value === undefined || value === null)
            return []
        if (Array.isArray(value))
            return value
        if (typeof value !== "string" && value.length !== undefined) {
            const result = []
            for (let i = 0; i < value.length; ++i)
                result.push(String(value[i]))
            return result
        }
        return String(value).split(",").filter(function (v) {
            return v.length > 0
        })
    }

    function listHas(key, value) {
        return listForKey(key).indexOf(String(value)) >= 0
    }

    function boolHas(key, value) {
        const current = libraryQuery ? libraryQuery[key] : undefined
        if (current === undefined || current === null)
            return false
        return Boolean(current) === Boolean(value)
    }

    function dynamicList(key) {
        const value = filterOptions ? filterOptions[key] : undefined
        if (!value)
            return []
        if (Array.isArray(value))
            return value
        if (value.length !== undefined) {
            const result = []
            for (let i = 0; i < value.length; ++i)
                result.push(value[i])
            return result
        }
        return []
    }

    function isSeriesLibrary() {
        return collectionType === "tvshows"
    }

    function isMovieLikeLibrary() {
        return collectionType === "movies" || collectionType === "homevideos" || collectionType === "musicvideos"
                || collectionType.length === 0
    }

    function currentSortBy() {
        return libraryQuery && libraryQuery.sortBy ? String(libraryQuery.sortBy) : "SortName"
    }

    function currentSortOrder() {
        return libraryQuery && libraryQuery.sortOrder ? String(libraryQuery.sortOrder) : "Ascending"
    }

    function currentSortLabel() {
        const by = currentSortBy()
        for (let i = 0; i < sortEntries.length; ++i)
            if (sortEntries[i].value === by)
                return sortEntries[i].label
        return "Sort"
    }

    function buildSortEntries() {
        const common = [
                  {
                      label: "Name",
                      value: "SortName"
                  },
                  {
                      label: "Random",
                      value: "Random"
                  },
                  {
                      label: "Community rating",
                      value: "CommunityRating"
                  },
                  {
                      label: "Date added",
                      value: "DateCreated"
                  },
                  {
                      label: "Date played",
                      value: isSeriesLibrary() ? "SeriesDatePlayed" : "DatePlayed"
                  },
                  {
                      label: "Parental rating",
                      value: "OfficialRating"
                  },
                  {
                      label: "Release date",
                      value: "PremiereDate"
                  }
              ]
        if (isSeriesLibrary())
            common.splice(4, 0, {
                              label: "Date episode added",
                              value: "DateLastContentAdded"
                          })
        else
            common.splice(3, 0, {
                              label: "Critic rating",
                              value: "CriticRating"
                          })
        common.push({
                        label: "Play count",
                        value: "PlayCount"
                    })
        common.push({
                        label: "Runtime",
                        value: "Runtime"
                    })
        common.push({
                        label: "Ascending",
                        value: "order:Ascending"
                    })
        common.push({
                        label: "Descending",
                        value: "order:Descending"
                    })
        return common
    }

    function addSection(entries, title) {
        entries.push({
                         section: true,
                         label: title
                     })
    }

    function addListFilter(entries, section, label, key, value) {
        entries.push({
                         section: false,
                         sectionName: section,
                         label: label,
                         key: key,
                         value: String(value),
                         kind: "list",
                         checked: listHas(key, value)
                     })
    }

    function addBoolFilter(entries, section, label, key) {
        entries.push({
                         section: false,
                         sectionName: section,
                         label: label,
                         key: key,
                         kind: "bool",
                         checked: boolHas(key, true)
                     })
    }

    function addNullableBoolFilter(entries, section, label, key, value) {
        entries.push({
                         section: false,
                         sectionName: section,
                         label: label,
                         key: key,
                         value: value,
                         kind: "nullableBool",
                         checked: boolHas(key, value)
                     })
    }

    function buildFilterEntries() {
        const entries = []
        addSection(entries, "Status")
        addListFilter(entries, "Status", "Played", "filters", "IsPlayed")
        addListFilter(entries, "Status", "Unplayed", "filters", "IsUnplayed")
        addListFilter(entries, "Status", "Favorite", "filters", "IsFavorite")
        addListFilter(entries, "Status", "Continue watching", "filters", "IsResumable")

        if (collectionType === "movies")
            addListFilter(entries, "Status", "Collections", "includeItemTypes", "BoxSet")
        if (isMovieLikeLibrary()) {
            addSection(entries, "Video")
            addNullableBoolFilter(entries, "Video", "HD", "isHd", true)
            addNullableBoolFilter(entries, "Video", "SD", "isHd", false)
            addBoolFilter(entries, "Video", "4K", "is4K")
            addBoolFilter(entries, "Video", "3D", "is3D")
            addListFilter(entries, "Video", "DVD", "videoTypes", "Dvd")
            addListFilter(entries, "Video", "Blu-ray", "videoTypes", "BluRay")
            addListFilter(entries, "Video", "ISO", "videoTypes", "Iso")
        }

        addSection(entries, "Features")
        addBoolFilter(entries, "Features", "Subtitles", "hasSubtitles")
        addBoolFilter(entries, "Features", "Trailers", "hasTrailer")
        addBoolFilter(entries, "Features", "Extras", "hasSpecialFeature")
        addBoolFilter(entries, "Features", "Theme songs", "hasThemeSong")
        addBoolFilter(entries, "Features", "Theme videos", "hasThemeVideo")

        if (isSeriesLibrary()) {
            addSection(entries, "Series status")
            addListFilter(entries, "Series status", "Continuing", "seriesStatus", "Continuing")
            addListFilter(entries, "Series status", "Ended", "seriesStatus", "Ended")
            addListFilter(entries, "Series status", "Unreleased", "seriesStatus", "Unreleased")
        }

        const genres = dynamicList("genres")
        if (genres.length > 0) {
            addSection(entries, "Genres")
            for (let i = 0; i < genres.length; ++i)
                addListFilter(entries, "Genres", String(genres[i]), "genres", String(genres[i]))
        }

        const ratings = dynamicList("officialRatings")
        if (ratings.length > 0) {
            addSection(entries, "Parental ratings")
            for (let i = 0; i < ratings.length; ++i)
                addListFilter(entries, "Parental ratings", String(ratings[i]), "officialRatings", String(ratings[i]))
        }

        const tags = dynamicList("tags")
        if (tags.length > 0) {
            addSection(entries, "Tags")
            for (let i = 0; i < tags.length; ++i)
                addListFilter(entries, "Tags", String(tags[i]), "tags", String(tags[i]))
        }

        const years = dynamicList("years")
        if (years.length > 0) {
            addSection(entries, "Years")
            for (let i = 0; i < years.length; ++i)
                addListFilter(entries, "Years", String(years[i]), "years", String(years[i]))
        }

        const studios = dynamicList("studios")
        if (studios.length > 0) {
            addSection(entries, "Studios")
            for (let i = 0; i < studios.length; ++i)
                addListFilter(entries, "Studios", studios[i].name || "", "studioIds", studios[i].id || "")
        }
        return entries
    }

    function firstActionableFilterIndex(entries) {
        for (let i = 0; i < entries.length; ++i) {
            if (!entries[i].section)
                return i
        }
        return 0
    }

    function openSortMenu() {
        libraryOpen = false
        filtersOpen = false
        sortEntries = buildSortEntries()
        sortIndex = Math.max(0, sortEntries.findIndex(function (entry) {
            return entry.value === currentSortBy()
        }))
        sortOpen = true
        Qt.callLater(function () {
            InputKeys.focus(sortList)
        })
    }

    function openFilterMenu() {
        libraryOpen = false
        sortOpen = false
        filterEntries = buildFilterEntries()
        filterIndex = firstActionableFilterIndex(filterEntries)
        filtersOpen = true
        Qt.callLater(function () {
            InputKeys.focus(filterList)
        })
    }

    function openLibraryMenu() {
        if (root.isFixedBrowseView || libraryCount() <= 0)
            return
        sortOpen = false
        filtersOpen = false
        libraryIndex = Math.max(0, currentLibraryModelIndex())
        libraryOpen = true
        Qt.callLater(function () {
            InputKeys.focus(libraryList)
        })
    }

    function closeMenus() {
        libraryOpen = false
        sortOpen = false
        filtersOpen = false
        InputKeys.focus(grid)
    }

    function hasShell() {
        return shell !== null && shell !== undefined
    }

    function savedGridIndex() {
        return hasShell() ? shell.lastGridIndex : 0
    }

    function setSavedGridIndex(index) {
        if (hasShell())
            shell.lastGridIndex = index
    }

    function setSavedLibraryIndex(index) {
        if (hasShell())
            shell.lastLibraryIndex = index
    }

    function activateLibraryIndex(index) {
        if (index < 0 || index >= libraryCount())
            return
        libraryOpen = false
        sortOpen = false
        filtersOpen = false
        setSavedLibraryIndex(index)
        setSavedGridIndex(0)
        App.openLibrary(index)
        if (hasShell())
            shell.replaceRoute("libraryGrid")
        InputKeys.focus(grid)
    }

    function activateSortEntry(entry) {
        if (!entry)
            return
        setSavedGridIndex(0)
        if (String(entry.value).indexOf("order:") === 0)
            Browse.setSort(currentSortBy(), String(entry.value).split(":")[1])
        else
            Browse.setSort(entry.value, currentSortOrder())
        sortEntries = buildSortEntries()
    }

    function activateFilterEntry(entry) {
        if (!entry || entry.section)
            return
        setSavedGridIndex(0)
        if (entry.kind === "list")
            Browse.setQueryListValue(entry.key, entry.value, !entry.checked)
        else if (entry.kind === "bool")
            Browse.setQueryValue(entry.key, entry.checked ? null : true)
        else if (entry.kind === "nullableBool")
            Browse.setQueryValue(entry.key, entry.checked ? null : entry.value)
        filterEntries = buildFilterEntries()
    }

    function focusToolbar() {
        InputKeys.focus(libraryButton)
    }

    Connections {
        target: Browse
        function onChanged() {
            if (root.filtersOpen)
                root.filterEntries = root.buildFilterEntries()
            if (root.sortOpen)
                root.sortEntries = root.buildSortEntries()
        }
    }

    function activateCurrent() {
        if (grid.currentIndex < 0)
            return
        setSavedGridIndex(grid.currentIndex)
        openCurrentDetails()
    }

    function openCurrentDetails() {
        if (grid.currentIndex < 0)
            return
        setSavedGridIndex(grid.currentIndex)
        const item = Browse.items ? (Browse.items.get(grid.currentIndex) || ({})) : ({})
        const type = String(item.itemType || "")
        if (type === "Playlist" || type === "Folder") {
            App.playFromModel(Browse.items, grid.currentIndex)
            return
        }
        if (hasShell())
            shell.openDetailsAt(Browse.items, grid.currentIndex, "movies", "libraryGrid")
    }

    function currentCard() {
        return grid.currentItem
    }

    function routeKey(key, phase, repeat) {
        if (libraryList.activeFocus)
            return libraryList.routeKey(key, phase, repeat)
        if (sortList.activeFocus)
            return sortList.routeKey(key, phase, repeat)
        if (filterList.activeFocus)
            return filterList.routeKey(key, phase, repeat)

        if (libraryButton.activeFocus || sortButton.activeFocus || filterButton.activeFocus
                || clearFiltersButton.activeFocus) {
            if (key === Qt.Key_Up) {
                if (hasShell())
                    shell.focusNavBar()
                return true
            }
            if (key === Qt.Key_Left) {
                if (sortButton.activeFocus)
                    InputKeys.focus(libraryButton)
                else if (filterButton.activeFocus)
                    InputKeys.focus(sortButton)
                else if (clearFiltersButton.activeFocus)
                    InputKeys.focus(filterButton)
                return true
            }
            if (key === Qt.Key_Right) {
                if (libraryButton.activeFocus && sortButton.visible)
                    InputKeys.focus(sortButton)
                else if (sortButton.activeFocus)
                    InputKeys.focus(filterButton)
                else if (filterButton.activeFocus && clearFiltersButton.visible)
                    InputKeys.focus(clearFiltersButton)
                return true
            }
            if (key === Qt.Key_Down) {
                InputKeys.focus(grid)
                return true
            }
            return false
        }
        return grid.count > 0 && grid.routeKey(key, phase, repeat)
    }

    function activate() {
        if (libraryList.activeFocus)
            libraryList.activate()
        else if (sortList.activeFocus)
            sortList.activate()
        else if (filterList.activeFocus)
            filterList.activate()
        else if (libraryButton.activeFocus)
            openLibraryMenu()
        else if (sortButton.activeFocus)
            openSortMenu()
        else if (filterButton.activeFocus)
            openFilterMenu()
        else if (clearFiltersButton.activeFocus)
            Browse.clearFilters()
        else
            grid.activate()
    }

    function longPress() {
        if (!grid.activeFocus || grid.currentIndex < 0 || !hasShell())
            return false
        return shell.openItemMenu(Browse.items.get(grid.currentIndex) || ({}), currentCard())
    }

    function back() {
        if (libraryOpen || sortOpen || filtersOpen) {
            closeMenus()
            return true
        }
        return false
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin(width)
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            FocusScope {
                id: libraryButton
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                focus: true

                Rectangle {
                    anchors.left: titleRow.left
                    anchors.right: titleRow.right
                    anchors.verticalCenter: titleRow.verticalCenter
                    height: 36
                    radius: Theme.radiusSmall
                    color: libraryButton.activeFocus || root.libraryOpen ? Theme.accentPanel : "transparent"
                    border.width: libraryButton.activeFocus ? 2 : 0
                    border.color: Theme.textPrimary
                }

                Row {
                    id: titleRow
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 4
                    anchors.rightMargin: 8
                    spacing: 6
                    width: Math.min(titleText.implicitWidth + 30, Math.max(160, parent.width - headerDetailText.width
                                                                           - 32))

                    AppText {
                        id: titleText
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.max(0, titleRow.width - 30)
                        text: Browse.title.length > 0 ? Browse.title : "Library"
                        font.pixelSize: Metrics.bodyPx(root.width) + 4
                        font.weight: Font.DemiBold
                        maximumLineCount: 1
                        elide: Text.ElideRight
                    }

                    MaterialIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !root.isFixedBrowseView
                        name: root.libraryOpen ? "expand_less" : "expand_more"
                        iconSize: 24
                        iconColor: root.libraryOpen || libraryButton.activeFocus ? Theme.textPrimary :
                                                                                   Theme.textSecondary
                    }
                }

                MonoText {
                    id: headerDetailText
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.headerDetail()
                    color: Theme.textMuted
                    font.pixelSize: Metrics.metaPx(root.width)
                    maximumLineCount: 1
                    elide: Text.ElideRight
                    width: Math.min(520, Math.max(0, parent.width * 0.52))
                }

                BusyIndicator {
                    anchors.right: headerDetailText.left
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    width: 28
                    height: 28
                    running: root.browseLoading
                    visible: running
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: root.openLibraryMenu()
                }
            }

            ToolbarButton {
                id: sortButton
                visible: !root.isFixedBrowseView
                iconName: root.currentSortOrder() === "Descending" ? "south" : "north"
                label: root.currentSortLabel()
                checked: root.sortOpen
                onActivated: root.openSortMenu()
            }

            ToolbarButton {
                id: filterButton
                visible: !root.isFixedBrowseView
                iconName: "filter_list"
                label: "Filter"
                checked: root.filtersOpen
                badge: root.activeFilterCount > 0
                onActivated: root.openFilterMenu()
            }

            ToolbarButton {
                id: clearFiltersButton
                iconName: "filter_list_off"
                label: "Clear"
                visible: !root.isFixedBrowseView && root.activeFilterCount > 0
                onActivated: {
                    root.setSavedGridIndex(0)
                    Browse.clearFilters()
                }
            }
        }

        NavGrid {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: true
            clip: true
            keyNavigationEnabled: false
            reuseItems: true
            opacity: root.browseLoading && count > 0 ? 0.62 : 1
            boundsBehavior: Flickable.StopAtBounds
            model: Browse.items
            cellWidth: Math.floor((width - Metrics.gap(root.width) * (columns - 1)) / columns)
            cellHeight: root.episodeGrid ? Math.round(cellWidth * 9 / 16 + 62) : cellWidth * 1.5 + 64
            cacheBuffer: 2 * cellHeight
            Component.onCompleted: {
                restoreIndex()
                requestMoreIfNeeded()
            }
            onCountChanged: {
                if (count <= 0)
                    currentIndex = -1
                else if (currentIndex < 0 || currentIndex >= count)
                    restoreIndex()
                requestMoreIfNeeded()
            }
            onContentYChanged: loadMoreDebounce.restart()
            onCurrentIndexChanged: {
                root.setSavedGridIndex(currentIndex)
                ensureCurrentVisible()
                loadMoreDebounce.restart()
            }

            FastWheelHandler {
                flickable: grid
            }
            onEdgeUp: root.focusToolbar()
            onAccepted: root.activateCurrent()

            function restoreIndex() {
                currentIndex = count > 0 ? Math.max(0, Math.min(root.savedGridIndex(), count - 1)) : -1
                ensureCurrentVisible()
            }

            function ensureCurrentVisible() {
                if (currentIndex >= 0)
                    positionViewAtIndex(currentIndex, GridView.Contain)
            }

            function lastLikelyVisibleIndex() {
                if (count <= 0 || cellHeight <= 0 || columns <= 0)
                    return -1
                const firstRow = Math.max(0, Math.floor(contentY / cellHeight))
                const visibleRows = Math.ceil(height / cellHeight) + 3
                return Math.min(count - 1, (firstRow + visibleRows) * columns - 1)
            }

            function firstLikelyVisibleIndex() {
                if (count <= 0 || cellHeight <= 0 || columns <= 0)
                    return -1
                return Math.min(count - 1, Math.max(0, Math.floor(contentY / cellHeight) * columns))
            }

            function requestMoreIfNeeded() {
                if (count <= 0)
                    return
                const visibleHead = firstLikelyVisibleIndex()
                const visibleTail = Math.max(currentIndex, lastLikelyVisibleIndex())
                Browse.prefetchVisibleRange(visibleHead, visibleTail)
            }

            Timer {
                id: loadMoreDebounce
                interval: 80
                repeat: false
                onTriggered: grid.requestMoreIfNeeded()
            }

            delegate: Item {
                id: gridDelegate
                required property int index
                required property var item
                readonly property var movie: item || ({})
                width: grid.cellWidth
                height: grid.cellHeight

                MediaItemCard {
                    id: card
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.rightMargin: Metrics.gap(root.width)
                    height: parent.height
                    shell: root.shell
                    kind: root.episodeGrid ? "landscape" : "poster"
                    preferEpisodeTitle: root.episodeGrid
                    useSeriesPoster: !root.episodeGrid
                    focused: gridDelegate.GridView.isCurrentItem
                    item: gridDelegate.movie
                    onActivated: {
                        grid.currentIndex = index
                        root.setSavedGridIndex(index)
                        root.activateCurrent()
                    }
                }
            }
        }
    }

    PopupMenuPanel {
        id: libraryPanel
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: Metrics.pageMargin(root.width) + 52
        anchors.leftMargin: Metrics.pageMargin(root.width)
        width: 360
        open: root.libraryOpen
        openHeight: Math.min(420, libraryList.contentHeight + 20)
        z: 22

        MenuListView {
            id: libraryList
            anchors.fill: parent
            anchors.margins: 10
            visible: libraryPanel.visible
            model: Libraries
            currentIndex: root.libraryIndex
            edgeEscapeItem: libraryButton
            onCurrentIndexChanged: root.libraryIndex = currentIndex
            onDismissed: root.closeMenus()
            onAccepted: index => root.activateLibraryIndex(index)

            delegate: MenuRow {
                required property int index
                required property string name
                required property string collectionType
                required property string libraryId
                width: libraryList.width
                label: name
                checked: String(libraryId || "") === String(Browse.libraryId || "")
                iconName: checked ? "radio_button_checked" : "radio_button_unchecked"
                highlighted: ListView.isCurrentItem && libraryList.activeFocus
                metricsWidth: root.width
                checkIconName: ""
                onHovered: libraryList.currentIndex = index
                onActivated: root.activateLibraryIndex(index)
            }
        }
    }

    PopupMenuPanel {
        id: sortPanel
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: Metrics.pageMargin(root.width) + 52
        anchors.rightMargin: Metrics.pageMargin(root.width)
        width: 320
        open: root.sortOpen
        openHeight: Math.min(430, sortList.contentHeight + 20)
        z: 20

        MenuListView {
            id: sortList
            anchors.fill: parent
            anchors.margins: 10
            visible: sortPanel.visible
            model: root.sortEntries
            currentIndex: root.sortIndex
            edgeEscapeItem: sortButton
            onCurrentIndexChanged: root.sortIndex = currentIndex
            onDismissed: root.closeMenus()
            onAccepted: index => root.activateSortEntry(root.sortEntries[index])

            delegate: MenuRow {
                required property int index
                required property var modelData
                width: sortList.width
                label: modelData.label || ""
                checked: String(modelData.value || "").indexOf("order:") === 0 ? String(modelData.value).split(":")[1]
                                                                                 === root.currentSortOrder() :
                                                                                 modelData.value === root.currentSortBy(
                                                                                     )
                iconName: checked ? "radio_button_checked" : "radio_button_unchecked"
                highlighted: ListView.isCurrentItem && sortList.activeFocus
                metricsWidth: root.width
                checkIconName: ""
                onHovered: sortList.currentIndex = index
                onActivated: root.activateSortEntry(modelData)
            }
        }
    }

    PopupMenuPanel {
        id: filterPanel
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: Metrics.pageMargin(root.width) + 52
        anchors.rightMargin: Metrics.pageMargin(root.width)
        width: 380
        open: root.filtersOpen
        openHeight: Math.min(root.height - Metrics.pageMargin(root.width) * 2 - 70, 620)
        z: 21

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            visible: filterPanel.visible
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                AppText {
                    Layout.fillWidth: true
                    text: "Filters"
                    font.pixelSize: Metrics.bodyPx(root.width)
                    font.weight: Font.DemiBold
                }
                ActionButton {
                    text: "Reset"
                    enabled: root.activeFilterCount > 0
                    onClicked: {
                        root.setSavedGridIndex(0)
                        Browse.clearFilters()
                    }
                }
            }

            MenuListView {
                id: filterList
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: root.filterEntries
                currentIndex: root.filterIndex
                edgeEscapeItem: filterButton
                rowEnabled: function (entry, index) {
                    const row = root.filterEntries[index] || ({})
                    return row.section !== true
                }
                onCurrentIndexChanged: root.filterIndex = currentIndex
                onDismissed: root.closeMenus()
                onAccepted: index => root.activateFilterEntry(root.filterEntries[index])

                delegate: MenuRow {
                    required property int index
                    required property var modelData
                    width: filterList.width
                    section: modelData.section === true
                    label: modelData.label || ""
                    checked: modelData.checked === true
                    iconName: checked ? "check_box" : "check_box_outline_blank"
                    highlighted: ListView.isCurrentItem && filterList.activeFocus
                    metricsWidth: root.width
                    rowHeight: 48
                    checkIconName: ""
                    onHovered: filterList.currentIndex = index
                    onActivated: root.activateFilterEntry(modelData)
                }
            }
        }
    }
}
