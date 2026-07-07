import QtQuick

ListView {
    id: root

    signal edgeUp()
    signal edgeDown()
    signal accepted(int index)

    focus: true
    keyNavigationEnabled: false

    function clampIndex() {
        currentIndex = count > 0 ? Math.max(0, Math.min(currentIndex, count - 1)) : -1
    }

    function handleKey(key) {
        clampIndex()
        if (key === Qt.Key_Up) {
            if (currentIndex <= 0) { edgeUp(); return true }
            --currentIndex
            return true
        }
        if (key === Qt.Key_Down) {
            if (currentIndex >= count - 1) { edgeDown(); return true }
            ++currentIndex
            return true
        }
        if (InputKeys.isAccept(key, false)) {
            accepted(currentIndex)
            return true
        }
        return false
    }
}
