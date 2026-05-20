import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    property var shell
    property string query: ""
    focus: true

    Component.onCompleted: field.focusRow()

    function handleNavigationKey(key) {
        if (key === Qt.Key_Left && !field.editing) { shell.focusRail(); return true }
        if (key === Qt.Key_Down && field.activeFocus) {
            if (results.count > 0) { results.forceActiveFocus(); results.currentIndex = Math.max(0, results.currentIndex) }
            return true
        }
        if (key === Qt.Key_Up && results.activeFocus && results.currentIndex <= 0) {
            field.focusRow()
            return true
        }
        if ((key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select) && results.activeFocus && results.currentIndex >= 0) {
            shell.lastSearchIndex = results.currentIndex
            shell.lastGridIndex = results.currentIndex
            shell.pushRoute("itemDetails")
            return true
        }
        return false
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.pageMargin(width)
        spacing: 16

        SectionHeader { Layout.fillWidth: true; title: "Search" }

        TextFieldRow {
            id: field
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            label: "Search this library"
            placeholderText: "Title, actor, genre…"
            inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            onTextEdited: root.query = text
            KeyNavigation.down: results
        }

        // Quick suggestion chips. Selecting one populates the field.
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Repeater {
                model: ["resume", "hdr10", "unwatched", "direct play"]
                delegate: MetadataChip {
                    required property string modelData
                    text: modelData
                    MouseArea {
                        anchors.fill: parent
                        onClicked: { field.text = modelData; root.query = modelData }
                    }
                }
            }
        }

        ListView {
            id: results
            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: false
            keyNavigationEnabled: false
            currentIndex: count > 0 ? Math.max(0, Math.min(shell.lastSearchIndex, count - 1)) : -1
            spacing: 10
            clip: true
            model: appController.movies
            KeyNavigation.up: field

            delegate: Surface {
                required property int index
                required property string title
                required property string subtitle
                required property int year
                width: results.width
                height: 92
                focused: ListView.isCurrentItem && results.activeFocus
                visible: root.query.length === 0 || title.toLowerCase().indexOf(root.query.toLowerCase()) >= 0
                opacity: visible ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 120 } }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    ColumnLayout {
                        Layout.fillWidth: true
                        AppText { text: title; font.weight: Font.Medium }
                        TechMetadataLine { Layout.fillWidth: true; metadata: subtitle }
                    }
                    MetadataChip { text: year > 0 ? String(year) : "item" }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: { results.currentIndex = index; shell.lastSearchIndex = index; shell.lastGridIndex = index; shell.pushRoute("itemDetails") }
                }
            }

            Keys.onReleased: (event) => {
                if (event.key === Qt.Key_Up && currentIndex <= 0) { field.focusRow(); event.accepted = true }
                else if (event.key === Qt.Key_Left) { shell.focusRail(); event.accepted = true }
                else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Select) {
                    shell.lastSearchIndex = currentIndex
                    shell.lastGridIndex = currentIndex
                    shell.pushRoute("itemDetails")
                    event.accepted = true
                }
            }
        }

        EmptyPlaceholder {
            Layout.fillWidth: true
            visible: results.count === 0
            title: "Type to search"
            detail: "Press Select on the field to open the keyboard."
        }
    }
}
