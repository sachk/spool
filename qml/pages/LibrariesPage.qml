import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    property var shell
    focus: true
    function handleNavigationKey(key) {
        if (grid.count <= 0)
            return false
        const columns = Math.max(1, Math.floor(grid.width / Math.max(1, grid.cellWidth)))
        if (key === Qt.Key_Left) {
            if (grid.currentIndex % columns !== 0)
                grid.currentIndex = Math.max(0, grid.currentIndex - 1)
            return true
        }
        if (key === Qt.Key_Right) { grid.currentIndex = Math.min(grid.count - 1, grid.currentIndex + 1); return true }
        if (key === Qt.Key_Up) {
            if (grid.currentIndex < columns) shell.focusNavBar()
            else grid.currentIndex = Math.max(0, grid.currentIndex - columns)
            return true
        }
        if (key === Qt.Key_Down) { grid.currentIndex = Math.min(grid.count - 1, grid.currentIndex + columns); return true }
        if (InputKeys.isAccept(key, false)) {
            if (grid.currentIndex < 0)
                return true
            shell.lastLibraryIndex = grid.currentIndex
            appController.openLibrary(grid.currentIndex)
            shell.replaceRoute("libraryGrid")
            return true
        }
        return false
    }
    GridView {
        id: grid
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin(width)
        cellWidth: Math.floor((width - Metrics.gap(root.width) * 2) / 3)
        cellHeight: 160
        model: appController.libraries
        focus: true
        clip: true
        keyNavigationEnabled: false
        currentIndex: count > 0 ? Math.max(0, Math.min(shell.lastLibraryIndex, count - 1)) : -1
        onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, GridView.Contain)
        FastWheelHandler { flickable: grid }
        header: Item { width: grid.width; height: 158; ColumnLayout { anchors.fill: parent; spacing: 12; SectionHeader { Layout.fillWidth: true; title: "Libraries" } RowLayout { Layout.fillWidth: true; Repeater { model: ["Total Items", "Recently Added", "Server", "Active Users"]; delegate: MetadataChip { required property string modelData; text: modelData; Layout.preferredHeight: 34 } } } } }
        delegate: Surface {
            required property int index
            required property string name
            required property string collectionType
            required property string imageUrl
            width: grid.cellWidth - Metrics.gap(root.width)
            height: 132
            focused: GridView.isCurrentItem
            baseColor: Theme.bgRaised

            Image {
                anchors.fill: parent
                source: imageUrl
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: true
                sourceSize.width: Math.max(1, Math.round(width * Screen.devicePixelRatio))
                sourceSize.height: Math.max(1, Math.round(height * Screen.devicePixelRatio))
                opacity: status === Image.Ready ? 0.62 : 0
            }

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#33000000" }
                    GradientStop { position: 0.72; color: "#CC0E0E0E" }
                    GradientStop { position: 1.0; color: "#EE0E0E0E" }
                }
            }

            ColumnLayout { anchors.fill: parent; anchors.margins: 16; AppText { text: name; font.pixelSize: Metrics.titlePx(root.width) - 8; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight } MonoText { text: collectionType.length > 0 ? collectionType : "library" } Item { Layout.fillHeight: true } TechMetadataLine { Layout.fillWidth: true; metadata: "Artwork · Jellyfin library" } }
            MouseArea { anchors.fill: parent; onClicked: { grid.currentIndex = index; shell.lastLibraryIndex = index; appController.openLibrary(index); shell.replaceRoute("libraryGrid") } }
        }
        Keys.onReleased: (event) => {
            const columns = Math.max(1, Math.floor(width / Math.max(1, cellWidth)))
            if (event.key === Qt.Key_Up && currentIndex < columns) {
                shell.focusNavBar()
                event.accepted = true
            } else if (InputKeys.isAccept(event.key, false)) {
                shell.lastLibraryIndex = currentIndex
                appController.openLibrary(currentIndex)
                shell.replaceRoute("libraryGrid")
                event.accepted = true
            }
        }
    }
}
