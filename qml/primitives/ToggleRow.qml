import QtQuick
import "../theme"

SettingRow {
    id: root
    property bool checked: false
    signal toggled(bool checked)
    valueText: checked ? "On" : "Off"
    Accessible.role: Accessible.CheckBox
    Accessible.name: title
    Accessible.description: description
    Accessible.checkable: true
    Accessible.checked: checked
    Accessible.focusable: enabled
    Accessible.focused: activeFocus
    Accessible.onPressAction: toggle()
    Accessible.onToggleAction: toggle()

    function toggle() {
        checked = !checked
        toggled(checked)
    }

    onClicked: toggle()

    Keys.onReleased: (event) => {
        if (InputKeys.isAccept(event.key)) {
            toggle()
            event.accepted = true
        }
    }
}
