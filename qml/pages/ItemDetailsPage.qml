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
    readonly property int posterWidth: Math.min(360, Math.max(220, width * 0.21))
    focus: true

    component DetailAction: FocusScope {
        id: actionRoot
        property string iconName: "play_arrow"
        property string label: ""
        property bool primary: false
        property bool enabledButton: true
        signal activated()

        width: Math.min(Math.max(actionLabel.implicitWidth + 72, 150), 260)
        height: 50
        focus: true
        opacity: enabledButton ? 1.0 : 0.45

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusMedium
            color: actionRoot.primary ? Theme.accentPanel : Theme.bgPanel
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

    Component.onCompleted: Qt.callLater(focusDefaultAction)
    onActiveFocusChanged: if (activeFocus) focusDefaultAction()

    Keys.onReleased: (event) => {
        if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape || event.key === Qt.Key_Backspace) {
            shell.back()
            event.accepted = true
        }
    }

    function focusDefaultAction() {
        if (showPrimaryAction && primaryAction.enabledButton)
            primaryAction.forceActiveFocus()
        else
            infoAction.forceActiveFocus()
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
        } else {
            appController.playMovie(selectedIndex)
        }
    }

    function openMediaInfo() {
        if (shell)
            shell.openMediaInfo(item)
    }

    function primaryLabel() {
        return item.playActionLabel || "Play"
    }

    function primaryIcon() {
        return "play_arrow"
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

    function metadataParts() {
        const parts = []
        if (typeText.length > 0) parts.push(typeText)
        if (item.year && item.year > 0) parts.push(String(item.year))
        const runtime = runtimeText(item.runtimeTicks)
        if (runtime.length > 0) parts.push(runtime)
        if (subtitleText.length > 0) parts.push(subtitleText)
        return parts
    }

    function metadataLine() {
        return metadataParts().join(" · ")
    }

    function handleNavigationKey(key) {
        if (key === Qt.Key_Left) {
            if (infoAction.activeFocus && showPrimaryAction) primaryAction.forceActiveFocus()
            else shell.focusRail()
            return true
        }
        if (key === Qt.Key_Right) {
            if (primaryAction.activeFocus) infoAction.forceActiveFocus()
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
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Math.round(parent.height * 0.58)
        source: item.posterUrl || ""
        fillMode: Image.PreserveAspectCrop
        opacity: 0.20
        asynchronous: true
        cache: true
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Math.round(parent.height * 0.62)
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#22000000" }
            GradientStop { position: 0.50; color: "#B00E0E0E" }
            GradientStop { position: 1.0; color: Theme.bg }
        }
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: Math.max(height, contentColumn.implicitHeight + root.contentMargin)
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: contentColumn
            width: parent.width
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(500, Math.round(root.height * 0.72))

                RowLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: root.contentMargin
                    anchors.rightMargin: root.contentMargin
                    anchors.topMargin: Math.round(root.height * 0.08)
                    spacing: Math.round(root.width * 0.045)

                    ImageCard {
                        id: posterFrame
                        Layout.preferredWidth: root.posterWidth
                        Layout.preferredHeight: root.posterWidth * 1.5
                        Layout.maximumHeight: parent.height - 24
                        imageUrl: root.item.posterUrl || ""
                        fallbackText: root.typeText
                        focused: false
                        retainWhileLoading: true
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 12

                        AppText {
                            Layout.fillWidth: true
                            visible: root.parentText.length > 0
                            text: root.parentText
                            color: Theme.textSecondary
                            font.pixelSize: Metrics.bodyPx(root.width)
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }

                        AppText {
                            Layout.fillWidth: true
                            text: root.titleText
                            font.pixelSize: Math.min(58, Metrics.titlePx(root.width) + 14)
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        TechMetadataLine {
                            Layout.fillWidth: true
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
                            topPadding: 6

                            DetailAction {
                                id: primaryAction
                                iconName: root.primaryIcon()
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
                            lineHeight: 1.14
                            maximumLineCount: 6
                        }
                    }
                }
            }
        }
    }
}
