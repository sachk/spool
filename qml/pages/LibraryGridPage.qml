pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    property var shell
    property var uiTransitionToken: 0
    property int columns: Metrics.columns(width)
    property bool sortOpen: false
    property bool filtersOpen: false
    property bool libraryOpen: false
    property int sortIndex: 0
    property int filterIndex: 0
    property int libraryIndex: 0
    property var sortEntries: []
    property var filterEntries: []
    property var libraryEntries: []
    property string typeAheadBuffer: ""
    readonly property var libraryList: libraryPanel.menuList
    readonly property var sortList: sortPanel.menuList
    readonly property var filterList: filterPanel.menuList
    readonly property string collectionType: Browse.libraryCollectionType
    readonly property var libraryQuery: Browse.query
    readonly property var filterOptions: Browse.filterOptions
    readonly property int activeFilterCount: Browse.filterActiveCount
    readonly property bool browseLoading: Browse.loadingMore
    // Fixed descriptor-backed pages reuse this grid but have no library
    // switcher, sort, or filter controls.
    readonly property bool isFixedBrowseView: ["genre", "studio", "playlist", "boxset", "folder", "artist"].indexOf(
        Browse.viewKind) >= 0
    focus: true
    readonly property bool contentReady: grid.count > 0 && gridReveal.delegatesReady
    // Grid position to restore across model resets (sort/filter/library
    // changes); the page itself is resident, so this survives navigation.
    property int savedIndex: 0
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
                font.pixelSize: Metrics.metaSizePx + 1
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

    function buildLibraryEntries() {
        const entries = []
        for (let index = 0; index < libraryCount(); ++index) {
            const library = Libraries.get(index)
            entries.push({
                             "label": String(library && library.name || ""),
                             "libraryId": String(library && library.libraryId || ""),
                             "index": index
                         })
        }
        return entries
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
        libraryEntries = buildLibraryEntries()
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

    function currentMediaItem() {
        if (grid.currentIndex < 0 || !Browse.items)
            return ({})
        return Browse.items.get(grid.currentIndex) || ({})
    }

    function typeAheadTitle(index) {
        if (!Browse.items || index < 0 || index >= Browse.items.count)
            return ""
        const item = Browse.items.get(index) || ({})
        return String(item.title || item.displayTitle || item.seriesName || "").toLocaleLowerCase()
    }

    function selectTypeAheadMatch(query, includeCurrent) {
        const count = Browse.items ? Browse.items.count : 0
        if (count <= 0 || query.length <= 0)
            return false
        const start = Math.max(0, grid.currentIndex)
        const firstOffset = includeCurrent ? 0 : 1
        for (let offset = firstOffset; offset < firstOffset + count; ++offset) {
            const index = (start + offset) % count
            if (typeAheadTitle(index).indexOf(query) === 0) {
                grid.currentIndex = index
                grid.ensureCurrentVisible()
                grid.requestMoreIfNeeded()
                return true
            }
        }
        for (let offset = firstOffset; offset < firstOffset + count; ++offset) {
            const index = (start + offset) % count
            if (typeAheadTitle(index).indexOf(query) >= 0) {
                grid.currentIndex = index
                grid.ensureCurrentVisible()
                grid.requestMoreIfNeeded()
                return true
            }
        }
        return false
    }

    function typeAhead(text) {
        if (libraryOpen || sortOpen || filtersOpen || !grid.activeFocus || !text || text.length <= 0)
            return false
        const character = String(text).toLocaleLowerCase()
        const code = character.charCodeAt(0)
        if (code < 32 || code === 127 || character === "/" || (character === " " && typeAheadBuffer.length <= 0))
            return false
        const previous = typeAheadBuffer
        const repeatedSingle = previous.length === 1 && previous === character
        const candidate = repeatedSingle ? character : previous + character
        typeAheadBuffer = candidate
        if (!selectTypeAheadMatch(candidate, previous.length > 0 && !repeatedSingle) && candidate.length > 1) {
            typeAheadBuffer = character
            selectTypeAheadMatch(character, false)
        }
        typeAheadReset.restart()
        return true
    }

    function activateLibraryIndex(index) {
        if (index < 0 || index >= libraryCount())
            return
        libraryOpen = false
        sortOpen = false
        filtersOpen = false
        savedIndex = 0
        gridReveal.reset()
        App.openLibrary(index)
        if (hasShell())
            shell.replaceRoute("libraryGrid")
        InputKeys.focus(grid)
    }

    function activateSortEntry(entry) {
        if (!entry)
            return
        savedIndex = 0
        gridReveal.reset()
        if (String(entry.value).indexOf("order:") === 0)
            Browse.setSort(currentSortBy(), String(entry.value).split(":")[1])
        else
            Browse.setSort(entry.value, currentSortOrder())
        sortEntries = buildSortEntries()
    }

    function activateFilterEntry(entry) {
        if (!entry || entry.section)
            return
        savedIndex = 0
        gridReveal.reset()
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

    AtomicViewReveal {
        id: gridReveal

        view: grid
        latencyMonitor: InputLatency
        transitionToken: root.uiTransitionToken
        enabled: Browse.items.count > 0
        firstIndex: Math.min(grid.count - 1, Math.max(0, Math.floor(grid.contentY / grid.cellHeight) * root.columns))
        lastIndex: Math.min(grid.count - 1, Math.max(firstIndex, Math.ceil((grid.contentY + grid.height)
                                                                           / grid.cellHeight) * root.columns - 1))
        onDelegatesReadyChanged: if (delegatesReady)
        grid.requestMoreIfNeeded()
    }

    function activateCurrent() {
        if (grid.currentIndex < 0)
            return
        savedIndex = grid.currentIndex
        openCurrentDetails()
    }

    function openCurrentDetails() {
        if (grid.currentIndex < 0)
            return
        savedIndex = grid.currentIndex
        const item = Browse.items ? (Browse.items.get(grid.currentIndex) || ({})) : ({})
        const type = String(item.itemType || "")
        if (["Playlist", "Folder", "PhotoAlbum", "MusicAlbum", "MusicArtist"].indexOf(type) >= 0) {
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
        if (libraryList && libraryList.activeFocus)
            return libraryList.routeKey(key, phase, repeat)
        if (sortList && sortList.activeFocus)
            return sortList.routeKey(key, phase, repeat)
        if (filterList && filterList.activeFocus)
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
        if (libraryList && libraryList.activeFocus)
            libraryList.activate()
        else if (sortList && sortList.activeFocus)
            sortList.activate()
        else if (filterList && filterList.activeFocus)
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
        return shell.openItemMenu(Browse.items.get(grid.currentIndex) || ({}), currentCard(), {
                                      "deferBackdropDismissal": true
                                  })
    }

    function back() {
        if (typeAheadBuffer.length > 0) {
            typeAheadBuffer = ""
            typeAheadReset.stop()
            return true
        }
        if (libraryOpen || sortOpen || filtersOpen) {
            closeMenus()
            return true
        }
        return false
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMarginPx
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
                        font.pixelSize: Metrics.bodySizePx + 4
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
                    font.pixelSize: Metrics.metaSizePx
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
                    root.savedIndex = 0
                    gridReveal.reset()
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
            highlightMoveDuration: 16
            opacity: gridReveal.delegatesReady ? 1 : 0
            boundsBehavior: Flickable.StopAtBounds
            model: Browse.items
            cellWidth: Math.floor((width - Metrics.gapPx * (columns - 1)) / columns)
            cellHeight: cellWidth * 1.5 + Metrics.scaled(64)
            cacheBuffer: gridReveal.delegatesReady ? cellHeight : 0
            Component.onCompleted: {
                restoreIndex()
                requestMoreIfNeeded()
                gridReveal.reset()
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
                if (currentIndex >= 0)
                root.savedIndex = currentIndex
                loadMoreDebounce.restart()
            }

            FastWheelHandler {
                flickable: grid
                animationDuration: 16
            }
            onEdgeUp: root.focusToolbar()
            onAccepted: root.activateCurrent()

            function restoreIndex() {
                currentIndex = count > 0 ? Math.max(0, Math.min(root.savedIndex, count - 1)) : -1
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
                if (count <= 0 || !gridReveal.delegatesReady)
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

            delegate: MediaItemCard {
                id: gridDelegate

                required property int index

                width: grid.cellWidth - Metrics.gapPx
                height: grid.cellHeight
                shell: root.shell
                kind: "poster"
                preferEpisodeTitle: false
                useSeriesPoster: true
                focused: gridDelegate.GridView.isCurrentItem
                artworkVisible: true

                Component.onCompleted: gridReveal.schedule()
                onArtworkReadyChanged: gridReveal.schedule()
            }
            highlightFollowsCurrentItem: true
            highlight: Rectangle {
                width: grid.currentItem ? grid.currentItem.width : grid.cellWidth - Metrics.gapPx
                height: grid.currentItem ? grid.currentItem.focusOutlineHeight : grid.cellHeight - Metrics.scaled(6)
                color: "transparent"
                radius: Theme.radiusMedium
                border.width: Theme.focusBorderWidth
                border.color: grid.activeFocus ? Theme.accent : "transparent"
                z: 2
            }

            MouseArea {
                property int pressedIndex: -1
                property bool longPressed: false
                anchors.fill: parent
                z: 3
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                pressAndHoldInterval: 520
                onPressed: mouse => {
                    longPressed = false
                    pressedIndex = grid.indexAt(mouse.x + grid.contentX, mouse.y + grid.contentY)
                    if (pressedIndex >= 0)
                    grid.currentIndex = pressedIndex
                }
                onReleased: if (longPressed && root.shell)
                root.shell.finishItemMenuOpeningGesture()
                onCanceled: if (longPressed && root.shell)
                root.shell.finishItemMenuOpeningGesture()
                onClicked: mouse => {
                    if (pressedIndex < 0)
                    return
                    if (longPressed) {
                        longPressed = false
                        return
                    }
                    if (mouse.button === Qt.RightButton && root.shell)
                    root.shell.openItemMenu(Browse.items.get(pressedIndex), grid.itemAtIndex(pressedIndex))
                    else
                    root.activateCurrent()
                }
                onPressAndHold: if (pressedIndex >= 0 && root.shell) {
                    longPressed = true
                    root.shell.openItemMenu(Browse.items.get(pressedIndex), grid.itemAtIndex(pressedIndex), {
                                                "deferBackdropDismissal": true
                                            })
                }
            }
        }
    }

    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: Metrics.pageMarginPx
        anchors.topMargin: Metrics.pageMarginPx + 54
        width: typeAheadText.implicitWidth + 30
        height: 40
        radius: height / 2
        visible: root.typeAheadBuffer.length > 0
        color: Theme.floatingPanel
        border.width: 1
        border.color: Theme.accent
        z: 30

        AppText {
            id: typeAheadText
            anchors.centerIn: parent
            text: root.typeAheadBuffer
            font.pixelSize: Metrics.bodySizePx
            font.weight: Font.DemiBold
        }
    }

    Timer {
        id: typeAheadReset
        interval: 1250
        repeat: false
        onTriggered: root.typeAheadBuffer = ""
    }

    LazyMenuPanel {
        id: libraryPanel
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: Metrics.pageMarginPx + 52
        anchors.leftMargin: Metrics.pageMarginPx
        width: 360
        open: root.libraryOpen
        maximumHeight: 420
        z: 22
        model: root.libraryEntries
        currentIndex: root.libraryIndex
        edgeEscapeItem: libraryButton
        checkedFor: function (entry) {
            return entry && String(entry.libraryId) === String(Browse.libraryId || "")
        }
        onCurrentIndexChanged: root.libraryIndex = currentIndex
        onDismissed: root.closeMenus()
        onAccepted: entry => root.activateLibraryIndex(entry.index)
    }

    LazyMenuPanel {
        id: sortPanel
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: Metrics.pageMarginPx + 52
        anchors.rightMargin: Metrics.pageMarginPx
        width: 320
        open: root.sortOpen
        maximumHeight: 430
        z: 20
        model: root.sortEntries
        currentIndex: root.sortIndex
        edgeEscapeItem: sortButton
        checkedFor: function (entry) {
            return String(entry && entry.value || "").indexOf("order:") === 0 ? String(entry.value).split(":")[1] === root.currentSortOrder() :
                                                                                entry && entry.value
                                                                                === root.currentSortBy()
        }
        onCurrentIndexChanged: root.sortIndex = currentIndex
        onDismissed: root.closeMenus()
        onAccepted: entry => root.activateSortEntry(entry)
    }

    LazyMenuPanel {
        id: filterPanel
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: Metrics.pageMarginPx + 52
        anchors.rightMargin: Metrics.pageMarginPx
        width: 380
        open: root.filtersOpen
        maximumHeight: Math.min(root.height - Metrics.pageMarginPx * 2 - 70, 620)
        z: 21
        model: root.filterEntries
        currentIndex: root.filterIndex
        edgeEscapeItem: filterButton
        title: "Filters"
        selectionStyle: "check"
        resetVisible: true
        resetEnabled: root.activeFilterCount > 0
        onCurrentIndexChanged: root.filterIndex = currentIndex
        onDismissed: root.closeMenus()
        onAccepted: entry => root.activateFilterEntry(entry)
        onReset: {
            root.savedIndex = 0
            Browse.clearFilters()
        }
    }
}
