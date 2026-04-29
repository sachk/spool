import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../theme"

SettingRow {
    id: root
    property bool checked: false
    signal toggled(bool checked)
    valueText: checked ? "On" : "Off"

    Keys.onReleased: (event) => {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            checked = !checked
            toggled(checked)
            event.accepted = true
        }
    }
}
