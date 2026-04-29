import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    property var shell
    property int columns: Metrics.columns(width)
    focus: true
    Component.onCompleted: grid.forceActiveFocus()
    onActiveFocusChanged: if (activeFocus) grid.forceActiveFocus()

    function activateCurrent() {
        if (grid.currentIndex < 0)
            return
        shell.lastGridIndex = grid.currentIndex
        const item = appController.movies.get(grid.currentIndex)
        const t = item ? item.itemType : ""
        if (t === "Series" || t === "Season") {
            // Direct navigation: drill into seasons/episodes immediately.
            appController.playMovie(grid.currentIndex)
        } else {
            shell.pushRoute("itemDetails")
        }
    }

    function handleNavigationKey(key) {
        if (grid.count <= 0)
            return false
        if (key === Qt.Key_Left) {
            if (grid.currentIndex % columns === 0) shell.focusRail()
            else grid.currentIndex = Math.max(0, grid.currentIndex - 1)
            return true
        }
        if (key === Qt.Key_Right) {
            grid.currentIndex = Math.min(grid.count - 1, grid.currentIndex + 1)
            return true
        }
        if (key === Qt.Key_Up) {
            grid.currentIndex = Math.max(0, grid.currentIndex - columns)
            return true
        }
        if (key === Qt.Key_Down) {
            grid.currentIndex = Math.min(grid.count - 1, grid.currentIndex + columns)
            return true
        }
        if (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select) {
            activateCurrent()
            return true
        }
        return false
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin(width)
        spacing: 12
        SectionHeader { Layout.fillWidth: true; title: appController.currentLibraryName.length > 0 ? appController.currentLibraryName : "Movies"; detail: "Sort: Recently Added · Filter: All · View: Posters · Columns: " + columns }
        TechMetadataLine { Layout.fillWidth: true; metadata: grid.currentIndex >= 0 ? "Title · Year · Runtime · Rating · H.265 · HDR10 · DTS-HD MA 5.1" : "Technical metadata unavailable" }
        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: true
            clip: true
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
            model: appController.movies
            cellWidth: Math.floor((width - Metrics.gap(root.width) * (columns - 1)) / columns)
            cellHeight: cellWidth * 1.5 + 64
            Component.onCompleted: restoreIndex()
            onCountChanged: restoreIndex()

            function restoreIndex() {
                currentIndex = count > 0 ? Math.max(0, Math.min(shell.lastGridIndex, count - 1)) : -1
            }

            delegate: Item {
                required property int index
                required property string title
                required property string posterUrl
                required property int year
                required property string subtitle
                width: grid.cellWidth
                height: grid.cellHeight
                PosterCard { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.rightMargin: Metrics.gap(root.width); title: parent.title; posterUrl: parent.posterUrl; year: parent.year; metadata: parent.subtitle; focused: parent.GridView.isCurrentItem }
                MouseArea { anchors.fill: parent; onClicked: { grid.currentIndex = index; shell.lastGridIndex = index; shell.lastGridY = grid.contentY; root.activateCurrent() } }
            }
            onCurrentIndexChanged: shell.lastGridIndex = currentIndex
            Keys.onReleased: (event) => {
                if (event.key === Qt.Key_Left && currentIndex % columns === 0) { shell.focusRail(); event.accepted = true }
                else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Select) { root.activateCurrent(); event.accepted = true }
                else if (event.key === Qt.Key_Space) { appController.playMovie(currentIndex); event.accepted = true }
                else if (event.key === Qt.Key_M) { shell.lastGridIndex = currentIndex; shell.openMediaInfo(appController.movies.get(currentIndex)); event.accepted = true }
            }
        }
    }
}
