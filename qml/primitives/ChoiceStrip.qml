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

        RowLayout {
            Layout.preferredWidth: Math.min(root.width * 0.54, Metrics.scaled(520))
            Layout.minimumWidth: Metrics.scaled(270)
            spacing: Metrics.scaled(4)

            Repeater {
                model: root.options

                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    readonly property bool chosen: index === root.currentIndex
                    Layout.fillWidth: true
                    Layout.minimumWidth: Metrics.scaled(82)
                    implicitHeight: Math.max(Metrics.controlHeightPx, Metrics.scaled(40))
                    radius: Theme.radiusMedium
                    color: chosen ? Theme.accentPanel : Theme.bgRaised
                    border.width: chosen ? Theme.focusBorderWidth : Theme.hoverBorderWidth
                    border.color: chosen ? Theme.accent : Theme.borderStrong
                    antialiasing: true

                    AppText {
                        anchors.fill: parent
                        anchors.leftMargin: Metrics.scaled(10)
                        anchors.rightMargin: Metrics.scaled(10)
                        text: String(modelData)
                        color: parent.chosen ? Theme.textPrimary : Theme.textSecondary
                        font.pixelSize: Metrics.metaSizePx
                        font.weight: parent.chosen ? Font.DemiBold : Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    TapHandler {
                        onTapped: {
                            InputKeys.focus(root)
                            root.selected(index)
                        }
                    }
                }
            }
        }
    }
}
