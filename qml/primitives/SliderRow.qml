import QtQuick
import "../theme"

SettingRow {
    id: root
    property real value: 1.0
    property real from: 0.75
    property real to: 1.5
    property real step: 0.05
    signal valueEdited(real value)
    valueText: Number(value).toFixed(2) + "x"

    Keys.onReleased: (event) => {
        if (event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
            const dir = event.key === Qt.Key_Right ? 1 : -1
            value = Math.max(from, Math.min(to, Math.round((value + step * dir) / step) * step))
            valueEdited(value)
            event.accepted = true
        }
    }
}
