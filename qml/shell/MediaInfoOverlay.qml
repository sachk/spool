import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    property var item: ({})
    signal closed()

    focus: visible
    onActiveFocusChanged: if (visible && !activeFocus) forceActiveFocus()
    onVisibleChanged: if (visible) forceActiveFocus()

    Keys.onReleased: (event) => {
        if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape ||
                event.key === Qt.Key_Backspace || event.key === Qt.Key_BrowserBack ||
                event.key === Qt.Key_I) {
            root.closed()
            event.accepted = true
        }
    }

    Rectangle { anchors.fill: parent; color: "#CC000000" }
    MouseArea { anchors.fill: parent; onClicked: root.closed() }

    Surface {
        anchors.centerIn: parent
        width: Math.min(parent.width - 160, 980)
        height: Math.min(parent.height - 160, 640)
        baseColor: Theme.bgRaised
        elevated: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 28
            spacing: 14

            AppText {
                Layout.fillWidth: true
                text: (root.item && root.item.title) ? root.item.title : "Media info"
                font.pixelSize: Metrics.titlePx(parent.width)
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                maximumLineCount: 2
            }

            AppText {
                Layout.fillWidth: true
                visible: text.length > 0
                color: Theme.textSecondary
                text: {
                    if (!root.item) return ""
                    const parts = []
                    if (root.item.itemType) parts.push(root.item.itemType)
                    if (root.item.year > 0) parts.push(String(root.item.year))
                    if (root.item.subtitle) parts.push(root.item.subtitle)
                    return parts.join(" · ")
                }
                font.pixelSize: Metrics.bodyPx(parent.width)
            }

            SectionHeader { Layout.fillWidth: true; title: "File" }
            Surface {
                Layout.fillWidth: true
                Layout.preferredHeight: 64
                baseColor: Theme.bgPanel
                MonoText {
                    anchors.fill: parent
                    anchors.margins: 14
                    text: (root.item && root.item.path && root.item.path.length > 0) ? root.item.path : "File path unavailable"
                    elide: Text.ElideMiddle
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.NoWrap
                }
            }

            SectionHeader { Layout.fillWidth: true; title: "Overview"; detail: "" }
            AppText {
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: (root.item && root.item.overview && root.item.overview.length > 0) ? root.item.overview : "No overview available."
                color: Theme.textSecondary
                wrapMode: Text.Wrap
                font.pixelSize: Metrics.bodyPx(parent.width)
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                ActionButton {
                    id: closeBtn
                    text: "Close"
                    focus: true
                    onClicked: root.closed()
                    KeyNavigation.left: closeBtn
                    KeyNavigation.right: closeBtn
                    Keys.onReleased: (event) => {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter ||
                                event.key === Qt.Key_Select || event.key === Qt.Key_Space) {
                            root.closed()
                            event.accepted = true
                        }
                    }
                }
            }
        }
    }
}
