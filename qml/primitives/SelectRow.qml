import QtQuick
import "../theme"

SettingRow {
    id: root
    property var options: []
    property int currentIndex: 0
    property bool handledNavigationPress: false
    signal selected(int index, string value)
    valueText: options.length > 0 ? String(options[Math.max(0, Math.min(options.length - 1, currentIndex))]) : ""

    function move(dir) {
        if (options.length <= 0)
            return
        currentIndex = (currentIndex + dir + options.length) % options.length
        selected(currentIndex, String(options[currentIndex]))
    }

    onClicked: move(1)

    function handleNavigationKey(key) {
        if ((key === Qt.Key_Left || key === Qt.Key_Right) && options.length > 0) {
            handledNavigationPress = true
            move(key === Qt.Key_Right ? 1 : -1)
            return true
        }
        return false
    }

    Keys.onReleased: (event) => {
        if ((event.key === Qt.Key_Left || event.key === Qt.Key_Right) && options.length > 0) {
            if (handledNavigationPress) {
                handledNavigationPress = false
                event.accepted = true
                return
            }
            const dir = event.key === Qt.Key_Right ? 1 : -1
            move(dir)
            event.accepted = true
        }
    }
}
