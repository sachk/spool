import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import "../theme"

T.Control {
    id: root

    property string title: ""
    property string description: ""
    property var options: []
    property int currentIndex: 0
    signal selected(int index)

    focusPolicy: Qt.StrongFocus
    implicitHeight: Math.max(Metrics.scaled(78), labels.implicitHeight + Metrics.scaled(28))
    leftPadding: Metrics.scaled(14)
    rightPadding: Metrics.scaled(14)
    topPadding: Metrics.scaled(14)
    bottomPadding: Metrics.scaled(14)

    function move(direction) {
        if (options.length <= 0)
            return false
        const next = Math.max(0, Math.min(options.length - 1, currentIndex + direction))
        if (next !== currentIndex)
            selected(next)
        return true
    }

    background: Surface {
        focused: root.activeFocus
        hovered: hover.hovered
    }

    HoverHandler {
        id: hover
    }

    contentItem: RowLayout {
        spacing: Metrics.scaled(20)

        ColumnLayout {
            id: labels
            Layout.fillWidth: true
            spacing: Metrics.scaled(3)

            AppText {
                Layout.fillWidth: true
                text: root.title
                font.pixelSize: Metrics.bodySizePx
                font.weight: Font.DemiBold
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            AppText {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.description
                color: Theme.textMuted
                font.pixelSize: Metrics.metaSizePx
                maximumLineCount: 1
                elide: Text.ElideRight
            }
        }

        // One track holding a moving pill, rather than a row of separate
        // buttons whose borders would double up where they meet.
        Rectangle {
            id: track
            readonly property int inset: Math.max(2, Metrics.scaled(3))
            readonly property real cellWidth: root.options.length > 0 ? (width - inset * 2) / root.options.length : 0

            Layout.preferredWidth: Math.min(root.width * 0.5, Metrics.scaled(430))
            Layout.minimumWidth: Metrics.scaled(230)
            Layout.preferredHeight: Math.max(Metrics.controlHeightPx, Metrics.scaled(42))
            Layout.alignment: Qt.AlignVCenter
            radius: Theme.radiusMedium
            color: Theme.bgRaised
            border.width: Theme.hoverBorderWidth
            border.color: Theme.borderStrong
            antialiasing: true

            Rectangle {
                x: track.inset + track.cellWidth * root.currentIndex
                y: track.inset
                width: track.cellWidth
                height: track.height - track.inset * 2
                radius: Theme.radiusSmall
                color: root.activeFocus ? Theme.accent : Theme.accentPanel
                border.width: Theme.hoverBorderWidth
                border.color: Theme.accent
                antialiasing: true

                Behavior on x {
                    enabled: !Theme.reducedMotion
                    NumberAnimation {
                        duration: 130
                        easing.type: Easing.OutCubic
                    }
                }
            }

            Row {
                anchors.fill: parent
                anchors.margins: track.inset

                Repeater {
                    model: root.options

                    delegate: Item {
                        id: cell
                        required property int index
                        required property var modelData
                        readonly property bool chosen: index === root.currentIndex

                        width: track.cellWidth
                        height: track.height - track.inset * 2

                        AppText {
                            anchors.fill: parent
                            anchors.leftMargin: Metrics.scaled(6)
                            anchors.rightMargin: Metrics.scaled(6)
                            text: String(cell.modelData)
                            color: !cell.chosen ? Theme.textSecondary : root.activeFocus ? Theme.accentText :
                                                                                           Theme.textPrimary

                            font.pixelSize: Metrics.metaSizePx
                            font.weight: cell.chosen ? Font.DemiBold : Font.Medium
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        TapHandler {
                            onTapped: {
                                InputKeys.focus(root)
                                root.selected(cell.index)
                            }
                        }
                    }
                }
            }
        }
    }
}
