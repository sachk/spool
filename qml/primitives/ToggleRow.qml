import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../theme"

SettingRow {
    id: root
    property bool checked: false
    signal toggled(bool checked)
    valueText: checked ? "On" : "Off"

    function toggle() {
        checked = !checked
        toggled(checked)
    }

    onClicked: toggle()

    Keys.onReleased: (event) => {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            toggle()
            event.accepted = true
        }
    }
}
