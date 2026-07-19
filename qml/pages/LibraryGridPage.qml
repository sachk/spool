pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../theme"
import "../primitives"
import "LibraryNavigation.js" as LibraryNavigation

FocusScope {
    id: root
    property var shell
    property var uiTransitionToken: 0
    readonly property bool listMode: String(Settings.values["appearance/libraryView"] || "Posters") === "List"
    property int columns: listMode ? 1 : Metrics.columns(width)
    readonly property bool largeZoom: Metrics.uiScalePercent >= 150
    readonly property bool smallZoom: Metrics.uiScalePercent <= 100
    readonly property int contentTopMargin: Math.max(Metrics.scaled(8), Math.round(Metrics.pageMarginPx * 0.4))
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
    readonly property bool directionRelease: true
    readonly property int alphabetFeedbackIndex: {
        if (grid.heldKey && grid.currentIndex >= 0)
        return grid.currentIndex
        grid.contentY
        if (grid.count <= 0)
        return -1
        const visibleIndex = grid.indexAt(grid.leftMargin + 1, Math.max(0, grid.contentY) + grid.topMargin + 1)
        if (visibleIndex >= 0)
        return Math.floor(visibleIndex / Math.max(1, root.columns)) * Math.max(1, root.columns)
        return grid.firstLikelyVisibleIndex()
    }
    readonly property string currentAlphabetLabel: {
        if (!Browse.items || alphabetFeedbackIndex < 0 || alphabetFeedbackIndex >= Browse.items.count)
        return "#"
        return LibraryNavigation.sectionLabel(currentSortBy(), Browse.items.get(alphabetFeedbackIndex))
    }
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
    property int savedIndex: shell ? Number(shell.routeArgs.focusIndex || 0) : 0
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
            shell.replaceRoute("libraryGrid", {
                                   libraryId: String(Libraries.get(index).libraryId || ""),
                                   focusIndex: 0
                               })
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

    function toolbarButtons() {
        return [libraryButton, viewButton, sortButton, filterButton, clearFiltersButton].filter(function (button) {
            return button.visible
        })
    }

    function toggleViewMode() {
        Settings.setValue("appearance/libraryView", listMode ? "Posters" : "List")
    }

    // The focused item's poster and synopsis (list mode's left pane). Latched
    // rather than bound so held-key scrolling does not request artwork for
    // every row it passes.
    property var paneItem: ({})

    function updatePane() {
        if (!listMode)
            return
        paneDebounce.stop()
        paneItem = currentMediaItem()
    }

    function schedulePaneUpdate() {
        if (!listMode)
            return
        if (grid.heldKey)
            paneDebounce.restart()
        else
            updatePane()
    }

    onListModeChanged: {
        updatePane()
        Qt.callLater(function () {
            grid.forceLayout()
            grid.ensureCurrentVisible()
            grid.requestMoreIfNeeded()
        })
    }

    // Kodi-style media info for the focused item, formatted natively in one
    // call so scrolling never iterates stream lists from JS.
    readonly property var mediaInfo: {
        if (grid.currentIndex < 0 || grid.count <= 0)
        return null
        const smart = String(Settings.values["audio/trackMode"] || "Default") === "Smart"
        return Browse.mediaInfoFor(grid.currentIndex, smart ? String(Settings.values["subtitles/language"] || "") : "")
    }

    Timer {
        id: paneDebounce
        interval: 140
        repeat: false
        onTriggered: root.updatePane()
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
        if (phase === "release" && InputKeys.isDirection(key))
            return grid.routeKey(key, phase, repeat)
        if (libraryList && libraryList.activeFocus)
            return libraryList.routeKey(key, phase, repeat)
        if (sortList && sortList.activeFocus)
            return sortList.routeKey(key, phase, repeat)
        if (filterList && filterList.activeFocus)
            return filterList.routeKey(key, phase, repeat)

        const toolbar = toolbarButtons()
        const toolbarIndex = toolbar.findIndex(function (button) {
            return button.activeFocus
        })
        if (toolbarIndex >= 0) {
            if (key === Qt.Key_Up) {
                if (hasShell())
                    shell.focusNavBar()
                return true
            }
            if (key === Qt.Key_Down) {
                InputKeys.focus(grid)
                return true
            }
            if (key === Qt.Key_Left || key === Qt.Key_Right) {
                const next = toolbarIndex + (key === Qt.Key_Right ? 1 : -1)
                if (next >= 0 && next < toolbar.length)
                    InputKeys.focus(toolbar[next])
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
        else if (viewButton.activeFocus)
            toggleViewMode()
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
        anchors.leftMargin: Metrics.pageMarginPx
        anchors.rightMargin: Metrics.pageMarginPx
        anchors.topMargin: root.contentTopMargin
        anchors.bottomMargin: 0
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
                id: viewButton
                iconName: root.listMode ? "grid_view" : "view_list"
                label: root.listMode ? "Posters" : "List"
                onActivated: root.toggleViewMode()
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

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: root.listMode ? 0 : Metrics.scaled(6)
            Layout.bottomMargin: mediaInfoBar.visible ? mediaInfoBar.height + Metrics.scaled(10) : 0
            spacing: root.listMode ? Metrics.sectionGapPx : 0

            // List mode's left pane: focused item's poster with the synopsis
            // underneath, Kodi list-view style.
            Item {
                id: listPane
                visible: root.listMode
                Layout.preferredWidth: Math.round(root.width * (root.largeZoom ? 0.36 : 0.28))
                Layout.fillHeight: true

                ImageCard {
                    id: panePoster
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    height: Math.round(Math.min(parent.width * 1.5, parent.height * (root.largeZoom ? 0.64 : 0.56)))
                    width: Math.round(height / 1.5)
                    imageUrl: root.paneItem && root.paneItem.movieId ? Art.url(root.paneItem, "poster", Math.ceil(width)) :
                                                                       ""
                    fallbackText: String(root.paneItem && root.paneItem.title || "")
                }

                Item {
                    id: paneHeading
                    readonly property bool metaInline: paneMeta.text.length > 0 && paneTitle.implicitWidth
                                                       + paneMeta.implicitWidth + Metrics.scaled(10) <= width
                    anchors.top: panePoster.bottom
                    anchors.topMargin: Metrics.scaled(root.smallZoom ? 30 : 22)
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: metaInline ? Math.max(paneTitle.contentHeight, paneMeta.implicitHeight) :
                                         paneTitle.contentHeight + (paneMeta.text.length > 0 ? Metrics.scaled(4)
                                                                                               + paneMeta.implicitHeight :
                                                                                               0)

                    TextMetrics {
                        id: paneTitleMetrics
                        font: paneTitle.font
                        text: paneTitle.text
                    }

                    TextMetrics {
                        id: paneMetaMetrics
                        font: paneMeta.font
                        text: paneMeta.text
                    }

                    AppText {
                        id: paneTitle
                        anchors.top: parent.top
                        anchors.left: parent.left
                        width: paneHeading.metaInline ? implicitWidth : parent.width
                        text: String(root.paneItem && root.paneItem.title || "")
                        font.pixelSize: Metrics.bodySizePx + 4
                        font.weight: Font.DemiBold
                        maximumLineCount: 2
                        wrapMode: Text.Wrap
                        elide: Text.ElideRight
                    }

                    MonoText {
                        id: paneMeta
                        anchors.left: paneHeading.metaInline ? paneTitle.right : parent.left
                        anchors.leftMargin: paneHeading.metaInline ? Metrics.scaled(10) : 0
                        y: paneHeading.metaInline ? Math.round(paneTitle.baselineOffset
                                                               + paneTitleMetrics.tightBoundingRect.y
                                                               + paneTitleMetrics.tightBoundingRect.height / 2
                                                               - paneMeta.baselineOffset
                                                               - paneMetaMetrics.tightBoundingRect.y
                                                               - paneMetaMetrics.tightBoundingRect.height / 2) :
                                                    paneTitle.contentHeight + Metrics.scaled(4)
                        text: String(root.paneItem && root.paneItem.subtitle || "")
                        visible: text.length > 0
                        color: Theme.textMuted
                        font.pixelSize: Metrics.metaSizePx
                        maximumLineCount: 1
                        elide: Text.ElideRight
                    }
                }

                AppText {
                    anchors.top: paneHeading.bottom
                    anchors.topMargin: Metrics.scaled(10)
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    visible: text.length > 0
                    text: String(root.paneItem && root.paneItem.overview || "")
                    color: Theme.textSecondary
                    font.pixelSize: Metrics.bodySizePx
                    lineHeight: 1.2
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                }
            }

            NavGrid {
                id: grid
                property int artworkWindowRevision: 0
                readonly property int memoryMiB: NativeWindow.systemMemoryBytes > 0 ? Math.round(
                                                                                          NativeWindow.systemMemoryBytes
                                                                                          / 1048576) : 2048
                readonly property int artworkMarginRows: Platform.isTV ? (memoryMiB >= 3000 ? 8 : memoryMiB >= 1800 ? 5 : 3) :
                                                                         12
                readonly property int focusPadding: Math.max(2, Metrics.scaled(2))
                holdTraversalSeconds: root.listMode && count > 200 ? 3.5 : 5
                Layout.fillWidth: true
                Layout.fillHeight: true
                focus: true
                clip: true
                keyNavigationEnabled: false
                reuseItems: true
                reducedMotion: Theme.reducedMotion
                opacity: gridReveal.delegatesReady ? 1 : 0
                boundsBehavior: Flickable.StopAtBounds
                model: Browse.items
                leftMargin: focusPadding
                rightMargin: focusPadding + Math.max(Metrics.scaled(12), 12) + Metrics.scaled(8)
                cellWidth: Math.floor((width - leftMargin - rightMargin - Metrics.gapPx * (columns - 1)) / columns)
                cellHeight: root.listMode ? Metrics.scaled(root.largeZoom ? 60 : 54) : cellWidth * 1.5 + Metrics.scaled(
                                                64)

                cacheBuffer: gridReveal.delegatesReady ? cellHeight * artworkMarginRows : 0
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
                    root.updatePane()
                }
                onContentYChanged: {
                    loadMoreDebounce.restart()
                    artworkWindowDebounce.restart()
                    alphabetFeedback.restart()
                }
                onCurrentIndexChanged: {
                    if (currentIndex >= 0)
                    root.savedIndex = currentIndex
                    alphabetFeedback.restart()
                    loadMoreDebounce.restart()
                    routeCheckpoint.restart()
                    root.schedulePaneUpdate()
                }

                FastWheelHandler {
                    flickable: grid
                    animationDuration: Theme.reducedMotion ? 0 : 16
                }
                ScrollBar.vertical: ScrollBar {
                    id: libraryScrollBar
                    readonly property bool activeState: pressed || hovered || grid.moving
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    width: Math.max(Metrics.scaled(12), 12)
                    z: 20
                    interactive: !Platform.isTV
                    policy: ScrollBar.AlwaysOn
                    minimumSize: 0.04
                    contentItem: Rectangle {
                        implicitWidth: Math.max(Metrics.scaled(8), 8)
                        radius: Math.min(Theme.radiusSmall, width / 4)
                        color: libraryScrollBar.activeState ? Theme.accent : Theme.accentDim
                        opacity: 1
                        border.width: Theme.hoverBorderWidth
                        border.color: libraryScrollBar.activeState ? Theme.textPrimary : Theme.accent
                    }
                    background: Rectangle {
                        radius: Math.min(Theme.radiusSmall, width / 4)
                        color: Theme.bgPanel
                        opacity: 1
                        border.width: Theme.hoverBorderWidth
                        border.color: Theme.borderStrong
                    }
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

                function artworkIndexResident(index) {
                    const revision = artworkWindowRevision
                    if (revision < 0 || index < 0 || count <= 0)
                        return false
                    const margin = artworkMarginRows * Math.max(1, columns)
                    const first = Math.max(0, firstLikelyVisibleIndex() - margin)
                    const last = Math.min(count - 1, lastLikelyVisibleIndex() + margin)
                    return index >= first && index <= last
                }

                Timer {
                    id: loadMoreDebounce
                    interval: 80
                    repeat: false
                    onTriggered: grid.requestMoreIfNeeded()
                }

                Timer {
                    id: artworkWindowDebounce
                    interval: 60
                    repeat: false
                    onTriggered: grid.artworkWindowRevision++
                }

                Timer {
                    id: routeCheckpoint
                    interval: 250
                    repeat: false
                    onTriggered: if (root.shell)
                    Router.checkpoint({
                                          libraryId: Browse.libraryId,
                                          focusIndex: Math.max(0, grid.currentIndex)
                                      })
                }

                Timer {
                    id: alphabetFeedback
                    interval: 700
                    repeat: false
                }

                Rectangle {
                    anchors.right: libraryScrollBar.left
                    anchors.rightMargin: Metrics.scaled(10)
                    anchors.verticalCenter: parent.verticalCenter
                    width: Metrics.scaled(52)
                    height: width
                    radius: Theme.radiusMedium
                    color: Theme.accentPanel
                    border.width: Theme.hoverBorderWidth
                    border.color: Theme.accent
                    opacity: alphabetFeedback.running || libraryScrollBar.pressed ? 1 : 0
                    visible: opacity > 0
                    z: 6

                    AppText {
                        anchors.centerIn: parent
                        text: root.currentAlphabetLabel
                        font.pixelSize: Metrics.scaled(26)
                        font.weight: Font.Bold
                        color: Theme.textPrimary
                    }

                    Behavior on opacity {
                        NumberAnimation {
                            duration: Theme.reducedMotion ? 0 : 100
                        }
                    }
                }

                delegate: root.listMode ? listRowComponent : posterCardComponent

                Component {
                    id: posterCardComponent

                    MediaItemCard {
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
                        artworkEnabled: grid.artworkIndexResident(index)

                        Component.onCompleted: gridReveal.schedule()
                        onArtworkReadyChanged: gridReveal.schedule()
                    }
                }

                Component {
                    id: listRowComponent

                    Item {
                        id: listRow

                        required property int index
                        required property string displayTitle
                        required property string displaySubtitle

                        // Consumed by the shared highlight delegate.
                        readonly property real focusOutlineHeight: height - Metrics.scaled(6)

                        width: grid.cellWidth - Metrics.gapPx
                        height: grid.cellHeight

                        AppText {
                            anchors.left: parent.left
                            anchors.leftMargin: Metrics.scaled(14)
                            anchors.right: rowSubtitle.left
                            anchors.rightMargin: Metrics.scaled(16)
                            anchors.verticalCenter: parent.verticalCenter
                            text: listRow.displayTitle
                            font.pixelSize: Metrics.bodySizePx + Metrics.scaled(2)
                            font.weight: listRow.GridView.isCurrentItem ? Font.DemiBold : Font.Medium
                            color: listRow.GridView.isCurrentItem ? Theme.textPrimary : Theme.textSecondary
                            maximumLineCount: 1
                            elide: Text.ElideRight
                        }

                        MonoText {
                            id: rowSubtitle
                            anchors.right: parent.right
                            anchors.rightMargin: Metrics.scaled(14)
                            anchors.verticalCenter: parent.verticalCenter
                            text: listRow.displaySubtitle
                            color: Theme.textMuted
                            font.pixelSize: Metrics.metaSizePx + Metrics.scaled(1)
                            maximumLineCount: 1
                        }

                        Component.onCompleted: gridReveal.schedule()
                    }
                }
                highlightFollowsCurrentItem: true
                highlight: Item {
                    width: grid.currentItem ? grid.currentItem.width : grid.cellWidth - Metrics.gapPx
                    height: grid.currentItem ? grid.currentItem.focusOutlineHeight : grid.cellHeight - Metrics.scaled(6)

                    Rectangle {
                        anchors.fill: parent
                        anchors.leftMargin: -grid.focusPadding
                        anchors.rightMargin: -grid.focusPadding
                        color: "transparent"
                        radius: Theme.radiusMedium
                        border.width: Theme.focusBorderWidth
                        border.color: grid.activeFocus ? Theme.accent : "transparent"
                        z: 2
                    }
                }

                MouseArea {
                    property int pressedIndex: -1
                    property bool longPressed: false
                    property bool pressedCurrent: false
                    anchors.fill: parent
                    z: 3
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    pressAndHoldInterval: 520
                    onPressed: mouse => {
                        longPressed = false
                        pressedCurrent = false
                        pressedIndex = grid.indexAt(mouse.x + grid.contentX, mouse.y + grid.contentY)
                        pressedCurrent = pressedIndex === grid.currentIndex
                        if (pressedIndex >= 0) {
                            grid.currentIndex = pressedIndex
                            grid.forceActiveFocus()
                        }
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
                        else if (!root.listMode || pressedCurrent)
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
    }

    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: Metrics.pageMarginPx
        anchors.topMargin: root.contentTopMargin + 54
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

    // Kodi-style single-line media info for the focused item.
    TechnicalDetailsBar {
        id: mediaInfoBar
        visible: root.mediaInfo !== null && Theme.technicalMetadataMode === "Always" && hasContent
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 0
        anchors.right: parent.right
        anchors.rightMargin: 0
        width: root.listMode ? grid.cellWidth - Metrics.gapPx : Math.max(0, grid.width - grid.leftMargin
                                                                         - grid.rightMargin)
        height: implicitHeight
        info: root.mediaInfo
        z: 18
    }

    LazyMenuPanel {
        id: libraryPanel
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: root.contentTopMargin + 52
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
        anchors.topMargin: root.contentTopMargin + 52
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
        anchors.topMargin: root.contentTopMargin + 52
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
