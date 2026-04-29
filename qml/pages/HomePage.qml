import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    property var shell
    readonly property int itemCount: appController.movies.rowCount()
    readonly property int spotlightIndex: itemCount > 0 ? Math.max(0, Math.min(shell.lastGridIndex, itemCount - 1)) : -1
    property var spotlight: spotlightIndex >= 0 ? appController.movies.get(spotlightIndex) : ({})
    focus: true

    function handleNavigationKey(key) {
        if (sections.currentItem && sections.currentItem.handleNavigationKey)
            return sections.currentItem.handleNavigationKey(key)
        if (key === Qt.Key_Up) {
            sections.currentIndex = Math.max(0, sections.currentIndex - 1)
            return true
        }
        if (key === Qt.Key_Down) {
            sections.currentIndex = Math.min(sections.count - 1, sections.currentIndex + 1)
            return true
        }
        return false
    }

    Component.onCompleted: sections.forceActiveFocus()
    onActiveFocusChanged: if (activeFocus) sections.forceActiveFocus()

    ListView {
        id: sections
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin(width)
        focus: true
        clip: true
        spacing: 22
        model: [
            { title: "Spotlight", kind: "spotlight" },
            { title: "Continue Watching", kind: "landscape" },
            { title: "Next Up", kind: "landscape" },
            { title: "Latest Media", kind: "poster" },
            { title: "Recently Added Movies", kind: "poster" },
            { title: "Libraries / Collections", kind: "landscape" }
        ]

        header: SectionHeader { width: sections.width; height: implicitHeight + 18; title: "Home"; detail: "Static spotlight · D-pad rows" }

        delegate: FocusScope {
            id: section
            required property int index
            required property var modelData
            width: sections.width
            height: modelData.kind === "spotlight" ? 238 : modelData.kind === "poster" ? 360 : 240
            focus: ListView.isCurrentItem
            onActiveFocusChanged: if (activeFocus && contentLoader.item) contentLoader.item.forceActiveFocus()

            function handleNavigationKey(key) {
                if (contentLoader.item && contentLoader.item.handleNavigationKey)
                    return contentLoader.item.handleNavigationKey(key)
                return false
            }

            Keys.onReleased: (event) => {
                if (event.key === Qt.Key_Up) {
                    sections.currentIndex = Math.max(0, sections.currentIndex - 1)
                    sections.forceActiveFocus()
                    event.accepted = true
                } else if (event.key === Qt.Key_Down) {
                    sections.currentIndex = Math.min(sections.count - 1, sections.currentIndex + 1)
                    sections.forceActiveFocus()
                    event.accepted = true
                }
            }

            Loader {
                id: contentLoader
                anchors.fill: parent
                sourceComponent: section.modelData.kind === "spotlight" ? spotlightComponent : rowComponent
                onLoaded: if (section.ListView.isCurrentItem) item.forceActiveFocus()
            }

            Component {
                id: spotlightComponent
                Surface {
                    focus: true
                    focused: section.ListView.isCurrentItem
                    function handleNavigationKey(key) {
                        if (key === Qt.Key_Left) {
                            shell.focusRail()
                            return true
                        }
                        if (key === Qt.Key_Up) {
                            sections.currentIndex = Math.max(0, sections.currentIndex - 1)
                            return true
                        }
                        if (key === Qt.Key_Down) {
                            sections.currentIndex = Math.min(sections.count - 1, sections.currentIndex + 1)
                            return true
                        }
                        if ((key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select) && root.spotlightIndex >= 0) {
                            appController.playMovie(root.spotlightIndex)
                            return true
                        }
                        return false
                    }
                    baseColor: Theme.bgRaised
                    RowLayout { anchors.fill: parent; anchors.margins: 20; spacing: 22
                        ImageCard { Layout.preferredWidth: 300; Layout.fillHeight: true; imageUrl: root.spotlight.posterUrl || ""; aspectRatio: 16/9; focused: section.ListView.isCurrentItem; fallbackText: "Spotlight"; retainWhileLoading: true }
                        ColumnLayout { Layout.fillWidth: true; spacing: 8
                            AppText { text: root.spotlight.title || appController.currentLibraryName || "Jellyfin"; font.pixelSize: Math.min(34, Metrics.titlePx(root.width)); font.weight: Font.DemiBold }
                            AppText { text: root.spotlight.year > 0 ? String(root.spotlight.year) : "Select a library"; color: Theme.textSecondary; font.pixelSize: Metrics.bodyPx(root.width) }
                            TechMetadataLine { Layout.fillWidth: true; metadata: root.spotlight.subtitle || "Technical metadata unavailable" }
                            AppText { Layout.fillWidth: true; text: root.spotlight.overview || "Select a library to load Jellyfin media."; color: Theme.textSecondary; wrapMode: Text.Wrap; maximumLineCount: 3 }
                            Row { spacing: 10
                                ActionButton { id: spotlightPlay; text: root.spotlight.playActionLabel || "Play"; kind: "primary"; onClicked: if (root.spotlightIndex >= 0) appController.playMovie(root.spotlightIndex) }
                                ActionButton { text: "More..."; onClicked: shell.openContextMenu() }
                            }
                        }
                    }
                    Keys.onReleased: (event) => {
                        if (event.key === Qt.Key_Left) {
                            shell.focusRail()
                            event.accepted = true
                        } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Select || event.key === Qt.Key_Space) && root.spotlightIndex >= 0) {
                            appController.playMovie(root.spotlightIndex)
                            event.accepted = true
                        }
                    }
                }
            }

            Component {
                id: rowComponent
                FocusScope {
                    id: rowScope
                    focus: section.ListView.isCurrentItem
                    onActiveFocusChanged: if (activeFocus) rowList.forceActiveFocus()
                    function handleNavigationKey(key) {
                        if (key === Qt.Key_Left) {
                            if (rowList.count <= 0 || rowList.currentIndex <= 0) shell.focusRail()
                            else rowList.currentIndex = rowList.currentIndex - 1
                            shell.lastGridIndex = rowList.currentIndex
                            return true
                        }
                        if (key === Qt.Key_Right) {
                            if (rowList.count <= 0)
                                return true
                            rowList.currentIndex = Math.min(rowList.count - 1, rowList.currentIndex + 1)
                            shell.lastGridIndex = rowList.currentIndex
                            return true
                        }
                        if (key === Qt.Key_Up) {
                            sections.currentIndex = Math.max(0, sections.currentIndex - 1)
                            return true
                        }
                        if (key === Qt.Key_Down) {
                            sections.currentIndex = Math.min(sections.count - 1, sections.currentIndex + 1)
                            return true
                        }
                        if (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select) {
                            if (rowList.currentIndex < 0)
                                return true
                            shell.lastGridIndex = rowList.currentIndex
                            shell.pushRoute("itemDetails")
                            return true
                        }
                        return false
                    }
                    ColumnLayout { anchors.fill: parent; spacing: 10
                        SectionHeader { Layout.fillWidth: true; title: section.modelData.title; detail: section.modelData.kind === "poster" ? "posters" : "landscape" }
                        ListView {
                            id: rowList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            focus: rowScope.focus
                            orientation: ListView.Horizontal
                            spacing: Metrics.gap(root.width)
                            clip: true
                            model: appController.movies
                            currentIndex: Math.max(0, Math.min(shell.lastGridIndex, count - 1))
                            delegate: Item {
                                required property int index
                                required property string title
                                required property string posterUrl
                                required property int year
                                required property string subtitle
                                width: section.modelData.kind === "poster" ? Metrics.basePosterWidth(root.width) : 330
                                height: rowList.height
                                Loader {
                                    anchors.fill: parent
                                    sourceComponent: section.modelData.kind === "poster" ? posterComponent : landscapeComponent
                                    onLoaded: {
                                        item.title = title
                                        item.posterUrl = posterUrl
                                        item.year = year
                                        item.subtitle = subtitle
                                        item.focused = Qt.binding(function() { return index === rowList.currentIndex && section.ListView.isCurrentItem })
                                    }
                                }
                                MouseArea { anchors.fill: parent; onClicked: { rowList.currentIndex = index; shell.lastGridIndex = index; shell.pushRoute("itemDetails") } }
                            }
                            Keys.onReleased: (event) => {
                        if (event.key === Qt.Key_Left) {
                                    if (currentIndex <= 0) {
                                        shell.focusRail()
                                    } else {
                                        currentIndex = currentIndex - 1
                                    }
                                    shell.lastGridIndex = currentIndex
                                    event.accepted = true
                                } else if (event.key === Qt.Key_Right) {
                                    currentIndex = Math.min(count - 1, currentIndex + 1)
                                    shell.lastGridIndex = currentIndex
                                    event.accepted = true
                                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Select) {
                                    shell.lastGridIndex = currentIndex
                                    shell.pushRoute("itemDetails")
                                    event.accepted = true
                                } else if (event.key === Qt.Key_Space) {
                                    appController.playMovie(currentIndex)
                                    event.accepted = true
                                }
                            }
                        }
                    }
                }
            }
        }

        Keys.onReleased: (event) => {
            if (event.key === Qt.Key_Up) {
                currentIndex = Math.max(0, currentIndex - 1)
                event.accepted = true
            } else if (event.key === Qt.Key_Down) {
                currentIndex = Math.min(count - 1, currentIndex + 1)
                event.accepted = true
            }
        }
    }

    Component { id: posterComponent; PosterCard { property string subtitle: ""; metadata: subtitle } }
    Component { id: landscapeComponent; LandscapeCard { property string posterUrl: ""; property int year: 0; property string subtitle: ""; imageUrl: posterUrl } }
}
