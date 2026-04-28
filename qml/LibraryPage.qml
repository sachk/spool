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
        anchors.margins: 54
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            GlowButton {
                text: "Back"
                implicitWidth: 130
                onClicked: appController.back()
            }

            Label {
                text: appController.currentLibraryName
                font.pixelSize: 48
                font.weight: Font.DemiBold
                color: "#f1fbff"
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            GlowButton {
                text: "Settings"
                implicitWidth: 165
                onClicked: appController.openSettings()
            }
        }

        Label {
            text: appController.currentContentLabel
            font.pixelSize: 21
            color: "#aac7d4"
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
                cellWidth: 232
                cellHeight: 382
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

            GlassPanel {
                id: detailPanel
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                width: libraryPage.sidePanelWidth
                radius: 28
                panelColor: "#8410202d"
                edgeColor: "#36576b"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 16

                    Label {
                        text: libraryPage.currentMovie ? libraryPage.currentMovie.title : "Select a movie"
                        wrapMode: Text.Wrap
                        font.pixelSize: 32
                        font.weight: Font.DemiBold
                        color: "#f3fbff"
                        Layout.fillWidth: true
                        maximumLineCount: 3
                        elide: Text.ElideRight
                    }

                    Rectangle {
                        implicitWidth: metaLabel.implicitWidth + 26
                        implicitHeight: 38
                        radius: 19
                        color: "#273947"
                        border.width: 1
                        border.color: "#3d5d70"
                        antialiasing: true

                        Label {
                            id: metaLabel
                            anchors.centerIn: parent
                            text: libraryPage.currentMovie && libraryPage.currentMovie.year > 0
                                  ? String(libraryPage.currentMovie.year)
                                  : (libraryPage.currentMovie && libraryPage.currentMovie.subtitle.length > 0
                                     ? libraryPage.currentMovie.subtitle
                                     : (libraryPage.currentMovie && libraryPage.currentMovie.playable ? "Direct Play" : "Open"))
                            font.pixelSize: 19
                            color: "#9edff0"
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: "#38586a"
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
                        color: "#d7e7f0"
                        font.pixelSize: 21
                        verticalAlignment: Text.AlignTop
                    }
                }
            }
        }
    }
}
