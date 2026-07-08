import QtQuick

FocusScope {
    id: root

    property bool wrap: false
    signal leftEdge
    signal rightEdge
    signal upEdge
    signal downEdge

    function edge(key) {
        if (key === Qt.Key_Left) {
            leftEdge()
            return true
        }
        if (key === Qt.Key_Right) {
            rightEdge()
            return true
        }
        if (key === Qt.Key_Up) {
            upEdge()
            return true
        }
        if (key === Qt.Key_Down) {
            downEdge()
            return true
        }
        return false
    }
}
