import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    property var shell
    readonly property int itemCount: appController.movies.rowCount()
    readonly property int selectedIndex: itemCount > 0 ? Math.max(0, Math.min(shell.lastGridIndex, itemCount - 1)) : -1
    property var item: selectedIndex >= 0 ? appController.movies.get(selectedIndex) : ({})
    focus: true
    onActiveFocusChanged: if (activeFocus) playButton.forceActiveFocus()
    Component.onCompleted: playButton.forceActiveFocus()

    Keys.onReleased: (event) => {
        if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape || event.key === Qt.Key_Backspace) {
            shell.back()
            event.accepted = true
        }
    }

    function activatePrimary() {
        if (selectedIndex < 0)
            return
        appController.playMovie(selectedIndex)
    }

    function handleNavigationKey(key) {
        if (key === Qt.Key_Left) { shell.focusRail(); return true }
        if (key === Qt.Key_Right) { moreButton.forceActiveFocus(); return true }
        if (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select) { activatePrimary(); return true }
        return false
    }

    Rectangle { anchors.fill: parent; color: Theme.bg }
    RowLayout { anchors.fill: parent; anchors.margins: Metrics.pageMargin(width); spacing: 28
        PosterCard { Layout.preferredWidth: Math.min(360, root.width * 0.2); title: item.title || item.name || "Selected item"; posterUrl: item.posterUrl || ""; year: item.year || 0; focused: true }
        ColumnLayout { Layout.fillWidth: true; Layout.fillHeight: true; spacing: 14
            AppText { text: item.title || "Selected item"; font.pixelSize: Metrics.titlePx(root.width); font.weight: Font.DemiBold; Layout.fillWidth: true; wrapMode: Text.Wrap; maximumLineCount: 2 }
            AppText { text: (item.year > 0 ? item.year + " · " : "") + "2h 21m · PG-13 · Sci-Fi · Adventure"; color: Theme.textSecondary; font.pixelSize: Metrics.bodyPx(root.width) }
            TechMetadataLine { Layout.fillWidth: true; metadata: item.subtitle || "Technical metadata unavailable" }
            MonoText { text: "Community 8.1 · Ends at 3:49 AM · Server: Jellyfin"; color: Theme.textMuted; font.pixelSize: Metrics.metaPx(root.width) }
            Row { spacing: 10
                ActionButton {
                    id: playButton
                    text: item.playActionLabel || "Play"
                    kind: "primary"
                    focus: true
                    KeyNavigation.right: moreButton
                    onClicked: root.activatePrimary()
                    Keys.onReleased: (event) => {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Select || event.key === Qt.Key_Space) {
                            root.activatePrimary()
                            event.accepted = true
                        } else if (event.key === Qt.Key_Left) {
                            shell.focusRail()
                            event.accepted = true
                        }
                    }
                }
                ActionButton { id: moreButton; text: "Media info"; KeyNavigation.left: playButton; KeyNavigation.right: trailerButton; onClicked: shell.openMediaInfo(item) }
                ActionButton { id: trailerButton; text: "Trailer"; KeyNavigation.left: moreButton }
            }
            AppText { Layout.fillWidth: true; text: item.overview && item.overview.length > 0 ? item.overview : "No overview is available from Jellyfin for this item."; wrapMode: Text.Wrap; color: Theme.textSecondary; font.pixelSize: Metrics.bodyPx(root.width); maximumLineCount: 7 }
            SectionHeader { Layout.fillWidth: true; title: "Cast" }
            Row { spacing: 10; Repeater { model: ["A. Rao · Captain", "M. Chen · Engineer", "S. Bell · Archivist"]; delegate: MetadataChip { required property string modelData; text: modelData } } }
            SectionHeader { Layout.fillWidth: true; title: "Related / Episodes" }
            ListView { Layout.fillWidth: true; Layout.fillHeight: true; orientation: ListView.Horizontal; spacing: Metrics.gap(root.width); model: appController.movies; delegate: Item { required property string title; required property string subtitle; required property string posterUrl; width: 300; height: 220; LandscapeCard { anchors.fill: parent; title: parent.title; subtitle: parent.subtitle; imageUrl: parent.posterUrl } } }
        }
    }
}
