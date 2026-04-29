import QtQuick
import "../theme"

SettingRow {
    id: root
    property var options: []
    property int currentIndex: 0
    signal selected(int index, string value)
    valueText: options.length > 0 ? String(options[Math.max(0, Math.min(options.length - 1, currentIndex))]) : ""

    Keys.onReleased: (event) => {
        if ((event.key === Qt.Key_Left || event.key === Qt.Key_Right) && options.length > 0) {
            const dir = event.key === Qt.Key_Right ? 1 : -1
            currentIndex = (currentIndex + dir + options.length) % options.length
            selected(currentIndex, String(options[currentIndex]))
            event.accepted = true
        }
    }
}
