import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root

    property var shell
    readonly property var itemModel: shell && shell.detailsModel ? shell.detailsModel : appController.movies
    readonly property int itemCount: itemModel && itemModel.rowCount ? itemModel.rowCount() : 0
    readonly property int selectedIndex: itemCount > 0 && shell ? shell.detailsIndexForModel(itemModel) : -1
    readonly property var item: selectedIndex >= 0 && itemModel ? itemModel.get(selectedIndex) : ({})
    readonly property string detailSource: shell && shell.detailsRoute ? shell.detailsRoute.source : "movies"
    readonly property string typeText: item.itemType || "Media"
    readonly property string titleText: typeText === "Episode" && item.title ? item.title : (item.displayTitle || item.title || item.seriesName || "Selected item")
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
    readonly property string backgroundArt: item.backdropUrl || item.thumbUrl || item.landscapeCardUrl || item.posterUrl || ""
    readonly property string stillArt: item.landscapeCardUrl || item.thumbUrl || item.backdropUrl || item.posterUrl || item.seriesPosterUrl || ""
    readonly property bool showSideArt: width >= 1120 && stillArt.length > 0
    readonly property real copyWidth: showSideArt ? Math.min(width * 0.56, 940) : width - contentMargin * 2
    readonly property bool loadingDetailRows: appController ? appController.detailRowsBusy : false
    readonly property int contextCount: appController && appController.detailSeasons ? appController.detailSeasons.count : 0
    readonly property int similarCount: appController && appController.detailSimilarItems ? appController.detailSimilarItems.count : 0
    readonly property bool contextItemsPossible: typeText === "Series" || ((typeText === "Episode" || typeText === "Season") && seriesIdText.length > 0)
    readonly property bool reserveContextRow: contextItemsPossible && contextCount === 0 && loadingDetailRows
    readonly property bool showContextRow: contextCount > 0 || reserveContextRow
    readonly property bool showSimilarRow: similarCount > 0
    readonly property var fullDetailItem: appController && appController.detailItem
                                      && String(appController.detailItem.movieId || "") === String(item.movieId || "")
                                      ? appController.detailItem : ({})
    readonly property var people: fullDetailItem.people || []
    readonly property bool showPeopleRow: people.length > 0
    readonly property var genreList: item.genres || []
    readonly property var studioList: item.studios || []
    readonly property var metadataRows: buildMetadataRows()
    readonly property bool showMetadataPanel: metadataRows.length > 0
    readonly property bool showSeriesLink: (typeText === "Episode" || typeText === "Season") && seriesIdText.length > 0 && seriesTitle.length > 0
    readonly property bool showSeasonLink: typeText === "Episode" && seriesIdText.length > 0 && (seasonIdText.length > 0 || Number(item.seasonNumber || 0) > 0)

    property bool favoriteState: false
    property bool playedState: false
    property bool overflowOpen: false
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
        implicitWidth: linkRow.implicitWidth
        implicitHeight: linkText.implicitHeight + 6
        focus: true

        Row {
            id: linkRow
            anchors.left: parent.left
            anchors.top: parent.top
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
            x: linkRow.x + linkText.x
            y: linkRow.y + linkText.y + linkText.height + 2
            width: linkText.width
            height: link.activeFocus ? 3 : 2
            radius: height / 2
            color: link.activeFocus || hover.hovered ? Theme.accent : Theme.borderStrong
            opacity: link.activeFocus || hover.hovered ? 1 : 0.75
        }


        HoverHandler {
            id: hover
        }
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onClicked: link.activated()
        }
        Keys.onReleased: event => {
            if (InputKeys.isAccept(event.key, false)) {
                link.activated();
                event.accepted = true;
            }
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
            browsing = false;
            forceActiveFocus();
        }

        function chipText(value) {
            if (value === undefined || value === null)
                return "";
            if (typeof value === "string")
                return value;
            return String(value.name || value.title || value.value || "");
        }

        function rowValues(row) {
            if (!hasRows || row < 0 || row >= rows.length)
                return [];
            return rows[row].values || [];
        }

        function normalizeSelection() {
            if (!hasRows) {
                currentRow = 0;
                currentChip = 0;
                browsing = false;
                return;
            }
            currentRow = Math.max(0, Math.min(currentRow, rows.length - 1));
            currentChip = Math.max(0, Math.min(currentChip, Math.max(0, rowValues(currentRow).length - 1)));
        }

        function moveRow(delta) {
            const next = currentRow + delta;
            if (next < 0) {
                browsing = false;
                leaveUp();
                return true;
            }
            if (next >= rows.length) {
                browsing = false;
                leaveDown();
                return true;
            }
            currentRow = next;
            currentChip = Math.min(currentChip, Math.max(0, rowValues(currentRow).length - 1));
            return true;
        }

        function moveChip(delta) {
            const values = rowValues(currentRow);
            if (values.length <= 0)
                return true;
            currentChip = Math.max(0, Math.min(currentChip + delta, values.length - 1));
            return true;
        }

        function activateCurrent() {
            normalizeSelection();
            const values = rowValues(currentRow);
            if (values.length <= 0)
                return false;
            activated(rows[currentRow].kind || "", values[currentChip]);
            return true;
        }

        function leaveBrowse() {
            browsing = false;
            forceActiveFocus();
        }

        function handleNavigationKey(key) {
            if (!hasRows)
                return false;
            normalizeSelection();
            if (!browsing) {
                if (InputKeys.isAccept(key, false)) {
                    browsing = true;
                    return true;
                }
                if (key === Qt.Key_Up) {
                    leaveUp();
                    return true;
                }
                if (key === Qt.Key_Down) {
                    leaveDown();
                    return true;
                }
                return key === Qt.Key_Left || key === Qt.Key_Right;
            }
            if (InputKeys.isBack(key, false, false)) {
                leaveBrowse();
                return true;
            }
            if (InputKeys.isAccept(key, false))
                return activateCurrent();
            if (key === Qt.Key_Left)
                return moveChip(-1);
            if (key === Qt.Key_Right)
                return moveChip(1);
            if (key === Qt.Key_Up)
                return moveRow(-1);
            if (key === Qt.Key_Down)
                return moveRow(1);
            return false;
        }

        width: parent ? parent.width : implicitWidth
        height: visible ? block.implicitHeight : 0
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
                            color: panel.browsing && panel.currentRow === rowDelegate.index ? Theme.textPrimary : Theme.textMuted
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
                                    selected: panel.browsing && panel.currentRow === rowDelegate.index && panel.currentChip === index
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
        syncUserState();
        updateDetailCounts();
        Qt.callLater(refreshDetailRows);
        Qt.callLater(refreshItemDetail);
        Qt.callLater(focusDefaultAction);
    }

    onActiveFocusChanged: if (activeFocus)
        focusDefaultAction()
    onItemChanged: {
        overflowOpen = false;
        syncUserState();
        Qt.callLater(refreshDetailRows);
        Qt.callLater(refreshItemDetail);
    }

    Connections {
        target: appController
        function onDetailRowsChanged() {
            root.updateDetailCounts();
        }
        function onItemFavoriteChanged(itemId, favorite) {
            if ((root.item.movieId || "") === itemId)
                root.favoriteState = favorite;
        }
        function onItemPlayedChanged(itemId, played) {
            if ((root.item.movieId || "") === itemId)
                root.playedState = played;
        }
    }

    Connections {
        target: appController ? appController.detailSeasons : null
        function onModelReset() {
            root.updateDetailCounts();
        }
        function onRowsInserted() {
            root.updateDetailCounts();
        }
        function onRowsRemoved() {
            root.updateDetailCounts();
        }
    }

    Connections {
        target: appController ? appController.detailSimilarItems : null
        function onModelReset() {
            root.updateDetailCounts();
        }
        function onRowsInserted() {
            root.updateDetailCounts();
        }
        function onRowsRemoved() {
            root.updateDetailCounts();
        }
    }

    function handleBack() {
        if (metadataPanel.activeFocus && metadataPanel.browsing) {
            metadataPanel.leaveBrowse();
            return true;
        }
        return false;
    }

    function updateDetailCounts() {
        contextRow.currentIndex = contextCount > 0 ? Math.max(0, Math.min(contextRow.currentIndex, contextCount - 1)) : 0;
        similarRow.currentIndex = similarCount > 0 ? Math.max(0, Math.min(similarRow.currentIndex, similarCount - 1)) : 0;
    }

    function refreshDetailRows() {
        const itemId = item.movieId || "";
        const key = itemId + ":" + typeText + ":" + seriesIdText + ":" + seasonIdText;
        if (key === loadedDetailKey)
            return;
        loadedDetailKey = key;
        contextRow.currentIndex = 0;
        similarRow.currentIndex = 0;
        if (appController)
            appController.loadDetailRows(itemId, typeText, seriesIdText, seasonIdText);
    }

    function refreshItemDetail() {
        const itemId = item.movieId || "";
        if (itemId.length > 0 && appController)
            appController.loadItemDetail(itemId);
    }

    function syncUserState() {
        favoriteState = Boolean(item.favorite);
        playedState = Boolean(item.played);
    }

    function focusDefaultAction() {
        if (detailsFlick)
            detailsFlick.contentY = 0;
        focusActionRow();
    }

    function focusActionRow() {
        if (showPrimaryAction && primaryAction.enabledButton)
            primaryAction.forceActiveFocus();
        else
            playedAction.forceActiveFocus();
        ensureDetailsItemVisible(actionRow);
    }

    function focusHeaderAboveActions() {
        if (seasonLink.visible) {
            seasonLink.forceActiveFocus();
            ensureDetailsItemVisible(seasonLink);
            return true;
        }
        if (seriesLink.visible) {
            seriesLink.forceActiveFocus();
            ensureDetailsItemVisible(seriesLink);
            return true;
        }
        if (detailsFlick)
            detailsFlick.contentY = 0;
        if (shell)
            shell.focusNavBar();
        return true;
    }

    function focusDetailsPanel() {
        if (!showMetadataPanel)
            return focusFirstMediaRow();
        metadataPanel.focusPanel();
        ensureDetailsItemVisible(metadataPanel);
        return true;
    }

    function focusBeforeMediaRows() {
        return showMetadataPanel ? focusDetailsPanel() : focusActionRow();
    }

    function focusFirstMediaRow() {
        if (showContextRow && contextCount > 0) {
            contextRow.focusList();
            ensureDetailsItemVisible(contextRow);
            return true;
        }
        if (showPeopleRow) {
            peopleRow.focusList();
            ensureDetailsItemVisible(peopleRow);
            return true;
        }
        if (showSimilarRow) {
            similarRow.focusList();
            ensureDetailsItemVisible(similarRow);
            return true;
        }
        return false;
    }

    function focusRowAfterContext() {
        if (showPeopleRow) {
            peopleRow.focusList();
            ensureDetailsItemVisible(peopleRow);
        } else if (showSimilarRow) {
            similarRow.focusList();
            ensureDetailsItemVisible(similarRow);
        }
    }

    function focusRowBeforeSimilar() {
        if (showPeopleRow) {
            peopleRow.focusList();
            ensureDetailsItemVisible(peopleRow);
        } else if (showContextRow && contextCount > 0) {
            contextRow.focusList();
            ensureDetailsItemVisible(contextRow);
        } else {
            focusBeforeMediaRows();
        }
    }

    function orderedActions() {
        const actions = [];
        if (showPrimaryAction)
            actions.push(primaryAction);
        if (showPrimaryAction && hasProgress)
            actions.push(restartAction);
        actions.push(playedAction);
        actions.push(favoriteAction);
        actions.push(menuAction);
        return actions;
    }

    function focusedActionIndex() {
        const actions = orderedActions();
        for (let i = 0; i < actions.length; ++i) {
            if (actions[i].activeFocus)
                return i;
        }
        return -1;
    }

    function focusActionIndex(index) {
        const actions = orderedActions();
        if (actions.length === 0)
            return false;
        actions[Math.max(0, Math.min(index, actions.length - 1))].forceActiveFocus();
        return true;
    }

    function focusNextAction(delta) {
        const actions = orderedActions();
        const current = focusedActionIndex();
        if (current < 0)
            return focusActionIndex(0);
        const next = current + delta;
        if (next < 0)
            return false;
        if (next >= actions.length)
            return true;
        actions[next].forceActiveFocus();
        return true;
    }

    function activatePrimary(fromStart) {
        if (selectedIndex < 0 || !canPlay)
            return;
        const start = fromStart === true;
        if (detailSource === "search")
            appController.playSearchResult(selectedIndex, start);
        else if (detailSource === "resume")
            appController.playResumeItem(selectedIndex, start);
        else if (detailSource === "nextup")
            appController.playNextUpItem(selectedIndex, start);
        else if (detailSource.indexOf("latestLibrary:") === 0)
            appController.playLatestLibraryItem(parseInt(detailSource.split(":")[1], 10), selectedIndex, start);
        else if (detailSource === "person")
            appController.playPersonItem(selectedIndex, start);
        else if (detailSource === "context")
            appController.playDetailSeasonItem(selectedIndex, start);
        else if (detailSource === "similar")
            appController.playDetailSimilarItem(selectedIndex, start);
        else if (detailSource === "suggestion")
            appController.playSuggestionItem(selectedIndex, start);
        else
            appController.playMovie(selectedIndex, start);
    }

    function toggleFavorite() {
        if (!item.movieId || !appController)
            return;
        favoriteState = !favoriteState;
        appController.setFavorite(item.movieId, favoriteState);
    }

    function togglePlayed() {
        if (!item.movieId || !appController)
            return;
        playedState = !playedState;
        appController.setPlayed(item.movieId, playedState);
    }

    function toggleOverflow() {
        overflowOpen = !overflowOpen;
        if (overflowOpen)
            Qt.callLater(function () {
                mediaInfoOption.forceActiveFocus();
            });
        else
            menuAction.forceActiveFocus();
    }

    function openMediaInfo() {
        overflowOpen = false;
        if (shell)
            shell.openMediaInfo(item);
    }

    function openSeriesLink() {
        if (seriesIdText.length <= 0 || !appController)
            return;
        if (shell)
            shell.replaceRoute("libraryGrid");
        appController.openSeriesById(seriesIdText, seriesTitle);
    }

    function openSeasonLink() {
        if (seriesIdText.length <= 0 || !appController)
            return;
        if (shell)
            shell.replaceRoute("libraryGrid");
        appController.openSeasonById(seriesIdText, seasonIdText, seasonTitleText);
    }

    function openContextItem(index) {
        if (index < 0 || !appController)
            return;
        if (typeText === "Series") {
            appController.openDetailSeason(index);
            if (shell)
                shell.replaceRoute("libraryGrid");
            return;
        }
        if (shell)
            shell.openDetailsAt(appController.detailSeasons, index, "context", shell.detailsReturnRoute || "libraryGrid");
    }

    function openSimilarItem(index) {
        if (index >= 0 && shell && appController)
            shell.openDetailsAt(appController.detailSimilarItems, index, "similar", shell.detailsReturnRoute || "libraryGrid");
    }

    function openPerson(person) {
        if (person && person.personId && shell)
            shell.openPerson(person);
    }

    function openGenrePage(value) {
        if (!value || !appController)
            return;
        appController.openGenre(value);
        if (shell)
            shell.replaceRoute("libraryGrid");
    }

    function openStudioPage(value) {
        if (!value || !appController)
            return;
        appController.openStudio(value);
        if (shell)
            shell.replaceRoute("libraryGrid");
    }

    function handlePressedKey(key) {
        if (contextRow.activeFocus)
            return contextRow.handlePressedKey(key);
        if (similarRow.activeFocus)
            return similarRow.handlePressedKey(key);
        return false;
    }

    function handleNavigationKey(key) {
        if (mediaInfoOption.activeFocus) {
            if (key === Qt.Key_Up || key === Qt.Key_Left || key === Qt.Key_Right) {
                overflowOpen = false;
                menuAction.forceActiveFocus();
                return true;
            }
            if (key === Qt.Key_Down) {
                overflowOpen = false;
                focusDetailsPanel();
                return true;
            }
            if (InputKeys.isAccept(key, false)) {
                openMediaInfo();
                return true;
            }
        }

        if (seriesLink.activeFocus) {
            if (key === Qt.Key_Up) {
                if (shell)
                    shell.focusNavBar();
                return true;
            }
            if (key === Qt.Key_Down) {
                seasonLink.visible ? seasonLink.forceActiveFocus() : focusActionRow();
                return true;
            }
            if (InputKeys.isAccept(key, false)) {
                openSeriesLink();
                return true;
            }
        }

        if (seasonLink.activeFocus) {
            if (key === Qt.Key_Up) {
                seriesLink.visible ? seriesLink.forceActiveFocus() : (shell ? shell.focusNavBar() : null);
                return true;
            }
            if (key === Qt.Key_Down) {
                focusActionRow();
                return true;
            }
            if (InputKeys.isAccept(key, false)) {
                openSeasonLink();
                return true;
            }
        }

        if (metadataPanel.activeFocus)
            return metadataPanel.handleNavigationKey(key);

        if (contextRow.activeFocus) {
            if (key === Qt.Key_Up) {
                focusBeforeMediaRows();
                return true;
            }
            if (key === Qt.Key_Down) {
                focusRowAfterContext();
                return true;
            }
            return contextRow.handleNavigationKey(key);
        }

        if (peopleRow.activeFocus) {
            if (key === Qt.Key_Up) {
                if (showContextRow && contextCount > 0)
                    contextRow.focusList();
                else
                    focusBeforeMediaRows();
                ensureDetailsItemVisible(showContextRow && contextCount > 0 ? contextRow : metadataPanel);
                return true;
            }
            if (key === Qt.Key_Down) {
                if (showSimilarRow) {
                    similarRow.focusList();
                    ensureDetailsItemVisible(similarRow);
                }
                return true;
            }
            return peopleRow.handleNavigationKey(key);
        }

        if (similarRow.activeFocus) {
            if (key === Qt.Key_Up) {
                focusRowBeforeSimilar();
                return true;
            }
            if (key === Qt.Key_Down)
                return true;
            return similarRow.handleNavigationKey(key);
        }

        if (key === Qt.Key_Left) {
            focusNextAction(-1);
            return true;
        }
        if (key === Qt.Key_Right) {
            focusNextAction(1);
            return true;
        }
        if (key === Qt.Key_Up)
            return focusHeaderAboveActions();
        if (key === Qt.Key_Down) {
            if (overflowOpen && menuAction.activeFocus) {
                mediaInfoOption.forceActiveFocus();
                return true;
            }
            focusDetailsPanel();
            return true;
        }
        if (InputKeys.isAccept(key, false)) {
            if (playedAction.activeFocus)
                togglePlayed();
            else if (favoriteAction.activeFocus)
                toggleFavorite();
            else if (menuAction.activeFocus)
                toggleOverflow();
            else if (restartAction.activeFocus)
                activatePrimary(true);
            else
                activatePrimary(false);
            return true;
        }
        return false;
    }

    function ensureDetailsItemVisible(target) {
        if (!target || !detailsFlick || !contentColumn)
            return;
        Qt.callLater(function () {
            const mapped = target.mapToItem(contentColumn, 0, 0);
            const top = Math.max(0, mapped.y - 18);
            const bottom = mapped.y + target.height + 18;
            const maxY = Math.max(0, detailsFlick.contentHeight - detailsFlick.height);
            if (top < detailsFlick.contentY)
                detailsFlick.contentY = Math.max(0, top);
            else if (bottom > detailsFlick.contentY + detailsFlick.height)
                detailsFlick.contentY = Math.min(maxY, bottom - detailsFlick.height);
        });
    }

    function primaryLabel() {
        return item.playActionLabel || "Play";
    }

    function runtimeText(ticks) {
        ticks = Number(ticks || 0);
        if (ticks <= 0)
            return "";
        const minutes = Math.round(ticks / 600000000);
        const hours = Math.floor(minutes / 60);
        const mins = minutes % 60;
        return hours > 0 ? hours + "h " + mins + "m" : mins + "m";
    }

    function remainingText() {
        const remaining = Number(item.runtimeTicks || 0) - Number(item.resumeTicks || 0);
        return remaining > 0 ? runtimeText(remaining) + " left" : "";
    }

    function progressText() {
        const watched = runtimeText(item.resumeTicks);
        const total = runtimeText(item.runtimeTicks);
        if (watched.length <= 0)
            return "";
        return total.length > 0 ? watched + " of " + total : watched;
    }

    function twoDigit(value) {
        const n = Number(value || 0);
        return n > 0 && n < 10 ? "0" + n : (n > 0 ? String(n) : "");
    }

    function seasonTitle() {
        if (typeText === "Season" && item.title)
            return item.title;
        const season = Number(item.seasonNumber || 0);
        return season > 0 ? "Season " + season : "Season";
    }

    function contextRowTitle() {
        if (typeText === "Series")
            return "Seasons";
        if (typeText === "Season")
            return "Episodes";
        return seasonTitleText !== "Season" ? "More from " + seasonTitleText : "More from this season";
    }

    function yearFromDate(value) {
        if (!value || value.length < 4)
            return 0;
        const parsed = parseInt(String(value).slice(0, 4), 10);
        return isNaN(parsed) ? 0 : parsed;
    }

    function yearRange() {
        const start = Number(item.year || 0) > 0 ? Number(item.year) : yearFromDate(item.premiereDate);
        const end = yearFromDate(item.endDate);
        if (typeText === "Series" && start > 0 && end > 0 && end !== start)
            return start + " - " + end;
        return start > 0 ? String(start) : "";
    }

    function ratingText() {
        const parts = [];
        if (item.officialRating)
            parts.push(item.officialRating);
        const community = Number(item.communityRating || 0);
        if (community > 0)
            parts.push(community.toFixed(1));
        return parts.join(" / ");
    }

    function metadataLine() {
        const parts = [];
        const years = yearRange();
        const rating = ratingText();
        const runtime = runtimeText(item.runtimeTicks);
        if (years.length > 0)
            parts.push(years);
        if (runtime.length > 0)
            parts.push(runtime);
        if (rating.length > 0)
            parts.push(rating);
        if (typeText.length > 0)
            parts.push(typeText);
        return parts.join(" / ");
    }

    function peopleByType(type) {
        const result = [];
        for (let i = 0; i < people.length; ++i) {
            const person = people[i] || ({});
            if (String(person.type || "") === type)
                result.push(person);
        }
        return result;
    }

    function valuesFromStrings(values) {
        const result = [];
        if (!values)
            return result;
        for (let i = 0; i < values.length; ++i) {
            const text = String(values[i] || "");
            if (text.length > 0)
                result.push(text);
        }
        return result;
    }

    function buildMetadataRows() {
        const rows = [];
        const genres = valuesFromStrings(genreList);
        const directors = peopleByType("Director");
        const writers = peopleByType("Writer");
        const studios = valuesFromStrings(studioList);
        if (genres.length > 0)
            rows.push({
                label: "Genres",
                kind: "genre",
                values: genres
            });
        if (directors.length > 0)
            rows.push({
                label: directors.length === 1 ? "Director" : "Directors",
                kind: "person",
                values: directors
            });
        if (writers.length > 0)
            rows.push({
                label: writers.length === 1 ? "Writer" : "Writers",
                kind: "person",
                values: writers
            });
        if (studios.length > 0)
            rows.push({
                label: studios.length === 1 ? "Studio" : "Studios",
                kind: "studio",
                values: studios
            });
        return rows;
    }

    function chipText(value) {
        if (value === undefined || value === null)
            return "";
        if (typeof value === "string")
            return value;
        return String(value.name || value.title || value.value || "");
    }

    function activateMetadata(kind, value) {
        if (kind === "genre")
            openGenrePage(chipText(value));
        else if (kind === "studio")
            openStudioPage(chipText(value));
        else if (kind === "person")
            openPerson(value);
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
                Layout.preferredHeight: Math.max(Metrics.detailHeroHeight(root.height), heroCopy.implicitHeight + root.contentMargin * 1.4)

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
                        text: root.typeText === "Episode" && root.twoDigit(root.item.episodeNumber).length > 0
                              ? "E" + root.twoDigit(root.item.episodeNumber) + " " + root.titleText
                              : root.titleText
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
                readonly property int visibleRowCount: (contextRow.visible ? 1 : 0) + (peopleRow.visible ? 1 : 0) + (similarRow.visible ? 1 : 0)
                readonly property int rowsHeight: contextRow.height + peopleRow.height + similarRow.height + Math.max(0, visibleRowCount - 1) * rowSpacing
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
                    rowModel: appController ? appController.detailSeasons : null
                    shell: root.shell
                    cardWidth: root.typeText === "Series" ? root.rowPosterWidth : root.rowLandscapeWidth
                    cardKind: root.typeText === "Series" ? "poster" : "landscape"
                    rowGap: root.rowGap
                    enabledRow: root.showContextRow
                    reserveWhenEmpty: root.reserveContextRow
                    loading: root.reserveContextRow
                    emptyText: root.typeText === "Series" ? "Loading seasons..." : "Loading episodes..."
                    useSeriesPoster: root.typeText === "Series"
                    preferEpisodeTitle: root.typeText !== "Series"
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
                    rowModel: appController ? appController.detailSimilarItems : null
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
