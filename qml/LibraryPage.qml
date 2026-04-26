import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

FocusScope {
    id: libraryPage
    focus: true

    onFocusChanged: {
        if (focus)
            movieGrid.forceActiveFocus()
    }

    property real sidePanelWidth: 340
    property real contentSpacing: 18
    property var currentMovie: appController.movies.rowCount() > 0 && movieGrid.currentIndex >= 0
                               ? appController.movies.get(movieGrid.currentIndex)
                               : null

    Keys.onReleased: (event) => {
        if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape || event.key === Qt.Key_BrowserBack) {
            appController.back()
            event.accepted = true
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 48
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            Button {
                text: "Back"
                onClicked: appController.back()
            }

            Label {
                text: appController.currentLibraryName
                font.pixelSize: 42
                font.weight: Font.DemiBold
                color: "#f1fbff"
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            Button {
                text: "Settings"
                onClicked: appController.openSettings()
            }
        }

        Label {
            text: appController.currentContentLabel
            font.pixelSize: 19
            color: "#8ec7de"
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            GridView {
                id: movieGrid
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: detailPanel.left
                anchors.rightMargin: libraryPage.contentSpacing
                model: appController.movies
                cellWidth: 222
                cellHeight: 368
                clip: true
                keyNavigationWraps: true
                currentIndex: -1
                focus: true
                interactive: true
                reuseItems: true
                boundsBehavior: Flickable.StopAtBounds
                cacheBuffer: 1500
                highlightRangeMode: GridView.NoHighlightRange
                delegate: Item {
                    id: movieDelegate
                    required property int index
                    required property string title
                    required property string overview
                    required property string posterUrl
                    required property string posterTag
                    required property int year
                    required property string itemType
                    required property string subtitle
                    required property bool playable

                    width: movieGrid.cellWidth
                    height: movieGrid.cellHeight

                    PosterCard {
                        anchors.fill: parent
                        anchors.margins: 4
                        title: movieDelegate.title
                        posterUrl: movieDelegate.posterUrl
                        year: movieDelegate.year
                        selected: movieDelegate.GridView.isCurrentItem
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            movieGrid.forceActiveFocus()
                            movieGrid.currentIndex = movieDelegate.index
                            appController.playMovie(movieDelegate.index)
                        }
                    }
                }

                onCountChanged: {
                    if (count > 0) {
                        currentIndex = 0
                        contentX = 0
                        contentY = 0
                        positionViewAtBeginning()
                    } else {
                        currentIndex = -1
                    }
                }

                Keys.onReleased: (event) => {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        appController.playMovie(currentIndex)
                        event.accepted = true
                    }
                }
            }

            Rectangle {
                id: detailPanel
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                width: libraryPage.sidePanelWidth
                radius: 28
                color: "#4b10202d"
                border.color: "#2a5a73"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 14

                    Label {
                        text: libraryPage.currentMovie ? libraryPage.currentMovie.title : "Select a movie"
                        wrapMode: Text.Wrap
                        font.pixelSize: 30
                        font.weight: Font.DemiBold
                        color: "#f3fbff"
                        Layout.fillWidth: true
                        maximumLineCount: 3
                        elide: Text.ElideRight
                    }

                    Label {
                        text: libraryPage.currentMovie && libraryPage.currentMovie.year > 0
                              ? String(libraryPage.currentMovie.year)
                              : (libraryPage.currentMovie && libraryPage.currentMovie.subtitle.length > 0
                                 ? libraryPage.currentMovie.subtitle
                                 : (libraryPage.currentMovie && libraryPage.currentMovie.playable ? "Direct Play" : "Open"))
                        font.pixelSize: 19
                        color: "#82daf5"
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: "#2a5a73"
                        opacity: 0.7
                    }

                    Label {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: libraryPage.currentMovie && libraryPage.currentMovie.overview.length > 0
                              ? libraryPage.currentMovie.overview
                              : (libraryPage.currentMovie && libraryPage.currentMovie.playable
                                 ? "Select an item to begin playback."
                                 : "Select an item to open it.")
                        wrapMode: Text.Wrap
                        color: "#d8eaf3"
                        font.pixelSize: 21
                        verticalAlignment: Text.AlignTop
                    }
                }
            }
        }
    }
}
