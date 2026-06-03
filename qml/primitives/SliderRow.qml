import QtQuick
import "../theme"

SettingRow {
    id: root
    property real value: 1.0
    property real from: 0.75
    property real to: 1.5
    property real step: 0.05
    property int decimals: 2
    property string valueSuffix: "x"
    property bool handledNavigationPress: false
    signal valueEdited(real value)
    valueText: Number(value).toFixed(decimals) + valueSuffix

    function adjust(dir) {
        value = Math.max(from, Math.min(to, Math.round((value + step * dir) / step) * step))
        valueEdited(value)
    }

    onClicked: adjust(1)

    function handleNavigationKey(key) {
        if (key === Qt.Key_Left || key === Qt.Key_Right) {
            handledNavigationPress = true
            adjust(key === Qt.Key_Right ? 1 : -1)
            return true
        }
        return false
    }

    Keys.onReleased: (event) => {
        if (event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
            if (handledNavigationPress) {
                handledNavigationPress = false
                event.accepted = true
                return
            }
            const dir = event.key === Qt.Key_Right ? 1 : -1
            adjust(dir)
            event.accepted = true
        }
    }
}
