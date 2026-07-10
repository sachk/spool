import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root

    property var shell
    readonly property var itemModel: shell && shell.detailsModel ? shell.detailsModel : Browse.items
    readonly property int itemCount: itemModel && itemModel.rowCount ? itemModel.rowCount() : 0
    readonly property int selectedIndex: itemCount > 0 && shell ? shell.detailsIndexForModel(itemModel) : -1
    readonly property var item: selectedIndex >= 0 && itemModel ? itemModel.get(selectedIndex) : ({})
    readonly property string detailSource: shell ? shell.detailsSource : "movies"
    readonly property string typeText: item.itemType || "Media"
    readonly property string titleText: typeText === "Episode" && item.title ? item.title : (item.displayTitle
                                                                                             || item.title
                                                                                             || item.seriesName
                                                                                             || "Selected item")
    readonly property string seriesTitle: item.seriesName || (typeText === "Series" ? titleText : "")
    readonly property string seasonTitleText: seasonTitle()
    readonly property string seriesIdText: item.seriesId || ""
    readonly property string seasonIdText: item.seasonId || ""
    readonly property bool canPlay: item.playable === undefined || item.playable
    readonly property bool showPrimaryAction: selectedIndex >= 0 && canPlay
    readonly property bool hasProgress: Number(item.resumeTicks || 0) > 0 && Number(item.runtimeTicks || 0) > 0
    readonly property int detailTitlePx: Math.min(68, Metrics.titlePx(width) + 24)
    readonly property int contentMargin: Metrics.pageMargin(width)
    readonly property int rowPosterWidth: Metrics.detailRowPosterWidth(width)
    readonly property int rowLandscapeWidth: Math.round(rowPosterWidth * 1.75)
    readonly property int rowGap: Math.max(14, Metrics.gap(width))
    readonly property string backgroundArt: item.backdropUrl || item.thumbUrl || item.landscapeCardUrl || item.posterUrl
                                            || ""
    readonly property string stillArt: item.landscapeCardUrl || item.thumbUrl || item.backdropUrl || item.posterUrl
                                       || item.seriesPosterUrl || ""
    readonly property bool showSideArt: width >= 1120 && stillArt.length > 0
    readonly property real copyWidth: showSideArt ? Math.min(width * 0.56, 940) : width - contentMargin * 2
    readonly property bool loadingDetailRows: Content.detailRowsBusy
    readonly property int contextCount: Content.detailSeasons ? Content.detailSeasons.count : 0
    readonly property int similarCount: Content.detailSimilarItems ? Content.detailSimilarItems.count : 0
    readonly property bool contextPosterCards: typeText === "Series" || typeText === "BoxSet"
    readonly property bool contextItemsPossible: contextPosterCards || ((typeText === "Episode" || typeText
                                                                         === "Season") && seriesIdText.length > 0)
    readonly property bool reserveContextRow: contextItemsPossible && contextCount === 0 && loadingDetailRows
    readonly property bool showContextPlaybackActions: contextCount > 0 && typeText !== "Series"
    readonly property bool showContextRow: contextCount > 0 || reserveContextRow
    readonly property bool showSimilarRow: similarCount > 0
    readonly property var fullDetailItem: Content.detailItem && String(Content.detailItem.movieId || "") === String(
                                              item.movieId || "") ? Content.detailItem : ({})
    readonly property var metadataPeople: fullDetailItem.people && fullDetailItem.people.length > 0
                                          ? fullDetailItem.people : (item.people || [])
    readonly property var people: metadataPeople
    readonly property bool showPeopleRow: people.length > 0
    readonly property var genreList: fullDetailItem.genres && fullDetailItem.genres.length > 0 ? fullDetailItem.genres :
                                                                                                 (item.genres || [])
    readonly property var studioList: fullDetailItem.studios && fullDetailItem.studios.length > 0
                                      ? fullDetailItem.studios : (item.studios || [])
    readonly property var metadataRows: buildMetadataRows()
    readonly property bool showMetadataPanel: metadataRows.length > 0
    readonly property bool showSeriesLink: (typeText === "Episode" || typeText === "Season") && seriesIdText.length > 0
                                           && seriesTitle.length > 0
    readonly property bool showSeasonLink: typeText === "Episode" && seriesIdText.length > 0 && (seasonIdText.length
                                                                                                 > 0 || Number(
                                                                                                     item.seasonNumber
                                                                                                     || 0) > 0)

    property bool favoriteState: false
    property bool playedState: false
    property bool overflowOpen: false
    property string focusZone: "actions"
    property int actionIndex: 0
    property int overflowIndex: 0
    property string loadedDetailKey: ""

    focus: true

    component DetailAction: ActionButton {
        property string label: ""
        property bool primary: false
        property bool enabledButton: true
        signal activated

        width: primary ? Math.min(Math.max(implicitWidth, 190), 320) : Math.min(Math.max(implicitWidth, 132), 230)
        text: label
        kind: primary ? "primary" : "secondary"
        enabled: enabledButton
        onClicked: activated()
    }

    component IconAction: IconButton {
        property string label: ""
        property bool enabledButton: true
        signal activated

        width: Metrics.controlHeight(root.width)
        height: width
        selected: checked
        enabled: enabledButton
        accessibleName: label
        onClicked: activated()
    }

    component MenuOption: ActionButton {
        property string label: ""
        signal activated

        width: parent ? parent.width : 280
        text: label
        kind: "flat"
        onClicked: activated()
    }

    component DetailLink: FocusScope {
        id: link
        property string label: ""
        signal activated

        visible: label.length > 0
        implicitWidth: linkColumn.implicitWidth
        implicitHeight: linkColumn.implicitHeight
        focus: true

        Column {
            id: linkColumn
            anchors.left: parent.left
            anchors.top: parent.top
            spacing: 2

            Row {
                id: linkRow
                spacing: 8

                AppText {
                    id: linkText
                    anchors.verticalCenter: parent.verticalCenter
                    text: link.label
                    color: link.activeFocus || hover.hovered ? Theme.textPrimary : Theme.textSecondary
                    font.pixelSize: root.detailTitlePx
                    font.weight: Font.DemiBold
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }

                MaterialIcon {
                    anchors.verticalCenter: linkText.verticalCenter
                    name: "chevron_right"
                    iconSize: Math.max(20, Math.round(root.detailTitlePx * 0.45))
                    iconColor: link.activeFocus || hover.hovered ? Theme.accent : Theme.textMuted
                }
            }

            Rectangle {
                width: linkText.width
                height: link.activeFocus ? 3 : 2
                radius: height / 2
                color: link.activeFocus || hover.hovered ? Theme.accent : Theme.borderStrong
                opacity: link.activeFocus || hover.hovered ? 1 : 0.75
            }
        }

        HoverHandler {
            id: hover
        }
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onClicked: link.activated()
        }
    }

    component MetadataPanel: FocusScope {
        id: panel
        property var rows: []
        property int currentRow: 0
        property int currentChip: 0
        property bool browsing: false
        readonly property bool hasRows: rows && rows.length > 0
        signal activated(string kind, var value)
        signal leaveUp
        signal leaveDown

        function focusPanel() {
            browsing = false
            InputKeys.focus(panel)
        }

        function chipText(value) {
            if (value === undefined || value === null)
                return ""
            if (typeof value === "string")
                return value
            return String(value.name || value.title || value.value || "")
        }

        function rowValues(row) {
            if (!hasRows || row < 0 || row >= rows.length)
                return []
            return rows[row].values || []
        }

        function normalizeSelection() {
            if (!hasRows) {
                currentRow = 0
                currentChip = 0
                browsing = false
                return
            }
            currentRow = Math.max(0, Math.min(currentRow, rows.length - 1))
            currentChip = Math.max(0, Math.min(currentChip, Math.max(0, rowValues(currentRow).length - 1)))
        }

        function moveRow(delta) {
            const next = currentRow + delta
            if (next < 0) {
                browsing = false
                leaveUp()
                return true
            }
            if (next >= rows.length) {
                browsing = false
                leaveDown()
                return true
            }
            currentRow = next
            currentChip = Math.min(currentChip, Math.max(0, rowValues(currentRow).length - 1))
            return true
        }

        function moveChip(delta) {
            const values = rowValues(currentRow)
            if (values.length <= 0)
                return true
            currentChip = Math.max(0, Math.min(currentChip + delta, values.length - 1))
            return true
        }

        function activateCurrent() {
            normalizeSelection()
            const values = rowValues(currentRow)
            if (values.length <= 0)
                return false
            activated(rows[currentRow].kind || "", values[currentChip])
            return true
        }

        function leaveBrowse() {
            browsing = false
            InputKeys.focus(panel)
        }

        function routeKey(key, phase, repeat) {
            if (!hasRows)
                return false
            normalizeSelection()
            if (!browsing) {
                if (key === Qt.Key_Up) {
                    leaveUp()
                    return true
                }
                if (key === Qt.Key_Down) {
                    leaveDown()
                    return true
                }
                return InputKeys.isHorizontal(key)
            }
            if (key === Qt.Key_Left)
                return moveChip(-1)
            if (key === Qt.Key_Right)
                return moveChip(1)
            if (key === Qt.Key_Up)
                return moveRow(-1)
            if (key === Qt.Key_Down)
                return moveRow(1)
            return false
        }

        function activate() {
            if (!browsing)
                browsing = true
            else
                activateCurrent()
        }

        function back() {
            if (!browsing)
                return false
            leaveBrowse()
            return true
        }

        width: parent ? parent.width : implicitWidth
        height: implicitHeight
        implicitHeight: visible ? block.implicitHeight : 0
        visible: hasRows
        focus: true
        onRowsChanged: normalizeSelection()

        Surface {
            id: block
            width: parent.width
            implicitHeight: metadataColumn.implicitHeight + 34
            height: implicitHeight
            focused: panel.activeFocus
            hovered: panelHover.hovered
            baseColor: Theme.floatingPanel
            radius: Theme.radiusPanel

            ColumnLayout {
                id: metadataColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 17
                spacing: 12

                Repeater {
                    model: panel.rows
                    delegate: RowLayout {
                        id: rowDelegate
                        required property int index
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 16

                        MonoText {
                            Layout.preferredWidth: Math.min(128, Math.max(88, block.width * 0.12))
                            text: String(rowDelegate.modelData.label || "")
                            color: panel.browsing && panel.currentRow === rowDelegate.index ? Theme.textPrimary :
                                                                                              Theme.textMuted

                            font.pixelSize: Metrics.metaPx(root.width)
                            font.weight: Font.DemiBold
                            font.letterSpacing: 1.7
                            maximumLineCount: 1
                            elide: Text.ElideRight
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 8

                            Repeater {
                                model: rowDelegate.modelData.values || []
                                delegate: MetadataChip {
                                    id: chip
                                    required property int index
                                    required property var modelData
                                    text: panel.chipText(modelData)
                                    selected: panel.browsing && panel.currentRow === rowDelegate.index
                                              && panel.currentChip === index
                                    hovered: chipMouse.containsMouse

                                    MouseArea {
                                        id: chipMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: panel.activated(rowDelegate.modelData.kind || "", modelData)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        HoverHandler {
            id: panelHover
        }
    }

    Component.onCompleted: {
        syncUserState()
        updateDetailCounts()
        Qt.callLater(refreshDetailRows)
        Qt.callLater(refreshItemDetail)
        Qt.callLater(focusDefaultAction)
    }

    onActiveFocusChanged: if (activeFocus)
                              focusDefaultAction()
    onItemChanged: {
        overflowOpen = false
        syncUserState()
        Qt.callLater(refreshDetailRows)
        Qt.callLater(refreshItemDetail)
    }

    Connections {
        target: Content
        function onDetailRowsChanged() {
            root.updateDetailCounts()
        }
    }

    Connections {
        target: App
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
        target: Content.detailSeasons
        function onModelReset() {
            root.updateDetailCounts()
        }
        function onRowsInserted() {
            root.updateDetailCounts()
        }
        function onRowsRemoved() {
            root.updateDetailCounts()
        }
    }

    Connections {
        target: Content.detailSimilarItems
        function onModelReset() {
            root.updateDetailCounts()
        }
        function onRowsInserted() {
            root.updateDetailCounts()
        }
        function onRowsRemoved() {
            root.updateDetailCounts()
        }
    }

    function back() {
        if (focusZone === "metadata" && metadataPanel.back())
            return true
        if (overflowOpen) {
            overflowOpen = false
            focusActionIndex(orderedActions().indexOf(menuAction))
            return true
        }
        return false
    }

    function updateDetailCounts() {
        contextRow.currentIndex = contextCount > 0 ? Math.max(0, Math.min(contextRow.currentIndex, contextCount - 1)) :
                                                     0
        similarRow.currentIndex = similarCount > 0 ? Math.max(0, Math.min(similarRow.currentIndex, similarCount - 1)) :
                                                     0
    }

    function refreshDetailRows() {
        const itemId = item.movieId || ""
        const key = itemId + ":" + typeText + ":" + seriesIdText + ":" + seasonIdText
        if (key === loadedDetailKey)
            return
        loadedDetailKey = key
        contextRow.currentIndex = 0
        similarRow.currentIndex = 0
        Content.loadDetailRows(itemId, typeText, seriesIdText, seasonIdText)
    }

    function refreshItemDetail() {
        const itemId = item.movieId || ""
        if (itemId.length > 0)
            Content.loadItemDetail(itemId)
    }

    function syncUserState() {
        favoriteState = Boolean(item.favorite)
        playedState = Boolean(item.played)
    }

    function focusDefaultAction() {
        if (detailsFlick)
            detailsFlick.contentY = 0
        focusActionRow()
    }

    function focusActionRow() {
        focusActionIndex(0)
        ensureDetailsItemVisible(actionRow)
    }

    function focusHeaderAboveActions() {
        if (seasonLink.visible) {
            focusZone = "season"
            InputKeys.focus(seasonLink)
            ensureDetailsItemVisible(seasonLink)
        } else if (seriesLink.visible) {
            focusZone = "series"
            InputKeys.focus(seriesLink)
            ensureDetailsItemVisible(seriesLink)
        } else {
            if (detailsFlick)
                detailsFlick.contentY = 0
            if (shell)
                shell.focusNavBar()
        }
        return true
    }

    function focusDetailsPanel() {
        if (!showMetadataPanel)
            return focusFirstMediaRow()
        focusZone = "metadata"
        metadataPanel.focusPanel()
        ensureDetailsItemVisible(metadataPanel)
        return true
    }

    function focusBeforeMediaRows() {
        return showMetadataPanel ? focusDetailsPanel() : focusActionRow()
    }

    function focusFirstMediaRow() {
        if (showContextRow && contextCount > 0) {
            focusZone = "context"
            contextRow.focusList()
            ensureDetailsItemVisible(contextRow)
            return true
        }
        if (showPeopleRow) {
            focusZone = "people"
            peopleRow.focusList()
            ensureDetailsItemVisible(peopleRow)
            return true
        }
        if (showSimilarRow) {
            focusZone = "similar"
            similarRow.focusList()
            ensureDetailsItemVisible(similarRow)
            return true
        }
        return false
    }

    function focusRowAfterContext() {
        if (showPeopleRow) {
            focusZone = "people"
            peopleRow.focusList()
            ensureDetailsItemVisible(peopleRow)
        } else if (showSimilarRow) {
            focusZone = "similar"
            similarRow.focusList()
            ensureDetailsItemVisible(similarRow)
        }
    }

    function focusRowBeforeSimilar() {
        if (showPeopleRow) {
            focusZone = "people"
            peopleRow.focusList()
            ensureDetailsItemVisible(peopleRow)
        } else if (showContextRow && contextCount > 0) {
            focusZone = "context"
            contextRow.focusList()
            ensureDetailsItemVisible(contextRow)
        } else {
            focusBeforeMediaRows()
        }
    }

    function orderedActions() {
        const actions = []
        if (showPrimaryAction)
            actions.push(primaryAction)
        if (showPrimaryAction && hasProgress)
            actions.push(restartAction)
        actions.push(playedAction)
        actions.push(favoriteAction)
        actions.push(menuAction)
        return actions
    }

    function focusActionIndex(index) {
        const actions = orderedActions()
        if (actions.length === 0)
            return false
        focusZone = "actions"
        actionIndex = Math.max(0, Math.min(index, actions.length - 1))
        InputKeys.focus(actions[actionIndex])
        return true
    }

    function focusNextAction(delta) {
        return focusActionIndex(actionIndex + delta)
    }

    function activatePrimary(fromStart) {
        if (selectedIndex < 0 || !canPlay)
            return
        App.playFromModel(itemModel, selectedIndex, fromStart === true)
    }

    function playDetailContext(shuffled) {
        if (!showContextPlaybackActions)
            return
        App.playDetailContext(shuffled === true)
        overflowOpen = false
    }

    function toggleFavorite() {
        if (!item.movieId)
            return
        favoriteState = !favoriteState
        App.setFavorite(item.movieId, favoriteState)
    }

    function togglePlayed() {
        if (!item.movieId)
            return
        playedState = !playedState
        App.setPlayed(item.movieId, playedState)
    }

    function overflowOptions() {
        return showContextPlaybackActions ? [playAllOption, shuffleOption, mediaInfoOption] : [mediaInfoOption]
    }

    function focusOverflow(index) {
        const options = overflowOptions()
        overflowIndex = Math.max(0, Math.min(index, options.length - 1))
        focusZone = "overflow"
        InputKeys.focus(options[overflowIndex])
    }

    function toggleOverflow() {
        overflowOpen = !overflowOpen
        if (overflowOpen)
            Qt.callLater(function () {
                focusOverflow(0)
            })
        else
            focusActionIndex(orderedActions().indexOf(menuAction))
    }

    function openMediaInfo() {
        overflowOpen = false
        if (shell)
            shell.openMediaInfo(item)
    }

    function openSeriesLink() {
        if (seriesIdText.length <= 0)
            return
        if (shell)
            shell.replaceRoute("libraryGrid")
        App.openSeriesById(seriesIdText, seriesTitle)
    }

    function openSeasonLink() {
        if (seriesIdText.length <= 0)
            return
        if (shell)
            shell.replaceRoute("libraryGrid")
        App.openSeasonById(seriesIdText, seasonIdText, seasonTitleText)
    }

    function openContextItem(index) {
        if (index < 0)
            return
        if (typeText === "Series") {
            App.openDetailSeason(index)
            if (shell)
                shell.replaceRoute("libraryGrid")
            return
        }
        if (shell)
            shell.openDetailsAt(Content.detailSeasons, index, "context", shell.detailsReturnRoute || "libraryGrid")
    }

    function openSimilarItem(index) {
        if (index >= 0 && shell)
            shell.openDetailsAt(Content.detailSimilarItems, index, "similar", shell.detailsReturnRoute || "libraryGrid")
    }

    function openPerson(person) {
        if (person && person.personId && shell)
            shell.openPerson(person)
    }

    function openGenrePage(value) {
        if (!value)
            return
        App.openGenre(value)
        if (shell)
            shell.replaceRoute("libraryGrid")
    }

    function openStudioPage(value) {
        if (!value)
            return
        App.openStudio(value)
        if (shell)
            shell.replaceRoute("libraryGrid")
    }

    function routeKey(key, phase, repeat) {
        if (focusZone === "series") {
            if (key === Qt.Key_Up) {
                if (shell)
                    shell.focusNavBar()
            } else if (key === Qt.Key_Down) {
                if (seasonLink.visible) {
                    focusZone = "season"
                    InputKeys.focus(seasonLink)
                } else {
                    focusActionRow()
                }
            } else {
                return InputKeys.isHorizontal(key)
            }
            return true
        }
        if (focusZone === "season") {
            if (key === Qt.Key_Up) {
                if (seriesLink.visible) {
                    focusZone = "series"
                    InputKeys.focus(seriesLink)
                } else if (shell) {
                    shell.focusNavBar()
                }
            } else if (key === Qt.Key_Down) {
                focusActionRow()
            } else {
                return InputKeys.isHorizontal(key)
            }
            return true
        }
        if (focusZone === "overflow") {
            const options = overflowOptions()
            if (key === Qt.Key_Up) {
                if (overflowIndex > 0)
                    focusOverflow(overflowIndex - 1)
                else {
                    overflowOpen = false
                    focusActionIndex(orderedActions().indexOf(menuAction))
                }
            } else if (key === Qt.Key_Down) {
                if (overflowIndex + 1 < options.length)
                    focusOverflow(overflowIndex + 1)
                else {
                    overflowOpen = false
                    focusDetailsPanel()
                }
            } else if (InputKeys.isHorizontal(key)) {
                overflowOpen = false
                focusActionIndex(orderedActions().indexOf(menuAction))
            } else {
                return false
            }
            return true
        }
        if (focusZone === "metadata")
            return metadataPanel.routeKey(key, phase, repeat)
        if (focusZone === "context") {
            if (key === Qt.Key_Up)
                return focusBeforeMediaRows()
            if (key === Qt.Key_Down) {
                focusRowAfterContext()
                return true
            }
            return contextRow.routeKey(key, phase, repeat)
        }
        if (focusZone === "people") {
            if (key === Qt.Key_Up) {
                if (showContextRow && contextCount > 0) {
                    focusZone = "context"
                    contextRow.focusList()
                    ensureDetailsItemVisible(contextRow)
                } else {
                    focusBeforeMediaRows()
                }
                return true
            }
            if (key === Qt.Key_Down) {
                if (showSimilarRow) {
                    focusZone = "similar"
                    similarRow.focusList()
                    ensureDetailsItemVisible(similarRow)
                }
                return true
            }
            return peopleRow.routeKey(key, phase, repeat)
        }
        if (focusZone === "similar") {
            if (key === Qt.Key_Up) {
                focusRowBeforeSimilar()
                return true
            }
            if (key === Qt.Key_Down)
                return true
            return similarRow.routeKey(key, phase, repeat)
        }

        if (key === Qt.Key_Left)
            return focusNextAction(-1)
        if (key === Qt.Key_Right)
            return focusNextAction(1)
        if (key === Qt.Key_Up)
            return focusHeaderAboveActions()
        if (key === Qt.Key_Down) {
            if (overflowOpen && orderedActions()[actionIndex] === menuAction)
                focusOverflow(0)
            else
                focusDetailsPanel()
            return true
        }
        return false
    }

    function activate() {
        if (focusZone === "series") {
            openSeriesLink()
        } else if (focusZone === "season") {
            openSeasonLink()
        } else if (focusZone === "metadata") {
            metadataPanel.activate()
        } else if (focusZone === "context") {
            contextRow.activate()
        } else if (focusZone === "people") {
            peopleRow.activate()
        } else if (focusZone === "similar") {
            similarRow.activate()
        } else if (focusZone === "overflow") {
            const option = overflowOptions()[overflowIndex]
            if (option === playAllOption)
                playDetailContext(false)
            else if (option === shuffleOption)
                playDetailContext(true)
            else
                openMediaInfo()
        } else {
            const action = orderedActions()[actionIndex]
            if (action === playedAction)
                togglePlayed()
            else if (action === favoriteAction)
                toggleFavorite()
            else if (action === menuAction)
                toggleOverflow()
            else if (action === restartAction)
                activatePrimary(true)
            else
                activatePrimary(false)
        }
    }

    function longPress() {
        if (focusZone === "context")
            return contextRow.longPress()
        if (focusZone === "similar")
            return similarRow.longPress()
        return focusZone === "actions" && shell ? shell.openItemMenu(item, orderedActions()[actionIndex]) : false
    }

    function ensureDetailsItemVisible(target) {
        if (!target || !detailsFlick || !contentColumn)
            return
        Qt.callLater(function () {
            const mapped = target.mapToItem(contentColumn, 0, 0)
            const top = Math.max(0, mapped.y - 18)
            const bottom = mapped.y + target.height + 18
            const maxY = Math.max(0, detailsFlick.contentHeight - detailsFlick.height)
            if (top < detailsFlick.contentY)
                detailsFlick.contentY = Math.max(0, top)
            else if (bottom > detailsFlick.contentY + detailsFlick.height)
                detailsFlick.contentY = Math.min(maxY, bottom - detailsFlick.height)
        })
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

    function remainingText() {
        const remaining = Number(item.runtimeTicks || 0) - Number(item.resumeTicks || 0)
        return remaining > 0 ? runtimeText(remaining) + " left" : ""
    }

    function progressText() {
        const watched = runtimeText(item.resumeTicks)
        const total = runtimeText(item.runtimeTicks)
        if (watched.length <= 0)
            return ""
        return total.length > 0 ? watched + " of " + total : watched
    }

    function twoDigit(value) {
        const n = Number(value || 0)
        return n > 0 && n < 10 ? "0" + n : (n > 0 ? String(n) : "")
    }

    function seasonTitle() {
        if (typeText === "Season" && item.title)
            return item.title
        const season = Number(item.seasonNumber || 0)
        return season > 0 ? "Season " + season : "Season"
    }

    function contextRowTitle() {
        if (typeText === "Series")
            return "Seasons"
        if (typeText === "BoxSet")
            return "Collection"
        if (typeText === "Season")
            return "Episodes"
        return seasonTitleText !== "Season" ? "More from " + seasonTitleText : "More from this season"
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
        if (item.officialRating)
            parts.push(item.officialRating)
        const community = Number(item.communityRating || 0)
        if (community > 0)
            parts.push(community.toFixed(1))
        return parts.join(" / ")
    }

    function metadataLine() {
        const parts = []
        const years = yearRange()
        const rating = ratingText()
        const runtime = runtimeText(item.runtimeTicks)
        if (years.length > 0)
            parts.push(years)
        if (runtime.length > 0)
            parts.push(runtime)
        if (rating.length > 0)
            parts.push(rating)
        if (typeText.length > 0)
            parts.push(typeText)
        return parts.join(" / ")
    }

    function peopleByType(type) {
        const result = []
        for (let i = 0; i < metadataPeople.length; ++i) {
            const person = metadataPeople[i] || ({})
            if (String(person.type || "") === type)
                result.push(person)
        }
        return result
    }

    function valuesFromStrings(values) {
        const result = []
        if (!values)
            return result
        for (let i = 0; i < values.length; ++i) {
            const text = String(values[i] || "")
            if (text.length > 0)
                result.push(text)
        }
        return result
    }

    function buildMetadataRows() {
        const rows = []
        const directors = peopleByType("Director")
        const writers = peopleByType("Writer")
        const genres = valuesFromStrings(genreList)
        const studios = valuesFromStrings(studioList)
        if (directors.length > 0)
            rows.push({
                          label: directors.length === 1 ? "Director" : "Directors",
                          kind: "person",
                          values: directors
                      })
        if (writers.length > 0)
            rows.push({
                          label: writers.length === 1 ? "Writer" : "Writers",
                          kind: "person",
                          values: writers
                      })
        if (genres.length > 0)
            rows.push({
                          label: genres.length === 1 ? "Genre" : "Genres",
                          kind: "genre",
                          values: genres
                      })
        if (studios.length > 0)
            rows.push({
                          label: studios.length === 1 ? "Studio" : "Studios",
                          kind: "studio",
                          values: studios
                      })
        return rows
    }

    function chipText(value) {
        if (value === undefined || value === null)
            return ""
        if (typeof value === "string")
            return value
        return String(value.name || value.title || value.value || "")
    }

    function activateMetadata(kind, value) {
        if (kind === "genre")
            openGenrePage(chipText(value))
        else if (kind === "studio")
            openStudioPage(chipText(value))
        else if (kind === "person")
            openPerson(value)
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
    }

    Image {
        anchors.fill: parent
        source: root.backgroundArt
        fillMode: Image.PreserveAspectCrop
        opacity: status === Image.Ready ? 0.34 : 0
        asynchronous: true
        cache: true
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Theme.backdropScrimTop
            }
            GradientStop {
                position: 0.48
                color: Theme.backdropScrimMiddle
            }
            GradientStop {
                position: 1.0
                color: Theme.bg
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop {
                position: 0.0
                color: Theme.backdropScrimLeft
            }
            GradientStop {
                position: 0.55
                color: Theme.bg
            }
            GradientStop {
                position: 1.0
                color: Theme.backdropScrimRight
            }
        }
    }

    Flickable {
        id: detailsFlick
        anchors.fill: parent
        contentWidth: width
        contentHeight: Math.max(height, contentColumn.implicitHeight + root.contentMargin)
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        FastWheelHandler {
            flickable: detailsFlick
        }

        Behavior on contentY {
            enabled: !Theme.reducedMotion
            NumberAnimation {
                duration: 90
                easing.type: Easing.OutCubic
            }
        }

        ColumnLayout {
            id: contentColumn
            width: detailsFlick.width
            height: implicitHeight
            spacing: Metrics.sectionGap(root.width)

            Item {
                id: hero
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(Metrics.detailHeroHeight(root.height), heroCopy.implicitHeight
                                                 + root.contentMargin * 1.4)

                ColumnLayout {
                    id: heroCopy
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.leftMargin: root.contentMargin
                    anchors.topMargin: Math.max(30, root.height * 0.05)
                    width: root.copyWidth
                    spacing: 14

                    DetailLink {
                        id: seriesLink
                        label: root.showSeriesLink ? root.seriesTitle : ""
                        onActivated: root.openSeriesLink()
                    }

                    DetailLink {
                        id: seasonLink
                        label: root.showSeasonLink ? root.seasonTitleText : ""
                        onActivated: root.openSeasonLink()
                    }

                    AppText {
                        Layout.fillWidth: true
                        text: root.typeText === "Episode" && root.twoDigit(root.item.episodeNumber).length > 0 ? "E"
                                                                                                                 + root.twoDigit(
                                                                                                                     root.item.episodeNumber)
                                                                                                                 + " " + root.titleText :
                                                                                                                 root.titleText
                        font.pixelSize: root.detailTitlePx
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        lineHeight: 0.96
                    }

                    TechMetadataLine {
                        Layout.fillWidth: true
                        visible: root.metadataLine().length > 0
                        metadata: root.metadataLine()
                    }

                    AppText {
                        Layout.fillWidth: true
                        Layout.topMargin: 8
                        visible: root.item.overview && root.item.overview.length > 0
                        text: root.item.overview || ""
                        color: Theme.textSecondary
                        wrapMode: Text.Wrap
                        font.pixelSize: Metrics.bodyPx(root.width) + 1
                        lineHeight: 1.18
                        maximumLineCount: 5
                        elide: Text.ElideRight
                    }

                    Row {
                        id: actionRow
                        Layout.topMargin: 18
                        spacing: 10

                        DetailAction {
                            id: primaryAction
                            iconName: "play_arrow"
                            label: root.primaryLabel()
                            primary: true
                            visible: root.showPrimaryAction
                            enabledButton: root.showPrimaryAction
                            onActivated: root.activatePrimary(false)
                        }

                        DetailAction {
                            id: restartAction
                            iconName: "replay"
                            label: "Restart"
                            visible: root.showPrimaryAction && root.hasProgress
                            enabledButton: root.showPrimaryAction && root.hasProgress
                            onActivated: root.activatePrimary(true)
                        }

                        IconAction {
                            id: playedAction
                            iconName: root.playedState ? "check_circle" : "radio_button_unchecked"
                            label: root.playedState ? "Mark unwatched" : "Mark watched"
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

                    MonoText {
                        Layout.fillWidth: true
                        visible: root.hasProgress && root.remainingText().length > 0
                        text: root.progressText() + " / " + root.remainingText()
                        color: Theme.textMuted
                        font.pixelSize: Metrics.metaPx(root.width)
                    }

                    Rectangle {
                        id: overflowMenu
                        Layout.preferredWidth: 292
                        Layout.preferredHeight: root.overflowOpen ? menuColumn.implicitHeight + 14 : 0
                        visible: root.overflowOpen
                        radius: Theme.radiusMedium
                        color: Theme.floatingPanel
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
                                id: playAllOption
                                visible: root.showContextPlaybackActions
                                iconName: "play_arrow"
                                label: "Play all"
                                onActivated: root.playDetailContext(false)
                            }

                            MenuOption {
                                id: shuffleOption
                                visible: root.showContextPlaybackActions
                                iconName: "shuffle"
                                label: "Shuffle play"
                                onActivated: root.playDetailContext(true)
                            }

                            MenuOption {
                                id: mediaInfoOption
                                iconName: "info"
                                label: "Media info"
                                onActivated: root.openMediaInfo()
                            }
                        }
                    }

                    MetadataPanel {
                        id: metadataPanel
                        Layout.fillWidth: true
                        Layout.topMargin: 28
                        rows: root.metadataRows
                        onActivated: (kind, value) => root.activateMetadata(kind, value)
                        onLeaveUp: root.focusActionRow()
                        onLeaveDown: root.focusFirstMediaRow()
                    }
                }

                Item {
                    id: stillPanel
                    visible: root.showSideArt
                    anchors.right: parent.right
                    anchors.rightMargin: root.contentMargin
                    anchors.top: heroCopy.top
                    width: Math.min(parent.width * 0.38, 760)
                    height: Math.round(width * 9 / 16) + (root.hasProgress ? 46 : 0)

                    ImageCard {
                        id: stillCard
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        height: Math.round(width * 9 / 16)
                        imageUrl: root.stillArt
                        fallbackText: root.typeText
                        focused: false
                        retainWhileLoading: true
                    }

                    Rectangle {
                        anchors.left: stillCard.left
                        anchors.right: stillCard.right
                        anchors.bottom: stillCard.bottom
                        height: 5
                        visible: root.hasProgress
                        color: Theme.border
                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: parent.width * Math.max(0, Math.min(1, root.item.progress || 0))
                            color: Theme.accent
                        }
                    }

                    RowLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: stillCard.bottom
                        anchors.topMargin: 12
                        visible: root.hasProgress

                        MonoText {
                            Layout.fillWidth: true
                            text: root.progressText()
                            color: Theme.textMuted
                            font.pixelSize: Metrics.metaPx(root.width)
                            elide: Text.ElideRight
                        }
                        MonoText {
                            text: root.remainingText()
                            color: Theme.textMuted
                            font.pixelSize: Metrics.metaPx(root.width)
                        }
                    }
                }
            }

            Item {
                id: detailRowsArea
                Layout.fillWidth: true
                readonly property int rowSpacing: Metrics.sectionGap(root.width)
                readonly property int visibleRowCount: (contextRow.visible ? 1 : 0) + (peopleRow.visible ? 1 : 0) + (
                                                           similarRow.visible ? 1 : 0)
                readonly property int rowsHeight: contextRow.height + peopleRow.height + similarRow.height + Math.max(0,
                                                                                                                      visibleRowCount
                                                                                                                      - 1) * rowSpacing
                readonly property int contextY: 0
                readonly property int peopleY: contextY + (contextRow.visible ? contextRow.height + rowSpacing : 0)
                readonly property int similarY: peopleY + (peopleRow.visible ? peopleRow.height + rowSpacing : 0)
                Layout.preferredHeight: rowsHeight + root.contentMargin
                implicitHeight: rowsHeight + root.contentMargin
                height: implicitHeight

                MediaPosterScrollerRow {
                    id: contextRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: root.contentMargin
                    anchors.rightMargin: root.contentMargin
                    y: detailRowsArea.contextY
                    title: root.contextRowTitle()
                    rowModel: Content.detailSeasons
                    shell: root.shell
                    cardWidth: root.contextPosterCards ? root.rowPosterWidth : root.rowLandscapeWidth
                    cardKind: root.contextPosterCards ? "poster" : "landscape"
                    rowGap: root.rowGap
                    enabledRow: root.showContextRow
                    reserveWhenEmpty: root.reserveContextRow
                    loading: root.reserveContextRow
                    emptyText: root.typeText === "Series" ? "Loading seasons..." : (root.typeText === "BoxSet"
                                                                                    ? "Loading collection..." :
                                                                                      "Loading episodes...")
                    useSeriesPoster: root.typeText === "Series"
                    preferEpisodeTitle: !root.contextPosterCards
                    onActivated: index => root.openContextItem(index)
                }

                PersonScrollerRow {
                    id: peopleRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: root.contentMargin
                    anchors.rightMargin: root.contentMargin
                    y: detailRowsArea.peopleY
                    title: "Cast & Crew"
                    peopleModel: root.people
                    shell: root.shell
                    rowGap: root.rowGap
                    enabledRow: root.showPeopleRow
                    onActivated: person => root.openPerson(person)
                }

                MediaPosterScrollerRow {
                    id: similarRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: root.contentMargin
                    anchors.rightMargin: root.contentMargin
                    y: detailRowsArea.similarY
                    title: "More Like This"
                    rowModel: Content.detailSimilarItems
                    shell: root.shell
                    cardWidth: root.typeText === "Episode" ? root.rowLandscapeWidth : root.rowPosterWidth
                    cardKind: root.typeText === "Episode" ? "landscape" : "poster"
                    rowGap: root.rowGap
                    enabledRow: root.showSimilarRow
                    useSeriesPoster: root.typeText !== "Episode"
                    preferEpisodeTitle: root.typeText === "Episode"
                    onActivated: index => root.openSimilarItem(index)
                }
            }
        }
    }
}
