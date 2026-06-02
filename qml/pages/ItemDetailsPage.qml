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
    property int seasonCount: appController && appController.detailSeasons ? appController.detailSeasons.rowCount() : 0
    property int similarCount: appController && appController.detailSimilarItems ? appController.detailSimilarItems.rowCount() : 0
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
        signal activated(int index)

        Layout.fillWidth: true
        Layout.preferredHeight: visible && rowCount > 0 ? listView.height + rowHeader.implicitHeight + 16 : 0
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

        function handleNavigationKey(key) {
            if (rowCount <= 0)
                return false
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
            if (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select || key === Qt.Key_Space) {
                currentIndex = listView.currentIndex
                activated(listView.currentIndex)
                return true
            }
            return false
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            SectionHeader {
                id: rowHeader
                Layout.fillWidth: true
                title: rowRoot.title
            }

            ListView {
                id: listView
                Layout.fillWidth: true
                Layout.preferredHeight: rowRoot.cardWidth * 1.5 + 56
                focus: true
                keyNavigationEnabled: false
                clip: true
                orientation: ListView.Horizontal
                boundsBehavior: Flickable.StopAtBounds
                spacing: root.rowGap
                model: rowRoot.rowModel
                currentIndex: rowRoot.rowCount > 0 ? Math.max(0, Math.min(rowRoot.currentIndex, rowRoot.rowCount - 1)) : -1

                delegate: Item {
                    id: posterDelegate
                    required property int index
                    required property string title
                    required property string posterUrl
                    required property int year
                    required property string subtitle
                    required property string displayTitle
                    required property string displaySubtitle
                    width: rowRoot.cardWidth
                    height: listView.height

                    PosterCard {
                        anchors.fill: parent
                        title: posterDelegate.displayTitle || posterDelegate.title
                        posterUrl: posterDelegate.posterUrl
                        year: posterDelegate.year
                        metadata: posterDelegate.displaySubtitle || posterDelegate.subtitle
                        focused: posterDelegate.index === listView.currentIndex && listView.activeFocus
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            listView.currentIndex = posterDelegate.index
                            rowRoot.currentIndex = posterDelegate.index
                            rowRoot.activated(posterDelegate.index)
                        }
                    }
                }

                Keys.onReleased: (event) => {
                    if (rowRoot.handleNavigationKey(event.key))
                        event.accepted = true
                }
            }
        }
    }

    Component.onCompleted: {
        updateDetailCounts()
        Qt.callLater(refreshDetailRows)
        Qt.callLater(focusDefaultAction)
    }

    onActiveFocusChanged: if (activeFocus) focusDefaultAction()
    onItemChanged: Qt.callLater(refreshDetailRows)

    Connections {
        target: appController
        function onDetailRowsChanged() { root.updateDetailCounts() }
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

    function focusDefaultAction() {
        if (showPrimaryAction && primaryAction.enabledButton)
            primaryAction.forceActiveFocus()
        else
            infoAction.forceActiveFocus()
    }

    function focusFirstContentRow() {
        if (showSeasonsRow) {
            seasonsRow.focusList()
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
        } else if (detailSource === "similar") {
            appController.playDetailSimilarItem(selectedIndex)
        } else {
            appController.playMovie(selectedIndex)
        }
    }

    function openMediaInfo() {
        if (shell)
            shell.openMediaInfo(item)
    }

    function openSimilarItem(index) {
        if (index < 0 || !appController)
            return
        appController.playDetailSimilarItem(index)
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
        if (seasonsRow.activeFocus) {
            if (key === Qt.Key_Up) {
                focusDefaultAction()
                return true
            }
            if (key === Qt.Key_Down) {
                if (showSimilarRow)
                    similarRow.focusList()
                return true
            }
            return seasonsRow.handleNavigationKey(key)
        }

        if (similarRow.activeFocus) {
            if (key === Qt.Key_Up) {
                if (showSeasonsRow) seasonsRow.focusList()
                else focusDefaultAction()
                return true
            }
            if (key === Qt.Key_Down)
                return true
            return similarRow.handleNavigationKey(key)
        }

        if (key === Qt.Key_Left) {
            if (infoAction.activeFocus && showPrimaryAction) primaryAction.forceActiveFocus()
            else shell.focusRail()
            return true
        }
        if (key === Qt.Key_Right) {
            if (primaryAction.activeFocus) infoAction.forceActiveFocus()
            return true
        }
        if (key === Qt.Key_Down) {
            focusFirstContentRow()
            return true
        }
        if (key === Qt.Key_Up) {
            focusDefaultAction()
            return true
        }
        if (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select) {
            if (infoAction.activeFocus) openMediaInfo()
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
                Layout.preferredHeight: Math.max(610, Math.round(root.height * 0.70))

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
                        color: "#CC151515"
                        border.width: 1
                        border.color: "#44FFFFFF"
                        antialiasing: true

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

                                DetailAction {
                                    id: infoAction
                                    iconName: "info"
                                    label: "Media info"
                                    enabledButton: root.selectedIndex >= 0
                                    onActivated: root.openMediaInfo()
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
                spacing: 28

                DetailPosterRow {
                    id: seasonsRow
                    title: "Seasons"
                    rowModel: appController ? appController.detailSeasons : null
                    cardWidth: root.rowPosterWidth
                    visible: root.showSeasonsRow
                    onActivated: (index) => appController.openDetailSeason(index)
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
