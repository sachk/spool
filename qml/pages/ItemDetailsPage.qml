import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    property var shell
    readonly property var itemModel: shell && shell.detailsModel ? shell.detailsModel : appController.movies
    readonly property int itemCount: itemModel && itemModel.rowCount ? itemModel.rowCount() : 0
    readonly property int selectedIndex: itemCount > 0 ? Math.max(0, Math.min(shell ? shell.detailsIndex : 0, itemCount - 1)) : -1
    readonly property var item: selectedIndex >= 0 && itemModel ? itemModel.get(selectedIndex) : ({})
    readonly property string detailSource: shell ? shell.detailsSource : "movies"
    readonly property string titleText: item.displayTitle || item.title || item.seriesName || "Selected item"
    readonly property string parentText: item.itemType === "Episode" && item.seriesName ? item.seriesName : ""
    readonly property string typeText: item.itemType || "Media"
    readonly property string subtitleText: item.displaySubtitle || item.subtitle || ""
    readonly property bool canPlay: item.playable === undefined || item.playable
    readonly property bool showPrimaryAction: selectedIndex >= 0 && canPlay
    readonly property bool hasProgress: Number(item.resumeTicks || 0) > 0 && Number(item.runtimeTicks || 0) > 0
    readonly property int contentMargin: Metrics.pageMargin(width)
    readonly property int posterWidth: Math.min(340, Math.max(220, width * 0.19))
    readonly property int rowPosterWidth: Math.min(176, Math.max(132, width * 0.096))
    readonly property int rowGap: Math.max(14, Metrics.gap(width))
    readonly property string backgroundArt: item.backdropUrl || item.thumbUrl || item.posterUrl || ""
    readonly property string titleArt: item.logoUrl || item.bannerUrl || ""
    readonly property bool showSeasonsRow: typeText === "Series" && seasonCount > 0
    readonly property bool showSimilarRow: similarCount > 0
    readonly property var people: item.people || []
    readonly property bool showPeopleRow: people.length > 0
    property int seasonCount: appController && appController.detailSeasons ? appController.detailSeasons.rowCount() : 0
    property int similarCount: appController && appController.detailSimilarItems ? appController.detailSimilarItems.rowCount() : 0
    property bool favoriteState: false
    property bool playedState: false
    property bool overflowOpen: false
    property string loadedDetailKey: ""
    focus: true

    component DetailAction: FocusScope {
        id: actionRoot
        property string iconName: "play_arrow"
        property string label: ""
        property bool primary: false
        property bool enabledButton: true
        signal activated()

        width: Math.min(Math.max(actionLabel.implicitWidth + 74, 150), 260)
        height: 50
        focus: true
        opacity: enabledButton ? 1.0 : 0.45

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusMedium
            color: actionRoot.primary ? Theme.accentPanel : "#B51B1B1B"
            border.width: actionRoot.activeFocus ? 2 : 1
            border.color: actionRoot.activeFocus ? Theme.accent : Theme.border
            antialiasing: true
        }

        Row {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 9

            MaterialIcon {
                anchors.verticalCenter: parent.verticalCenter
                name: actionRoot.iconName
                iconSize: 23
                iconColor: actionRoot.enabledButton ? Theme.textPrimary : Theme.textDisabled
            }

            AppText {
                id: actionLabel
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(0, actionRoot.width - 64)
                text: actionRoot.label
                color: actionRoot.enabledButton ? Theme.textPrimary : Theme.textDisabled
                font.pixelSize: Metrics.metaPx(root.width) + 1
                font.weight: actionRoot.primary ? Font.DemiBold : Font.Medium
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }

        MouseArea {
            anchors.fill: parent
            enabled: actionRoot.enabledButton
            onClicked: actionRoot.activated()
        }

        Keys.onReleased: (event) => {
            if (!actionRoot.enabledButton)
                return
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Select || event.key === Qt.Key_Space) {
                actionRoot.activated()
                event.accepted = true
            }
        }
    }

    component IconAction: FocusScope {
        id: iconRoot
        property string iconName: "menu"
        property string label: ""
        property bool checked: false
        property bool enabledButton: true
        signal activated()

        width: 50
        height: 50
        focus: true
        opacity: enabledButton ? 1.0 : 0.45

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusMedium
            color: iconRoot.checked ? Theme.accentPanel : "#B51B1B1B"
            border.width: iconRoot.activeFocus ? 2 : 1
            border.color: iconRoot.activeFocus ? Theme.accent : iconRoot.checked ? Theme.accentDim : Theme.border
            antialiasing: true
        }

        MaterialIcon {
            anchors.centerIn: parent
            name: iconRoot.iconName
            iconSize: 24
            iconColor: iconRoot.checked ? Theme.accent : Theme.textPrimary
        }

        MouseArea {
            anchors.fill: parent
            enabled: iconRoot.enabledButton
            onClicked: iconRoot.activated()
        }

        Keys.onReleased: (event) => {
            if (!iconRoot.enabledButton)
                return
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Select || event.key === Qt.Key_Space) {
                iconRoot.activated()
                event.accepted = true
            }
        }
    }

    component MenuOption: FocusScope {
        id: optionRoot
        property string iconName: "info"
        property string label: ""
        signal activated()

        width: parent ? parent.width : 280
        height: 48
        focus: true

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusSmall
            color: optionRoot.activeFocus ? "#2A3034" : "transparent"
            border.width: optionRoot.activeFocus ? 1 : 0
            border.color: Theme.accent
        }

        Row {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 10

            MaterialIcon {
                anchors.verticalCenter: parent.verticalCenter
                name: optionRoot.iconName
                iconSize: 21
                iconColor: Theme.textSecondary
            }

            AppText {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 48
                text: optionRoot.label
                color: Theme.textPrimary
                font.pixelSize: Metrics.metaPx(root.width) + 1
                font.weight: Font.Medium
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: optionRoot.activated()
        }

        Keys.onReleased: (event) => {
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Select || event.key === Qt.Key_Space) {
                optionRoot.activated()
                event.accepted = true
            }
        }
    }

    component InfoLine: RowLayout {
        id: infoRoot
        property string label: ""
        property string value: ""
        visible: value.length > 0
        spacing: 10

        AppText {
            Layout.preferredWidth: Math.min(112, Math.max(80, root.width * 0.06))
            text: infoRoot.label
            color: Theme.textMuted
            font.pixelSize: Metrics.metaPx(root.width)
            font.weight: Font.Medium
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        AppText {
            Layout.fillWidth: true
            text: infoRoot.value
            color: Theme.textSecondary
            font.pixelSize: Metrics.metaPx(root.width)
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
    }

    component DetailPosterRow: FocusScope {
        id: rowRoot
        property string title: ""
        property var rowModel
        property int cardWidth: root.rowPosterWidth
        property int currentIndex: 0
        readonly property int rowCount: rowModel && rowModel.rowCount ? rowModel.rowCount() : 0
        readonly property int cardHeight: Math.round(cardWidth * 1.5 + 60)
        readonly property int headerHeight: 34
        readonly property int rowHeight: rowCount > 0 ? headerHeight + 10 + cardHeight : 0
        signal activated(int index)

        Layout.fillWidth: true
        Layout.preferredHeight: visible ? rowHeight : 0
        Layout.minimumHeight: visible ? rowHeight : 0
        Layout.maximumHeight: visible ? rowHeight : 0
        implicitHeight: visible ? rowHeight : 0
        height: visible ? rowHeight : 0
        visible: rowCount > 0
        focus: true

        function focusList() {
            listView.forceActiveFocus()
            listView.currentIndex = rowCount > 0 ? Math.max(0, Math.min(currentIndex, rowCount - 1)) : -1
            ensureVisible()
        }

        function ensureVisible() {
            if (listView.currentIndex >= 0)
                listView.positionViewAtIndex(listView.currentIndex, ListView.Contain)
        }

        function currentCard() {
            return listView.currentItem
        }

        function handlePressedKey(key) {
            const card = currentCard()
            return card && card.handleAcceptPressed ? card.handleAcceptPressed(key) : false
        }

        function handleNavigationKey(key) {
            if (rowCount <= 0)
                return false
            const acceptKey = key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select || key === Qt.Key_Space
            const card = currentCard()
            if (!acceptKey && card && card.handleNavigationKey && card.handleNavigationKey(key))
                return true
            if (key === Qt.Key_Left) {
                if (listView.currentIndex <= 0) shell.focusRail()
                else listView.currentIndex = listView.currentIndex - 1
                currentIndex = listView.currentIndex
                ensureVisible()
                return true
            }
            if (key === Qt.Key_Right) {
                listView.currentIndex = Math.min(rowCount - 1, listView.currentIndex + 1)
                currentIndex = listView.currentIndex
                ensureVisible()
                return true
            }
            if (acceptKey) {
                currentIndex = listView.currentIndex
                if (card && card.handleAcceptReleased && card.handleAcceptReleased(key))
                    return true
                if (card && card.handleNavigationKey && card.handleNavigationKey(key))
                    return true
                activated(listView.currentIndex)
                return true
            }
            return false
        }

        SectionHeader {
            id: rowHeader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: rowRoot.headerHeight
            title: rowRoot.title
        }

        ListView {
            id: listView
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: rowHeader.bottom
            anchors.topMargin: 10
            height: rowRoot.cardHeight
            focus: true
            keyNavigationEnabled: false
            clip: true
            orientation: ListView.Horizontal
            boundsBehavior: Flickable.StopAtBounds
            spacing: root.rowGap
            model: rowRoot.rowModel
            currentIndex: rowRoot.rowCount > 0 ? Math.max(0, Math.min(rowRoot.currentIndex, rowRoot.rowCount - 1)) : -1

            delegate: MediaItemCard {
                id: posterDelegate
                required property int index
                required property string title
                required property string posterUrl
                required property string seriesPosterUrl
                required property int year
                required property string subtitle
                required property string displayTitle
                required property string displaySubtitle
                required property string movieId
                readonly property var itemData: rowRoot.rowModel.get(index)
                width: rowRoot.cardWidth
                height: listView.height
                item: itemData
                kind: "poster"
                useSeriesPoster: true
                focused: posterDelegate.index === listView.currentIndex && listView.activeFocus
                onActivated: {
                    listView.currentIndex = posterDelegate.index
                    rowRoot.currentIndex = posterDelegate.index
                    rowRoot.activated(posterDelegate.index)
                }
                onFavoriteToggled: (favorite) => appController.setFavorite(posterDelegate.movieId || "", favorite)
                onPlayedToggled: (played) => appController.setPlayed(posterDelegate.movieId || "", played)
                onMediaInfoRequested: shell.openMediaInfo(posterDelegate.itemData)
            }

            Keys.onReleased: (event) => {
                if (rowRoot.handleNavigationKey(event.key))
                    event.accepted = true
            }
        }
    }

    component PeopleRow: FocusScope {
        id: peopleRoot
        property string title: "Cast & Crew"
        property var peopleModel: []
        property int currentIndex: 0
        readonly property int rowCount: peopleModel ? peopleModel.length : 0
        readonly property int cardWidth: Math.min(156, Math.max(124, root.width * 0.084))
        readonly property int cardHeight: Math.round(cardWidth * 1.18 + 48)
        readonly property int headerHeight: 34
        readonly property int rowHeight: rowCount > 0 ? headerHeight + 10 + cardHeight : 0
        signal activated(var person)

        Layout.fillWidth: true
        Layout.preferredHeight: visible ? rowHeight : 0
        Layout.minimumHeight: visible ? rowHeight : 0
        Layout.maximumHeight: visible ? rowHeight : 0
        implicitHeight: visible ? rowHeight : 0
        height: visible ? rowHeight : 0
        visible: rowCount > 0
        focus: true

        function focusList() {
            peopleList.forceActiveFocus()
            peopleList.currentIndex = rowCount > 0 ? Math.max(0, Math.min(currentIndex, rowCount - 1)) : -1
            ensureVisible()
        }

        function ensureVisible() {
            if (peopleList.currentIndex >= 0)
                peopleList.positionViewAtIndex(peopleList.currentIndex, ListView.Contain)
        }

        function handlePressedKey(key) {
            return false
        }

        function handleNavigationKey(key) {
            if (rowCount <= 0)
                return false
            if (key === Qt.Key_Left) {
                if (peopleList.currentIndex <= 0) shell.focusRail()
                else peopleList.currentIndex = peopleList.currentIndex - 1
                currentIndex = peopleList.currentIndex
                ensureVisible()
                return true
            }
            if (key === Qt.Key_Right) {
                peopleList.currentIndex = Math.min(rowCount - 1, peopleList.currentIndex + 1)
                currentIndex = peopleList.currentIndex
                ensureVisible()
                return true
            }
            if (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select || key === Qt.Key_Space) {
                currentIndex = peopleList.currentIndex
                activated(peopleModel[peopleList.currentIndex] || ({}))
                return true
            }
            return false
        }

        SectionHeader {
            id: peopleHeader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: peopleRoot.headerHeight
            title: peopleRoot.title
        }

        ListView {
            id: peopleList
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: peopleHeader.bottom
            anchors.topMargin: 10
            height: peopleRoot.cardHeight
            focus: true
            keyNavigationEnabled: false
            clip: true
            orientation: ListView.Horizontal
            boundsBehavior: Flickable.StopAtBounds
            spacing: root.rowGap
            model: peopleRoot.peopleModel
            currentIndex: peopleRoot.rowCount > 0 ? Math.max(0, Math.min(peopleRoot.currentIndex, peopleRoot.rowCount - 1)) : -1

            delegate: Item {
                id: personDelegate
                required property int index
                required property var modelData
                width: peopleRoot.cardWidth
                height: peopleList.height

                ImageCard {
                    id: personImage
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: width * 1.18
                    imageUrl: modelData.imageUrl || ""
                    fallbackText: modelData.type || "Person"
                    focused: personDelegate.index === peopleList.currentIndex && peopleList.activeFocus
                    retainWhileLoading: true
                }

                AppText {
                    id: personName
                    anchors.top: personImage.bottom
                    anchors.topMargin: 8
                    anchors.left: parent.left
                    anchors.right: parent.right
                    text: modelData.name || ""
                    font.pixelSize: Metrics.metaPx(root.width) + 1
                    font.weight: Font.Medium
                    color: personDelegate.index === peopleList.currentIndex && peopleList.activeFocus ? Theme.textPrimary : Theme.textSecondary
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }

                MonoText {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: personName.bottom
                    anchors.topMargin: 2
                    text: modelData.role || modelData.type || ""
                    color: Theme.textMuted
                    font.pixelSize: Metrics.metaPx(root.width) - 1
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        peopleList.currentIndex = personDelegate.index
                        peopleRoot.currentIndex = personDelegate.index
                        peopleRoot.activated(personDelegate.modelData)
                    }
                }
            }

            Keys.onReleased: (event) => {
                if (peopleRoot.handleNavigationKey(event.key))
                    event.accepted = true
            }
        }
    }

    Component.onCompleted: {
        syncUserState()
        updateDetailCounts()
        Qt.callLater(refreshDetailRows)
        Qt.callLater(focusDefaultAction)
    }

    onActiveFocusChanged: if (activeFocus) focusDefaultAction()
    onItemChanged: {
        overflowOpen = false
        syncUserState()
        Qt.callLater(refreshDetailRows)
    }

    Connections {
        target: appController
        function onDetailRowsChanged() { root.updateDetailCounts() }
        function onItemFavoriteChanged(itemId, favorite) {
            if ((root.item.movieId || "") === itemId)
                root.favoriteState = favorite
        }
        function onItemPlayedChanged(itemId, played) {
            if ((root.item.movieId || "") === itemId)
                root.playedState = played
        }
    }

    Connections {
        target: appController ? appController.detailSeasons : null
        function onModelReset() { root.updateDetailCounts() }
        function onRowsInserted() { root.updateDetailCounts() }
        function onRowsRemoved() { root.updateDetailCounts() }
    }

    Connections {
        target: appController ? appController.detailSimilarItems : null
        function onModelReset() { root.updateDetailCounts() }
        function onRowsInserted() { root.updateDetailCounts() }
        function onRowsRemoved() { root.updateDetailCounts() }
    }

    Keys.onReleased: (event) => {
        if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape || event.key === Qt.Key_Backspace) {
            shell.back()
            event.accepted = true
        }
    }

    function updateDetailCounts() {
        seasonCount = appController && appController.detailSeasons ? appController.detailSeasons.rowCount() : 0
        similarCount = appController && appController.detailSimilarItems ? appController.detailSimilarItems.rowCount() : 0
        seasonsRow.currentIndex = seasonCount > 0 ? Math.max(0, Math.min(seasonsRow.currentIndex, seasonCount - 1)) : 0
        similarRow.currentIndex = similarCount > 0 ? Math.max(0, Math.min(similarRow.currentIndex, similarCount - 1)) : 0
    }

    function refreshDetailRows() {
        const itemId = item.movieId || ""
        const key = itemId + ":" + typeText
        if (key === loadedDetailKey)
            return
        loadedDetailKey = key
        seasonsRow.currentIndex = 0
        similarRow.currentIndex = 0
        if (appController)
            appController.loadDetailRows(itemId, typeText)
    }

    function syncUserState() {
        favoriteState = Boolean(item.favorite)
        playedState = Boolean(item.played)
    }

    function focusDefaultAction() {
        if (showPrimaryAction && primaryAction.enabledButton)
            primaryAction.forceActiveFocus()
        else
            playedAction.forceActiveFocus()
    }

    function orderedActions() {
        const actions = []
        if (showPrimaryAction)
            actions.push(primaryAction)
        actions.push(playedAction)
        actions.push(favoriteAction)
        actions.push(menuAction)
        return actions
    }

    function focusedActionIndex() {
        const actions = orderedActions()
        for (let i = 0; i < actions.length; ++i) {
            if (actions[i].activeFocus)
                return i
        }
        return -1
    }

    function focusActionIndex(index) {
        const actions = orderedActions()
        if (actions.length === 0)
            return false
        actions[Math.max(0, Math.min(index, actions.length - 1))].forceActiveFocus()
        return true
    }

    function focusNextAction(delta) {
        const actions = orderedActions()
        const current = focusedActionIndex()
        if (current < 0)
            return focusActionIndex(0)
        const next = current + delta
        if (next < 0)
            return false
        if (next >= actions.length)
            return true
        actions[next].forceActiveFocus()
        return true
    }

    function focusFirstContentRow() {
        if (showSeasonsRow) {
            seasonsRow.focusList()
            return true
        }
        if (showPeopleRow) {
            peopleRow.focusList()
            return true
        }
        if (showSimilarRow) {
            similarRow.focusList()
            return true
        }
        return false
    }

    function activatePrimary() {
        if (selectedIndex < 0 || !canPlay)
            return
        if (detailSource === "search") {
            appController.playSearchResult(selectedIndex)
        } else if (detailSource === "resume") {
            appController.playResumeItem(selectedIndex)
        } else if (detailSource === "nextup") {
            appController.playNextUpItem(selectedIndex)
        } else if (detailSource === "latest") {
            appController.playLatestItem(selectedIndex)
        } else if (detailSource.indexOf("latestLibrary:") === 0) {
            appController.playLatestLibraryItem(parseInt(detailSource.split(":")[1], 10), selectedIndex)
        } else if (detailSource === "person") {
            appController.playPersonItem(selectedIndex)
        } else if (detailSource === "similar") {
            appController.playDetailSimilarItem(selectedIndex)
        } else {
            appController.playMovie(selectedIndex)
        }
    }

    function openMediaInfo() {
        overflowOpen = false
        if (shell)
            shell.openMediaInfo(item)
    }

    function toggleFavorite() {
        if (!item.movieId || !appController)
            return
        favoriteState = !favoriteState
        appController.setFavorite(item.movieId, favoriteState)
    }

    function togglePlayed() {
        if (!item.movieId || !appController)
            return
        playedState = !playedState
        appController.setPlayed(item.movieId, playedState)
    }

    function toggleOverflow() {
        overflowOpen = !overflowOpen
        if (overflowOpen)
            Qt.callLater(function() { mediaInfoOption.forceActiveFocus() })
        else
            menuAction.forceActiveFocus()
    }

    function openSimilarItem(index) {
        if (index < 0 || !appController)
            return
        shell.openDetails(appController.detailSimilarItems, index, "similar", shell.detailsReturnRoute || "libraryGrid")
    }

    function openPerson(person) {
        if (!person || !person.personId || !shell)
            return
        shell.openPerson(person)
    }

    function handlePressedKey(key) {
        if (seasonsRow.activeFocus)
            return seasonsRow.handlePressedKey(key)
        if (similarRow.activeFocus)
            return similarRow.handlePressedKey(key)
        return false
    }

    function primaryLabel() {
        return item.playActionLabel || "Play"
    }

    function runtimeText(ticks) {
        ticks = Number(ticks || 0)
        if (ticks <= 0)
            return ""
        const minutes = Math.round(ticks / 600000000)
        const hours = Math.floor(minutes / 60)
        const mins = minutes % 60
        return hours > 0 ? hours + "h " + mins + "m" : mins + "m"
    }

    function progressText() {
        if (!item.resumeTicks || item.resumeTicks <= 0)
            return ""
        const watched = runtimeText(item.resumeTicks)
        const total = runtimeText(item.runtimeTicks)
        return total.length > 0 ? watched + " watched of " + total : watched + " watched"
    }

    function listText(value, maxItems) {
        if (!value || value.length === 0)
            return ""
        const result = []
        const count = Math.min(value.length, maxItems || value.length)
        for (let i = 0; i < count; ++i) {
            const text = String(value[i] || "")
            if (text.length > 0)
                result.push(text)
        }
        if (value.length > count)
            result.push("+" + (value.length - count))
        return result.join(", ")
    }

    function yearFromDate(value) {
        if (!value || value.length < 4)
            return 0
        const parsed = parseInt(String(value).slice(0, 4), 10)
        return isNaN(parsed) ? 0 : parsed
    }

    function yearRange() {
        const start = Number(item.year || 0) > 0 ? Number(item.year) : yearFromDate(item.premiereDate)
        const end = yearFromDate(item.endDate)
        if (typeText === "Series" && start > 0 && end > 0 && end !== start)
            return start + " - " + end
        return start > 0 ? String(start) : ""
    }

    function ratingText() {
        const parts = []
        if (item.officialRating && item.officialRating.length > 0)
            parts.push(item.officialRating)
        const community = Number(item.communityRating || 0)
        if (community > 0)
            parts.push(community.toFixed(1))
        return parts.join(" / ")
    }

    function metadataParts() {
        const parts = []
        const years = yearRange()
        if (years.length > 0) parts.push(years)
        const rating = ratingText()
        if (rating.length > 0) parts.push(rating)
        const runtime = runtimeText(item.runtimeTicks)
        if (runtime.length > 0) parts.push(runtime)
        if (typeText.length > 0) parts.push(typeText)
        if (subtitleText.length > 0 && subtitleText !== typeText && subtitleText !== years) parts.push(subtitleText)
        return parts
    }

    function metadataLine() {
        return metadataParts().join(" / ")
    }

    function handleNavigationKey(key) {
        if (mediaInfoOption.activeFocus) {
            if (key === Qt.Key_Up || key === Qt.Key_Left || key === Qt.Key_Right) {
                overflowOpen = false
                menuAction.forceActiveFocus()
                return true
            }
            if (key === Qt.Key_Down) {
                if (focusFirstContentRow())
                    overflowOpen = false
                return true
            }
            if (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select) {
                openMediaInfo()
                return true
            }
        }

        if (seasonsRow.activeFocus) {
            if (key === Qt.Key_Up) {
                focusDefaultAction()
                return true
            }
            if (key === Qt.Key_Down) {
                if (showPeopleRow) peopleRow.focusList()
                else if (showSimilarRow)
                    similarRow.focusList()
                return true
            }
            return seasonsRow.handleNavigationKey(key)
        }

        if (peopleRow.activeFocus) {
            if (key === Qt.Key_Up) {
                if (showSeasonsRow) seasonsRow.focusList()
                else focusDefaultAction()
                return true
            }
            if (key === Qt.Key_Down) {
                if (showSimilarRow)
                    similarRow.focusList()
                return true
            }
            return peopleRow.handleNavigationKey(key)
        }

        if (similarRow.activeFocus) {
            if (key === Qt.Key_Up) {
                if (showPeopleRow) peopleRow.focusList()
                else if (showSeasonsRow) seasonsRow.focusList()
                else focusDefaultAction()
                return true
            }
            if (key === Qt.Key_Down)
                return true
            return similarRow.handleNavigationKey(key)
        }

        if (key === Qt.Key_Left) {
            if (!focusNextAction(-1))
                shell.focusRail()
            return true
        }
        if (key === Qt.Key_Right) {
            focusNextAction(1)
            return true
        }
        if (key === Qt.Key_Down) {
            if (overflowOpen && menuAction.activeFocus) {
                mediaInfoOption.forceActiveFocus()
                return true
            }
            focusFirstContentRow()
            return true
        }
        if (key === Qt.Key_Up) {
            focusDefaultAction()
            return true
        }
        if (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select) {
            if (playedAction.activeFocus) togglePlayed()
            else if (favoriteAction.activeFocus) toggleFavorite()
            else if (menuAction.activeFocus) toggleOverflow()
            else activatePrimary()
            return true
        }
        return false
    }

    Rectangle { anchors.fill: parent; color: Theme.bg }

    Image {
        anchors.fill: parent
        source: root.backgroundArt
        fillMode: Image.PreserveAspectCrop
        opacity: status === Image.Ready ? 0.42 : 0
        asynchronous: true
        cache: true
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#A0000000" }
            GradientStop { position: 0.42; color: "#D20D0D0D" }
            GradientStop { position: 1.0; color: Theme.bg }
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#5A000000" }
            GradientStop { position: 0.54; color: "#22000000" }
            GradientStop { position: 1.0; color: "#C4000000" }
        }
    }

    Flickable {
        id: detailsFlick
        anchors.fill: parent
        contentWidth: width
        contentHeight: Math.max(height, contentColumn.implicitHeight + root.contentMargin)
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: contentColumn
            width: detailsFlick.width
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(500, Math.min(660, Math.round(root.height * 0.64)))

                Image {
                    id: titleArtImage
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: Math.round(root.height * 0.035)
                    width: Math.min(520, Math.max(280, parent.width * 0.32))
                    height: 116
                    source: root.titleArt
                    visible: root.titleArt.length > 0
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    cache: true
                    smooth: true
                }

                RowLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: titleArtImage.visible ? titleArtImage.bottom : parent.top
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: root.contentMargin
                    anchors.rightMargin: root.contentMargin
                    anchors.topMargin: titleArtImage.visible ? 22 : Math.round(root.height * 0.08)
                    anchors.bottomMargin: 24
                    spacing: Math.round(root.width * 0.032)

                    ImageCard {
                        id: posterFrame
                        Layout.preferredWidth: root.posterWidth
                        Layout.preferredHeight: root.posterWidth * 1.5
                        Layout.maximumHeight: parent.height
                        Layout.alignment: Qt.AlignTop
                        imageUrl: root.item.posterUrl || root.item.thumbUrl || ""
                        fallbackText: root.typeText
                        focused: false
                        retainWhileLoading: true
                    }

                    Rectangle {
                        id: infoPanel
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredHeight: Math.min(parent.height, infoColumn.implicitHeight + 44)
                        radius: Theme.radiusMedium
                        color: "#D4111111"
                        border.width: 1
                        border.color: "#33FFFFFF"
                        antialiasing: true

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            height: 2
                            color: Theme.accent
                            opacity: 0.85
                        }

                        ColumnLayout {
                            id: infoColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 22
                            spacing: 12

                            AppText {
                                Layout.fillWidth: true
                                visible: root.parentText.length > 0
                                text: root.parentText
                                color: Theme.textSecondary
                                font.pixelSize: Metrics.bodyPx(root.width)
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                                maximumLineCount: 1
                            }

                            AppText {
                                Layout.fillWidth: true
                                text: root.titleText
                                font.pixelSize: Math.min(48, Metrics.titlePx(root.width) + 8)
                                font.weight: Font.DemiBold
                                wrapMode: Text.Wrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                            }

                            TechMetadataLine {
                                Layout.fillWidth: true
                                visible: root.metadataLine().length > 0
                                metadata: root.metadataLine()
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                visible: root.hasProgress
                                spacing: 6

                                AppText {
                                    Layout.fillWidth: true
                                    text: root.progressText()
                                    color: Theme.textSecondary
                                    font.pixelSize: Metrics.metaPx(root.width)
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 5
                                    radius: 2
                                    color: Theme.border
                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        width: parent.width * Math.max(0, Math.min(1, root.item.progress || 0))
                                        radius: 2
                                        color: Theme.accent
                                    }
                                }
                            }

                            Row {
                                id: actionRow
                                spacing: 10
                                topPadding: 3

                                DetailAction {
                                    id: primaryAction
                                    iconName: "play_arrow"
                                    label: root.primaryLabel()
                                    primary: true
                                    visible: root.showPrimaryAction
                                    enabledButton: root.showPrimaryAction
                                    onActivated: root.activatePrimary()
                                }

                                IconAction {
                                    id: playedAction
                                    iconName: root.playedState ? "check_circle" : "radio_button_unchecked"
                                    label: root.playedState ? "Mark unplayed" : "Mark played"
                                    checked: root.playedState
                                    enabledButton: root.selectedIndex >= 0
                                    onActivated: root.togglePlayed()
                                }

                                IconAction {
                                    id: favoriteAction
                                    iconName: root.favoriteState ? "favorite" : "favorite_border"
                                    label: root.favoriteState ? "Remove favourite" : "Add favourite"
                                    checked: root.favoriteState
                                    enabledButton: root.selectedIndex >= 0
                                    onActivated: root.toggleFavorite()
                                }

                                IconAction {
                                    id: menuAction
                                    iconName: "menu"
                                    label: "More"
                                    checked: root.overflowOpen
                                    enabledButton: root.selectedIndex >= 0
                                    onActivated: root.toggleOverflow()
                                }
                            }

                            Rectangle {
                                id: overflowMenu
                                Layout.preferredWidth: 292
                                Layout.preferredHeight: root.overflowOpen ? menuColumn.implicitHeight + 14 : 0
                                Layout.alignment: Qt.AlignLeft
                                visible: root.overflowOpen
                                radius: Theme.radiusMedium
                                color: "#F01A1A1A"
                                border.width: 1
                                border.color: Theme.borderStrong
                                clip: true

                                Column {
                                    id: menuColumn
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 7

                                    MenuOption {
                                        id: mediaInfoOption
                                        iconName: "info"
                                        label: "Media info"
                                        onActivated: root.openMediaInfo()
                                    }
                                }
                            }

                            AppText {
                                Layout.fillWidth: true
                                visible: root.item.overview && root.item.overview.length > 0
                                text: root.item.overview || ""
                                color: Theme.textSecondary
                                wrapMode: Text.Wrap
                                font.pixelSize: Metrics.bodyPx(root.width)
                                lineHeight: 1.13
                                maximumLineCount: 5
                                elide: Text.ElideRight
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 5

                                InfoLine { label: "Genres"; value: root.listText(root.item.genres, 5) }
                                InfoLine { label: "Studio"; value: root.listText(root.item.studios, 3) }
                                InfoLine { label: "Tags"; value: root.listText(root.item.tags, 8) }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: root.contentMargin
                Layout.rightMargin: root.contentMargin
                Layout.topMargin: 18
                spacing: 28

                DetailPosterRow {
                    id: seasonsRow
                    title: "Seasons"
                    rowModel: appController ? appController.detailSeasons : null
                    cardWidth: root.rowPosterWidth
                    visible: root.showSeasonsRow
                    onActivated: (index) => appController.openDetailSeason(index)
                }

                PeopleRow {
                    id: peopleRow
                    title: "Cast & Crew"
                    peopleModel: root.people
                    visible: root.showPeopleRow
                    onActivated: (person) => root.openPerson(person)
                }

                DetailPosterRow {
                    id: similarRow
                    title: "More Like This"
                    rowModel: appController ? appController.detailSimilarItems : null
                    cardWidth: root.rowPosterWidth
                    visible: root.showSimilarRow
                    onActivated: (index) => root.openSimilarItem(index)
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.contentMargin
                }
            }
        }
    }
}
