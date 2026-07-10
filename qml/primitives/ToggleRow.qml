import QtQuick
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
}
