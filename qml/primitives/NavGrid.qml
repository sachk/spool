import QtQuick

GridView {
    id: root

    property var shell
    property int fallbackColumns: 1
    signal edgeUp
    signal accepted(int index)

    focus: true
    keyNavigationEnabled: false

    function columnCount() {
        return Math.max(1, Math.floor(width / Math.max(1, cellWidth || fallbackColumns)))
    }

    function moveBy(delta) {
        if (count <= 0)
            return true
        currentIndex = Math.max(0, Math.min(count - 1, currentIndex + delta))
        return true
    }

    function routeKey(key, phase, repeat) {
        const columns = columnCount()
        if (key === Qt.Key_Left)
            return currentIndex % columns === 0 || moveBy(-1)
        if (key === Qt.Key_Right)
            return moveBy(1)
        if (key === Qt.Key_Up) {
            if (currentIndex < columns) {
                edgeUp()
                return true
            }
            return moveBy(-columns)
        }
        return key === Qt.Key_Down && moveBy(columns)
    }

    function activate() {
        if (currentIndex >= 0)
            accepted(currentIndex)
    }
}
