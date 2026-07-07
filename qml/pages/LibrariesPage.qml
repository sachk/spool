import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    property var shell
    focus: true
    function openLibraryIndex(index) {
        if (index < 0)
            return
        shell.lastLibraryIndex = index
        appController.openLibrary(index)
        shell.replaceRoute("libraryGrid")
    }

    function handleKey(key) {
        return grid.handleKey(key)
    }
    NavGrid {
        id: grid
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin(width)
        cellWidth: Math.floor((width - Metrics.gap(root.width) * 2) / 3)
        cellHeight: 160
        model: appController.libraries
        clip: true
        currentIndex: count > 0 ? Math.max(0, Math.min(shell.lastLibraryIndex, count - 1)) : -1
        onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, GridView.Contain)
        FastWheelHandler { flickable: grid }
        onEdgeUp: shell.focusNavBar()
        onAccepted: index => root.openLibraryIndex(index)
        header: Item { width: grid.width; height: 158; ColumnLayout { anchors.fill: parent; spacing: 12; SectionHeader { Layout.fillWidth: true; title: "Libraries" } RowLayout { Layout.fillWidth: true; Repeater { model: ["Total Items", "Recently Added", "Server", "Active Users"]; delegate: MetadataChip { required property string modelData; text: modelData; Layout.preferredHeight: 34 } } } } }
        delegate: Surface {
            id: libraryDelegate
            required property int index
            required property string name
            required property string collectionType
            required property string imageUrl
            width: grid.cellWidth - Metrics.gap(root.width)
            height: 132
            focused: GridView.isCurrentItem
            baseColor: Theme.bgRaised

            function activate() {
                grid.currentIndex = index
                root.openLibraryIndex(index)
            }

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
            MouseArea { anchors.fill: parent; onClicked: libraryDelegate.activate() }
        }
    }
}
