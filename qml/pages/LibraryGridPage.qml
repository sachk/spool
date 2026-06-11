import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    property var shell
    property int columns: Metrics.columns(width)
    property bool sortOpen: false
    property bool filtersOpen: false
    property bool libraryOpen: false
    property int sortIndex: 0
    property int filterIndex: 0
    property int libraryIndex: 0
    property var sortEntries: []
    property var filterEntries: []
    readonly property string collectionType: appController ? appController.currentLibraryCollectionType : ""
    readonly property var libraryQuery: appController ? appController.libraryQuery : ({})
    readonly property var filterOptions: appController ? appController.libraryFilterOptions : ({})
    readonly property int activeFilterCount: appController ? appController.libraryFilterActiveCount : 0
    // Genre/studio tag pages reuse this grid but have no library switcher,
    // sort or filter controls — they are a fixed, server-filtered listing.
    readonly property bool isTagView: appController && (appController.currentViewKind === "genre" || appController.currentViewKind === "studio")
    focus: true
    Accessible.role: Accessible.Pane
    Accessible.name: appController && appController.currentLibraryName
                     ? appController.currentLibraryName : "Library"
    Component.onCompleted: grid.forceActiveFocus()
    onActiveFocusChanged: if (activeFocus) grid.forceActiveFocus()

    component ToolbarButton: FocusScope {
        id: buttonRoot
        property string iconName: "filter_list"
        property string label: ""
        property bool checked: false
        property bool badge: false
        signal activated()

        width: Math.max(118, labelText.implicitWidth + 58)
        height: 42
        focus: true

        HoverHandler { id: hover }

        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: buttonRoot.checked ? Theme.accentPanel
                  : (buttonRoot.activeFocus || hover.hovered) ? Theme.bgHover
                  : Theme.bgRaised
            border.width: buttonRoot.activeFocus ? 2 : 1
            border.color: buttonRoot.activeFocus ? Theme.textPrimary
                        : buttonRoot.checked ? Theme.accent
                        : Theme.border
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

        MouseArea { anchors.fill: parent; onClicked: buttonRoot.activated() }
        Keys.onReleased: (event) => {
            if (InputKeys.isAccept(event.key)) {
                buttonRoot.activated()
                event.accepted = true
            }
        }
    }

    component PopupRow: FocusScope {
        id: rowRoot
        property string label: ""
        property string detail: ""
        property string iconName: "check_box_outline_blank"
        property bool selected: false
        property int optionIndex: 0
        signal activated()

        width: parent ? parent.width : 320
        height: detail.length > 0 ? 54 : 44
        focus: true
        Accessible.role: Accessible.MenuItem
        Accessible.name: label
        Accessible.description: detail
        Accessible.focusable: true
        Accessible.focused: activeFocus
        Accessible.selected: selected
        Accessible.onPressAction: activated()

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusSmall
            color: rowRoot.activeFocus ? Theme.focusedFill : "transparent"
            border.width: rowRoot.activeFocus ? 1 : 0
            border.color: Theme.accent
        }

        Row {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 9

            MaterialIcon {
                anchors.verticalCenter: parent.verticalCenter
                name: rowRoot.iconName
                iconSize: 20
                iconColor: rowRoot.selected ? Theme.accent : Theme.textSecondary
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 42
                spacing: 1

                AppText {
                    width: parent.width
                    text: rowRoot.label
                    font.pixelSize: Metrics.metaPx(root.width) + 1
                    font.weight: Font.Medium
                    color: rowRoot.selected ? Theme.textPrimary : Theme.textSecondary
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }

                MonoText {
                    width: parent.width
                    visible: rowRoot.detail.length > 0
                    text: rowRoot.detail
                    color: Theme.textMuted
                    font.pixelSize: Metrics.metaPx(root.width) - 1
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }
            }
        }

        MouseArea { anchors.fill: parent; onClicked: rowRoot.activated() }
    }

    function currentItemData() {
        return grid.currentIndex >= 0 && appController && appController.movies
                ? appController.movies.get(grid.currentIndex)
                : ({})
    }

    function libraryCount() {
        return appController && appController.libraries ? appController.libraries.rowCount() : 0
    }

    function currentLibraryModelIndex() {
        const currentId = appController ? String(appController.currentLibraryId || "") : ""
        const count = libraryCount()
        for (let i = 0; i < count; ++i) {
            const library = appController.libraries.get(i)
            if (library && String(library.libraryId || "") === currentId)
                return i
        }
        return count > 0 ? 0 : -1
    }

    function headerDetail() {
        const total = appController.currentItemsTotalCount
        const count = appController.movies ? appController.movies.count : 0
        const parts = []
        if (count > 0) parts.push(count + (total > count ? " of " + total : "") + " items")
        if (activeFilterCount > 0) parts.push(activeFilterCount + " filter" + (activeFilterCount === 1 ? "" : "s"))
        return parts.join(" · ")
    }

    function shortPressPlays(item) {
        if (!item)
            return false
        if (item.itemType === "Season")
            return true
        return appController && appController.currentViewKind === "episodes" && item.itemType === "Episode"
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
        return String(value).split(",").filter(function(v) { return v.length > 0 })
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
        return collectionType === "movies" || collectionType === "homevideos" || collectionType === "musicvideos" || collectionType.length === 0
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
            { label: "Name", value: "SortName" },
            { label: "Random", value: "Random" },
            { label: "Community rating", value: "CommunityRating" },
            { label: "Date added", value: "DateCreated" },
            { label: "Date played", value: isSeriesLibrary() ? "SeriesDatePlayed" : "DatePlayed" },
            { label: "Parental rating", value: "OfficialRating" },
            { label: "Release date", value: "PremiereDate" }
        ]
        if (isSeriesLibrary())
            common.splice(4, 0, { label: "Date episode added", value: "DateLastContentAdded" })
        else
            common.splice(3, 0, { label: "Critic rating", value: "CriticRating" })
        common.push({ label: "Play count", value: "PlayCount" })
        common.push({ label: "Runtime", value: "Runtime" })
        common.push({ label: "Ascending", value: "order:Ascending" })
        common.push({ label: "Descending", value: "order:Descending" })
        return common
    }

    function addSection(entries, title) {
        entries.push({ section: true, label: title })
    }

    function addListFilter(entries, section, label, key, value) {
        entries.push({ section: false, sectionName: section, label: label, key: key, value: String(value), kind: "list", checked: listHas(key, value) })
    }

    function addBoolFilter(entries, section, label, key) {
        entries.push({ section: false, sectionName: section, label: label, key: key, kind: "bool", checked: boolHas(key, true) })
    }

    function addNullableBoolFilter(entries, section, label, key, value) {
        entries.push({ section: false, sectionName: section, label: label, key: key, value: value, kind: "nullableBool", checked: boolHas(key, value) })
    }

    function buildFilterEntries() {
        const entries = []
        addSection(entries, "Status")
        addListFilter(entries, "Status", "Played", "filters", "IsPlayed")
        addListFilter(entries, "Status", "Unplayed", "filters", "IsUnplayed")
        addListFilter(entries, "Status", "Favorite", "filters", "IsFavorite")
        addListFilter(entries, "Status", "Continue watching", "filters", "IsResumable")

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

    function openSortMenu() {
        libraryOpen = false
        filtersOpen = false
        sortEntries = buildSortEntries()
        sortIndex = Math.max(0, sortEntries.findIndex(function(entry) { return entry.value === currentSortBy() }))
        sortOpen = true
        Qt.callLater(function() { sortList.forceActiveFocus() })
    }

    function openFilterMenu() {
        libraryOpen = false
        sortOpen = false
        filterEntries = buildFilterEntries()
        filterIndex = filterEntries.length > 1 ? 1 : 0
        filtersOpen = true
        Qt.callLater(function() { filterList.forceActiveFocus() })
    }

    function openLibraryMenu() {
        if (root.isTagView || libraryCount() <= 0)
            return
        sortOpen = false
        filtersOpen = false
        libraryIndex = Math.max(0, currentLibraryModelIndex())
        libraryOpen = true
        Qt.callLater(function() { libraryList.forceActiveFocus() })
    }

    function closeMenus() {
        libraryOpen = false
        sortOpen = false
        filtersOpen = false
        grid.forceActiveFocus()
    }

    // Consume Back when a toolbar menu is open so it dismisses the menu rather
    // than navigating out of the library.
    function handleBack() {
        if (libraryOpen || sortOpen || filtersOpen) {
            closeMenus()
            return true
        }
        return false
    }

    function activateLibraryIndex(index) {
        if (!appController || index < 0 || index >= libraryCount())
            return
        libraryOpen = false
        sortOpen = false
        filtersOpen = false
        shell.lastLibraryIndex = index
        shell.lastGridIndex = 0
        appController.openLibrary(index)
        shell.replaceRoute("libraryGrid")
        grid.forceActiveFocus()
    }

    function activateSortEntry(entry) {
        if (!entry)
            return
        shell.lastGridIndex = 0
        if (String(entry.value).indexOf("order:") === 0)
            appController.setLibrarySort(currentSortBy(), String(entry.value).split(":")[1])
        else
            appController.setLibrarySort(entry.value, currentSortOrder())
        sortEntries = buildSortEntries()
    }

    function activateFilterEntry(entry) {
        if (!entry || entry.section)
            return
        shell.lastGridIndex = 0
        if (entry.kind === "list")
            appController.setLibraryQueryListValue(entry.key, entry.value, !entry.checked)
        else if (entry.kind === "bool")
            appController.setLibraryQueryBoolValue(entry.key, !entry.checked)
        else if (entry.kind === "nullableBool")
            appController.setLibraryQueryNullableBoolValue(entry.key, entry.checked ? null : entry.value)
        filterEntries = buildFilterEntries()
    }

    function focusToolbar() {
        libraryButton.forceActiveFocus()
    }

    Connections {
        target: appController
        function onLibraryQueryChanged() {
            if (root.filtersOpen)
                root.filterEntries = root.buildFilterEntries()
            if (root.sortOpen)
                root.sortEntries = root.buildSortEntries()
        }
        function onLibraryFilterOptionsChanged() {
            if (root.filtersOpen)
                root.filterEntries = root.buildFilterEntries()
        }
        function onCurrentItemsPagingChanged() {
            if (appController.currentItemsLoadingMore)
                root.Accessible.announce("Loading more items")
            else if (appController.movies && appController.movies.rowCount() > 0)
                root.Accessible.announce(root.headerDetail())
        }
    }

    function activateCurrent() {
        if (grid.currentIndex < 0)
            return
        shell.lastGridIndex = grid.currentIndex
        const item = currentItemData()
        if (shortPressPlays(item)) {
            appController.playMovie(grid.currentIndex)
            return
        }
        openCurrentDetails()
    }

    function openCurrentDetails() {
        if (grid.currentIndex < 0)
            return
        shell.lastGridIndex = grid.currentIndex
        shell.openDetailsAt(appController.movies, grid.currentIndex, "movies", "libraryGrid")
    }

    function currentCard() {
        return grid.currentItem
    }

    function handlePressedKey(key) {
        const card = currentCard()
        return card && card.handleAcceptPressed ? card.handleAcceptPressed(key) : false
    }

    function handleNavigationKey(key) {
        if (libraryList.activeFocus) {
            if (InputKeys.isBack(key, false, false) || key === Qt.Key_Left) {
                closeMenus()
                return true
            }
            if (key === Qt.Key_Down) {
                libraryIndex = Math.min(libraryCount() - 1, libraryIndex + 1)
                libraryList.currentIndex = libraryIndex
                return true
            }
            if (key === Qt.Key_Up) {
                if (libraryIndex <= 0) {
                    libraryButton.forceActiveFocus()
                } else {
                    libraryIndex = Math.max(0, libraryIndex - 1)
                    libraryList.currentIndex = libraryIndex
                }
                return true
            }
            if (InputKeys.isAccept(key)) {
                activateLibraryIndex(libraryIndex)
                return true
            }
            return false
        }

        if (sortList.activeFocus) {
            if (InputKeys.isBack(key, false, false) || key === Qt.Key_Left) {
                closeMenus()
                return true
            }
            if (key === Qt.Key_Down) {
                sortIndex = Math.min(sortEntries.length - 1, sortIndex + 1)
                sortList.currentIndex = sortIndex
                return true
            }
            if (key === Qt.Key_Up) {
                if (sortIndex <= 0) {
                    sortButton.forceActiveFocus()
                } else {
                    sortIndex = Math.max(0, sortIndex - 1)
                    sortList.currentIndex = sortIndex
                }
                return true
            }
            if (InputKeys.isAccept(key)) {
                activateSortEntry(sortEntries[sortIndex])
                return true
            }
            return false
        }

        if (filterList.activeFocus) {
            if (InputKeys.isBack(key, false, false) || key === Qt.Key_Left) {
                closeMenus()
                return true
            }
            if (key === Qt.Key_Down) {
                filterIndex = Math.min(filterEntries.length - 1, filterIndex + 1)
                while (filterIndex < filterEntries.length - 1 && filterEntries[filterIndex].section)
                    filterIndex += 1
                filterList.currentIndex = filterIndex
                return true
            }
            if (key === Qt.Key_Up) {
                if (filterIndex <= 0) {
                    filterButton.forceActiveFocus()
                } else {
                    filterIndex = Math.max(0, filterIndex - 1)
                    while (filterIndex > 0 && filterEntries[filterIndex].section)
                        filterIndex -= 1
                    filterList.currentIndex = filterIndex
                }
                return true
            }
            if (InputKeys.isAccept(key)) {
                activateFilterEntry(filterEntries[filterIndex])
                return true
            }
            return false
        }

        if (libraryButton.activeFocus || sortButton.activeFocus || filterButton.activeFocus || clearFiltersButton.activeFocus) {
            if (key === Qt.Key_Up) {
                shell.focusNavBar()
                return true
            }
            if (key === Qt.Key_Left) {
                if (sortButton.activeFocus) libraryButton.forceActiveFocus()
                else if (filterButton.activeFocus) sortButton.forceActiveFocus()
                else if (clearFiltersButton.activeFocus) filterButton.forceActiveFocus()
                return true
            }
            if (key === Qt.Key_Right) {
                if (libraryButton.activeFocus && sortButton.visible) sortButton.forceActiveFocus()
                else if (sortButton.activeFocus) filterButton.forceActiveFocus()
                else if (filterButton.activeFocus && clearFiltersButton.visible) clearFiltersButton.forceActiveFocus()
                return true
            }
            if (key === Qt.Key_Down) {
                grid.forceActiveFocus()
                return true
            }
            if (InputKeys.isAccept(key)) {
                if (libraryButton.activeFocus) openLibraryMenu()
                else if (sortButton.activeFocus) openSortMenu()
                else if (filterButton.activeFocus) openFilterMenu()
                else appController.clearLibraryFilters()
                return true
            }
        }

        if (grid.count <= 0)
            return false
        const acceptKey = InputKeys.isAccept(key)
        const card = currentCard()
        if (!acceptKey && card && card.handleNavigationKey && card.handleNavigationKey(key))
            return true
        if (key === Qt.Key_Left) {
            if (grid.currentIndex % columns !== 0)
                grid.currentIndex = Math.max(0, grid.currentIndex - 1)
            return true
        }
        if (key === Qt.Key_Right) {
            grid.currentIndex = Math.min(grid.count - 1, grid.currentIndex + 1)
            return true
        }
        if (key === Qt.Key_Up) {
            if (grid.currentIndex < columns) {
                focusToolbar()
                return true
            }
            grid.currentIndex = Math.max(0, grid.currentIndex - columns)
            return true
        }
        if (key === Qt.Key_Down) {
            grid.currentIndex = Math.min(grid.count - 1, grid.currentIndex + columns)
            return true
        }
        if (acceptKey) {
            if (card && card.handleAcceptReleased && card.handleAcceptReleased(key))
                return true
            if (card && card.handleNavigationKey && card.handleNavigationKey(key))
                return true
            activateCurrent()
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
                    width: Math.min(titleText.implicitWidth + 30, Math.max(160, parent.width - headerDetailText.width - 32))

                    AppText {
                        id: titleText
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.max(0, titleRow.width - 30)
                        text: appController.currentLibraryName.length > 0 ? appController.currentLibraryName : "Library"
                        font.pixelSize: Metrics.bodyPx(root.width) + 4
                        font.weight: Font.DemiBold
                        maximumLineCount: 1
                        elide: Text.ElideRight
                    }

                    MaterialIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !root.isTagView
                        name: root.libraryOpen ? "expand_less" : "expand_more"
                        iconSize: 24
                        iconColor: root.libraryOpen || libraryButton.activeFocus ? Theme.textPrimary : Theme.textSecondary
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

                MouseArea { anchors.fill: parent; onClicked: root.openLibraryMenu() }
            }

            ToolbarButton {
                id: sortButton
                visible: !root.isTagView
                iconName: root.currentSortOrder() === "Descending" ? "south" : "north"
                label: root.currentSortLabel()
                checked: root.sortOpen
                onActivated: root.openSortMenu()
            }

            ToolbarButton {
                id: filterButton
                visible: !root.isTagView
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
                visible: !root.isTagView && root.activeFilterCount > 0
                onActivated: {
                    shell.lastGridIndex = 0
                    appController.clearLibraryFilters()
                }
            }
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: true
            clip: true
            keyNavigationEnabled: false
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
            model: appController.movies
            cellWidth: Math.floor((width - Metrics.gap(root.width) * (columns - 1)) / columns)
            cellHeight: cellWidth * 1.5 + 64
            Component.onCompleted: {
                restoreIndex()
                requestMoreIfNeeded()
            }
            onCountChanged: {
                restoreIndex()
                requestMoreIfNeeded()
            }
            onContentYChanged: loadMoreDebounce.restart()
            onCurrentIndexChanged: {
                shell.lastGridIndex = currentIndex
                ensureCurrentVisible()
                loadMoreDebounce.restart()
            }

            FastWheelHandler { flickable: grid }

            function restoreIndex() {
                currentIndex = count > 0 ? Math.max(0, Math.min(shell.lastGridIndex, count - 1)) : -1
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
                return Math.min(count - 1,
                                Math.max(0, Math.floor(contentY / cellHeight) * columns))
            }

            function requestMoreIfNeeded() {
                if (!appController || count <= 0)
                    return
                const visibleHead = firstLikelyVisibleIndex()
                const visibleTail = Math.max(currentIndex, lastLikelyVisibleIndex())
                appController.prefetchCurrentItems(visibleHead, visibleTail)
                appController.maybeLoadMoreCurrentItems(visibleTail)
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
                required property string title
                required property string posterUrl
                required property string seriesPosterUrl
                required property int year
                required property string subtitle
                required property string displayTitle
                required property string displaySubtitle
                required property string movieId
                required property bool favorite
                required property bool played
                width: grid.cellWidth
                height: grid.cellHeight

                function handleAcceptPressed(key) { return card.handleAcceptPressed(key) }
                function handleAcceptReleased(key) { return card.handleAcceptReleased(key) }
                function handleNavigationKey(key) { return card.handleNavigationKey(key) }

                readonly property var itemData: appController.movies.get(index)

                MediaItemCard {
                    id: card
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.rightMargin: Metrics.gap(root.width)
                    height: parent.height
                    item: gridDelegate.itemData
                    kind: "poster"
                    focused: gridDelegate.GridView.isCurrentItem
                    longPressAction: root.shortPressPlays(gridDelegate.itemData) ? "details" : "menu"
                    onActivated: {
                        grid.currentIndex = index
                        shell.lastGridIndex = index
                        shell.lastGridY = grid.contentY
                        root.activateCurrent()
                    }
                    onDetailsRequested: {
                        grid.currentIndex = index
                        shell.lastGridIndex = index
                        shell.lastGridY = grid.contentY
                        root.openCurrentDetails()
                    }
                    onFavoriteToggled: (favorite) => appController.setFavorite(gridDelegate.movieId || "", favorite)
                    onPlayedToggled: (played) => appController.setPlayed(gridDelegate.movieId || "", played)
                    onMediaInfoRequested: shell.openMediaInfo(gridDelegate.itemData)
                }
            }
            Keys.onReleased: (event) => {
                if (event.key === Qt.Key_Up && currentIndex < columns) { shell.focusNavBar(); event.accepted = true }
                else if (InputKeys.isAccept(event.key)) {
                    const card = root.currentCard()
                    if (card && card.handleAcceptReleased && card.handleAcceptReleased(event.key)) {
                        event.accepted = true
                        return
                    }
                    root.activateCurrent()
                    event.accepted = true
                }
                else if (event.key === Qt.Key_M) { shell.lastGridIndex = currentIndex; shell.openMediaInfo(appController.movies.get(currentIndex)); event.accepted = true }
            }
        }
    }

    Surface {
        id: libraryPanel
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: Metrics.pageMargin(root.width) + 52
        anchors.leftMargin: Metrics.pageMargin(root.width)
        width: 360
        height: root.libraryOpen ? Math.min(420, libraryList.contentHeight + 20) : 0
        visible: root.libraryOpen
        z: 22
        baseColor: Theme.bgRaised
        elevated: true
        clip: true
        Accessible.role: Accessible.PopupMenu
        Accessible.name: "Libraries"

        ListView {
            id: libraryList
            anchors.fill: parent
            anchors.margins: 10
            clip: true
            focus: true
            keyNavigationEnabled: false
            model: appController.libraries
            currentIndex: root.libraryIndex
            boundsBehavior: Flickable.StopAtBounds
            Accessible.role: Accessible.List
            Accessible.name: "Libraries"
            Accessible.focusable: count > 0
            Accessible.focused: activeFocus
            onCurrentIndexChanged: {
                root.libraryIndex = currentIndex
                if (currentIndex >= 0)
                    positionViewAtIndex(currentIndex, ListView.Contain)
            }

            FastWheelHandler { flickable: libraryList }

            delegate: PopupRow {
                required property int index
                required property string name
                required property string collectionType
                required property string libraryId
                width: libraryList.width
                optionIndex: index
                label: name
                selected: String(libraryId || "") === String(appController.currentLibraryId || "")
                iconName: selected ? "radio_button_checked" : "radio_button_unchecked"
                focus: ListView.isCurrentItem && libraryList.activeFocus
                onActivated: {
                    libraryList.currentIndex = index
                    root.activateLibraryIndex(index)
                }
            }

            Keys.onReleased: (event) => {
                if (root.handleNavigationKey(event.key))
                    event.accepted = true
            }
        }
    }

    Surface {
        id: sortPanel
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: Metrics.pageMargin(root.width) + 52
        anchors.rightMargin: Metrics.pageMargin(root.width)
        width: 320
        height: root.sortOpen ? Math.min(430, sortList.contentHeight + 20) : 0
        visible: root.sortOpen
        z: 20
        baseColor: Theme.bgRaised
        elevated: true
        clip: true
        Accessible.role: Accessible.PopupMenu
        Accessible.name: "Sort library"

        ListView {
            id: sortList
            anchors.fill: parent
            anchors.margins: 10
            clip: true
            focus: true
            keyNavigationEnabled: false
            model: root.sortEntries
            currentIndex: root.sortIndex
            boundsBehavior: Flickable.StopAtBounds
            Accessible.role: Accessible.List
            Accessible.name: "Sort options"
            Accessible.focusable: count > 0
            Accessible.focused: activeFocus
            onCurrentIndexChanged: {
                root.sortIndex = currentIndex
                if (currentIndex >= 0)
                    positionViewAtIndex(currentIndex, ListView.Contain)
            }

            FastWheelHandler { flickable: sortList }

            delegate: PopupRow {
                required property int index
                required property var modelData
                width: sortList.width
                optionIndex: index
                label: modelData.label || ""
                selected: String(modelData.value || "").indexOf("order:") === 0
                          ? String(modelData.value).split(":")[1] === root.currentSortOrder()
                          : modelData.value === root.currentSortBy()
                iconName: selected ? "radio_button_checked" : "radio_button_unchecked"
                focus: ListView.isCurrentItem && sortList.activeFocus
                onActivated: {
                    sortList.currentIndex = index
                    root.activateSortEntry(modelData)
                }
            }

            Keys.onReleased: (event) => {
                if (root.handleNavigationKey(event.key))
                    event.accepted = true
            }
        }
    }

    Surface {
        id: filterPanel
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: Metrics.pageMargin(root.width) + 52
        anchors.rightMargin: Metrics.pageMargin(root.width)
        width: 380
        height: root.filtersOpen ? Math.min(root.height - Metrics.pageMargin(root.width) * 2 - 70, 620) : 0
        visible: root.filtersOpen
        z: 21
        baseColor: Theme.bgRaised
        elevated: true
        clip: true
        Accessible.role: Accessible.PopupMenu
        Accessible.name: "Filter library"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
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
                        shell.lastGridIndex = 0
                        appController.clearLibraryFilters()
                    }
                }
            }

            ListView {
                id: filterList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                focus: true
                keyNavigationEnabled: false
                model: root.filterEntries
                currentIndex: root.filterIndex
                boundsBehavior: Flickable.StopAtBounds
                Accessible.role: Accessible.List
                Accessible.name: "Filter options"
                Accessible.focusable: count > 0
                Accessible.focused: activeFocus
                onCurrentIndexChanged: {
                    root.filterIndex = currentIndex
                    if (currentIndex >= 0)
                        positionViewAtIndex(currentIndex, ListView.Contain)
                }

                FastWheelHandler { flickable: filterList }

                delegate: Item {
                    required property int index
                    required property var modelData
                    width: filterList.width
                    height: modelData.section ? 34 : 48
                    Accessible.role: modelData.section ? Accessible.Heading : Accessible.NoRole
                    Accessible.name: modelData.section ? (modelData.label || "") : ""

                    AppText {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        visible: modelData.section === true
                        text: modelData.label || ""
                        color: Theme.textMuted
                        font.pixelSize: Metrics.metaPx(root.width)
                        font.weight: Font.DemiBold
                        maximumLineCount: 1
                        elide: Text.ElideRight
                    }

                    PopupRow {
                        anchors.fill: parent
                        visible: modelData.section !== true
                        optionIndex: index
                        label: modelData.label || ""
                        selected: modelData.checked === true
                        iconName: selected ? "check_box" : "check_box_outline_blank"
                        focus: ListView.isCurrentItem && filterList.activeFocus
                        onActivated: {
                            filterList.currentIndex = index
                            root.activateFilterEntry(modelData)
                        }
                    }
                }

                Keys.onReleased: (event) => {
                    if (root.handleNavigationKey(event.key))
                        event.accepted = true
                }
            }
        }
    }
}
