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

    property real sidePanelWidth: 430
    property real contentSpacing: 28
    property var currentMovie: appController.movies.rowCount() > 0 && movieGrid.currentIndex >= 0
                               ? appController.movies.get(movieGrid.currentIndex)
                               : null

    Keys.onReleased: (event) => {
        if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape) {
            appController.back()
            event.accepted = true
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 70
        spacing: 20

        RowLayout {
            Layout.fillWidth: true
            spacing: 18

            Button {
                text: "Back"
                onClicked: appController.back()
            }

            Label {
                text: appController.currentLibraryName
                font.pixelSize: 48
                font.weight: Font.DemiBold
                color: "#f1fbff"
            }
        }

        Label {
            text: "Movies only in this prototype"
            font.pixelSize: 22
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
                cellWidth: 250
                cellHeight: 430
                clip: true
                keyNavigationWraps: true
                currentIndex: -1
                focus: true
                interactive: true
                reuseItems: true
                boundsBehavior: Flickable.StopAtBounds
                cacheBuffer: 1200
                highlightRangeMode: GridView.NoHighlightRange
                delegate: Item {
                    id: movieDelegate
                    required property int index
                    required property string title
                    required property string overview
                    required property string posterUrl
                    required property int year

                    width: movieGrid.cellWidth
                    height: movieGrid.cellHeight

                    PosterCard {
                        anchors.fill: parent
                        anchors.margins: 5
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
                radius: 32
                color: "#4b10202d"
                border.color: "#2a5a73"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 18

                    Label {
                        text: libraryPage.currentMovie ? libraryPage.currentMovie.title : "Select a movie"
                        wrapMode: Text.Wrap
                        font.pixelSize: 36
                        font.weight: Font.DemiBold
                        color: "#f3fbff"
                        Layout.fillWidth: true
                    }

                    Label {
                        text: libraryPage.currentMovie && libraryPage.currentMovie.year > 0
                              ? String(libraryPage.currentMovie.year)
                              : "Direct Play"
                        font.pixelSize: 22
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
                              : "Select a movie to begin playback."
                        wrapMode: Text.Wrap
                        color: "#d8eaf3"
                        font.pixelSize: 24
                        verticalAlignment: Text.AlignTop
                    }
                }
            }
        }
    }
}
