import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    property var shell
    readonly property int itemCount: appController.movies.rowCount()
    readonly property int selectedIndex: itemCount > 0 ? Math.max(0, Math.min(shell.lastGridIndex, itemCount - 1)) : -1
    readonly property var item: selectedIndex >= 0 ? appController.movies.get(selectedIndex) : ({})
    readonly property string titleText: item.displayTitle || item.title || item.seriesName || "Selected item"
    readonly property string parentText: item.itemType === "Episode" && item.seriesName ? item.seriesName : ""
    readonly property string typeText: item.itemType || "Media"
    readonly property string subtitleText: item.displaySubtitle || item.subtitle || ""
    readonly property bool canPlay: typeText === "Series" || typeText === "Season" || item.playable === undefined || item.playable
    readonly property int posterWidth: Math.min(420, Math.max(260, width * 0.25))
    focus: true

    component DetailIconButton: FocusScope {
        id: buttonRoot
        property string iconName: "play_arrow"
        property string label: ""
        property bool current: false
        property bool enabledButton: true
        signal activated()

        width: Math.round(96 * root.scaleFactor())
        height: Math.round(104 * root.scaleFactor())
        focus: true
        opacity: enabledButton ? 1.0 : 0.45

        Rectangle {
            id: ring
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            width: Math.round(62 * root.scaleFactor())
            height: width
            radius: width / 2
            color: buttonRoot.activeFocus || buttonRoot.current ? "#2600A4DC" : "transparent"
            border.width: buttonRoot.activeFocus ? 3 : buttonRoot.current ? 2 : 0
            border.color: buttonRoot.activeFocus ? "#EAF8FF" : Theme.accent

            MaterialIcon {
                anchors.centerIn: parent
                name: buttonRoot.iconName
                iconSize: Math.round(34 * root.scaleFactor())
                iconColor: buttonRoot.activeFocus ? "#FFFFFF" : Theme.textPrimary
            }
        }

        AppText {
            anchors.top: ring.bottom
            anchors.topMargin: 9
            anchors.left: parent.left
            anchors.right: parent.right
            text: buttonRoot.label
            color: buttonRoot.activeFocus ? Theme.textPrimary : Theme.textSecondary
            font.pixelSize: Metrics.metaPx(root.width)
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        MouseArea {
            anchors.fill: parent
            enabled: buttonRoot.enabledButton
            onClicked: buttonRoot.activated()
        }

        Keys.onReleased: (event) => {
            if (!buttonRoot.enabledButton)
                return
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Select || event.key === Qt.Key_Space) {
                buttonRoot.activated()
                event.accepted = true
            }
        }
    }

    Component.onCompleted: playButton.forceActiveFocus()
    onActiveFocusChanged: if (activeFocus) playButton.forceActiveFocus()

    Keys.onReleased: (event) => {
        if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape || event.key === Qt.Key_Backspace) {
            shell.back()
            event.accepted = true
        }
    }

    function scaleFactor() {
        return Math.max(0.82, Math.min(1.12, height / 1080))
    }

    function activatePrimary() {
        if (selectedIndex < 0 || !canPlay)
            return
        appController.playMovie(selectedIndex)
    }

    function openMediaInfo() {
        if (shell)
            shell.openMediaInfo(item)
    }

    function primaryLabel() {
        if (typeText === "Series") return "Seasons"
        if (typeText === "Season") return "Episodes"
        return item.playActionLabel || "Play"
    }

    function primaryIcon() {
        if (typeText === "Series" || typeText === "Season") return "list"
        return item.resumeTicks && item.resumeTicks > 0 ? "play_arrow" : "play_arrow"
    }

    function runtimeText(ticks) {
        if (!ticks || ticks <= 0)
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

    function primaryMisc() {
        const parts = []
        if (item.year && item.year > 0) parts.push(String(item.year))
        const runtime = runtimeText(item.runtimeTicks)
        if (runtime.length > 0) parts.push(runtime)
        if (typeText.length > 0) parts.push(typeText)
        return parts.join("  /  ")
    }

    function secondaryMisc() {
        const parts = []
        if (subtitleText.length > 0) parts.push(subtitleText)
        const progress = progressText()
        if (progress.length > 0) parts.push(progress)
        return parts.join("  /  ")
    }

    function handleNavigationKey(key) {
        if (key === Qt.Key_Left) {
            if (moreButton.activeFocus) infoButton.forceActiveFocus()
            else if (infoButton.activeFocus) replayButton.forceActiveFocus()
            else if (replayButton.activeFocus) playButton.forceActiveFocus()
            else shell.focusRail()
            return true
        }
        if (key === Qt.Key_Right) {
            if (playButton.activeFocus) replayButton.forceActiveFocus()
            else if (replayButton.activeFocus) infoButton.forceActiveFocus()
            else if (infoButton.activeFocus) moreButton.forceActiveFocus()
            return true
        }
        if (key === Qt.Key_Down) {
            relatedList.forceActiveFocus()
            return true
        }
        if (key === Qt.Key_Up) {
            playButton.forceActiveFocus()
            return true
        }
        if (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select) {
            if (infoButton.activeFocus || moreButton.activeFocus) openMediaInfo()
            else activatePrimary()
            return true
        }
        return false
    }

    Rectangle { anchors.fill: parent; color: Theme.bg }

    Image {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Math.round(parent.height * 0.42)
        source: item.posterUrl || ""
        fillMode: Image.PreserveAspectCrop
        opacity: 0.18
        asynchronous: true
        cache: true
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Math.round(parent.height * 0.46)
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#33000000" }
            GradientStop { position: 0.55; color: "#A80E0E0E" }
            GradientStop { position: 1.0; color: Theme.bg }
        }
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: Math.max(height, detailsColumn.implicitHeight + Metrics.pageMargin(root.width))
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: detailsColumn
            width: parent.width
            spacing: Math.round(22 * root.scaleFactor())

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.round(root.height * 0.53)

                PosterCard {
                    id: poster
                    anchors.left: parent.left
                    anchors.leftMargin: Math.round(root.width * 0.05)
                    anchors.top: parent.top
                    anchors.topMargin: Math.round(root.height * 0.11)
                    width: root.posterWidth
                    title: root.item.title || root.titleText
                    posterUrl: root.item.posterUrl || ""
                    year: root.item.year || 0
                    metadata: root.typeText
                    focused: false
                }

                ColumnLayout {
                    anchors.left: poster.right
                    anchors.leftMargin: Math.round(root.width * 0.07)
                    anchors.right: parent.right
                    anchors.rightMargin: Metrics.pageMargin(root.width)
                    anchors.top: poster.top
                    spacing: Math.round(11 * root.scaleFactor())

                    AppText {
                        Layout.fillWidth: true
                        visible: root.parentText.length > 0
                        text: root.parentText
                        color: Theme.textSecondary
                        font.pixelSize: Metrics.bodyPx(root.width) + 2
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                    }

                    AppText {
                        Layout.fillWidth: true
                        text: root.item.title || root.titleText
                        font.pixelSize: Math.min(64, Metrics.titlePx(root.width) + 18)
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }

                    AppText {
                        Layout.fillWidth: true
                        visible: root.primaryMisc().length > 0
                        text: root.primaryMisc()
                        color: Theme.textSecondary
                        font.pixelSize: Metrics.bodyPx(root.width)
                        elide: Text.ElideRight
                    }

                    AppText {
                        Layout.fillWidth: true
                        visible: root.secondaryMisc().length > 0
                        text: root.secondaryMisc()
                        color: Theme.textSecondary
                        font.pixelSize: Metrics.bodyPx(root.width)
                        elide: Text.ElideRight
                    }

                    Row {
                        id: actionRow
                        spacing: Math.round(8 * root.scaleFactor())
                        topPadding: Math.round(6 * root.scaleFactor())

                        DetailIconButton {
                            id: playButton
                            iconName: root.primaryIcon()
                            label: root.primaryLabel()
                            enabledButton: root.selectedIndex >= 0 && root.canPlay
                            onActivated: root.activatePrimary()
                        }

                        DetailIconButton {
                            id: replayButton
                            iconName: "replay"
                            label: "Play"
                            enabledButton: root.selectedIndex >= 0 && root.canPlay && root.item.resumeTicks > 0
                            onActivated: root.activatePrimary()
                        }

                        DetailIconButton {
                            id: infoButton
                            iconName: "info"
                            label: "Media info"
                            onActivated: root.openMediaInfo()
                        }

                        DetailIconButton {
                            id: moreButton
                            iconName: "more_vert"
                            label: "More"
                            onActivated: root.openMediaInfo()
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Math.round(root.width * 0.05) + root.posterWidth + Math.round(root.width * 0.07)
                Layout.rightMargin: Metrics.pageMargin(root.width)
                spacing: 0

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Math.round(14 * root.scaleFactor())

                    AppText {
                        Layout.fillWidth: true
                        visible: root.item.overview && root.item.overview.length > 0
                        text: root.item.overview || ""
                        color: Theme.textSecondary
                        wrapMode: Text.Wrap
                        font.pixelSize: Metrics.bodyPx(root.width)
                        lineHeight: 1.16
                        maximumLineCount: 8
                    }

                    Surface {
                        Layout.fillWidth: true
                        Layout.preferredHeight: detailsText.implicitHeight + 28
                        baseColor: "#141414"
                        border.color: Theme.border
                        visible: root.item.path && root.item.path.length > 0

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 4
                            MonoText { text: "File"; color: Theme.textMuted; font.pixelSize: Metrics.metaPx(root.width) }
                            MonoText {
                                id: detailsText
                                Layout.fillWidth: true
                                text: root.item.path || ""
                                color: Theme.textSecondary
                                elide: Text.ElideMiddle
                                maximumLineCount: 1
                            }
                        }
                    }
                }
            }

            SectionHeader {
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.pageMargin(root.width)
                Layout.rightMargin: Metrics.pageMargin(root.width)
                title: root.typeText === "Episode" ? "More From This Season" : "More Like This"
            }

            ListView {
                id: relatedList
                Layout.fillWidth: true
                Layout.preferredHeight: Math.round(250 * root.scaleFactor())
                Layout.leftMargin: Metrics.pageMargin(root.width)
                Layout.rightMargin: Metrics.pageMargin(root.width)
                focus: true
                orientation: ListView.Horizontal
                spacing: Metrics.gap(root.width)
                clip: true
                keyNavigationEnabled: false
                model: appController.movies
                currentIndex: root.selectedIndex

                function activateCurrent() {
                    if (currentIndex < 0)
                        return
                    shell.lastGridIndex = currentIndex
                    appController.playMovie(currentIndex)
                }

                delegate: Item {
                    required property int index
                    required property string title
                    required property string displayTitle
                    required property string displaySubtitle
                    required property string subtitle
                    required property string posterUrl
                    required property double progress
                    width: Metrics.homeLandscapeWidth(root.width)
                    height: relatedList.height

                    LandscapeCard {
                        anchors.fill: parent
                        title: parent.displayTitle || parent.title
                        subtitle: parent.displaySubtitle || parent.subtitle
                        imageUrl: parent.posterUrl
                        progress: parent.progress || 0
                        focused: parent.ListView.isCurrentItem && relatedList.activeFocus
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            relatedList.currentIndex = index
                            shell.lastGridIndex = index
                        }
                        onDoubleClicked: relatedList.activateCurrent()
                    }
                }

                Keys.onReleased: (event) => {
                    if (event.key === Qt.Key_Left) {
                        if (currentIndex <= 0) shell.focusRail()
                        else currentIndex = currentIndex - 1
                        shell.lastGridIndex = currentIndex
                        event.accepted = true
                    } else if (event.key === Qt.Key_Right) {
                        currentIndex = Math.min(count - 1, currentIndex + 1)
                        shell.lastGridIndex = currentIndex
                        event.accepted = true
                    } else if (event.key === Qt.Key_Up) {
                        playButton.forceActiveFocus()
                        event.accepted = true
                    } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Select || event.key === Qt.Key_Space) {
                        activateCurrent()
                        event.accepted = true
                    }
                }
            }
        }
    }
}
