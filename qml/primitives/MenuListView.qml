import QtQuick

ListView {
    id: root

    property bool dismissOnBack: true
    property bool dismissOnHorizontal: true
    property Item edgeEscapeItem: null
    // ListView converts JavaScript arrays into an opaque instance model.
    // Supply a lookup callback when navigation needs to inspect their entries.
    property var entryProvider: null
    property var rowEnabled: function (entry, index) {
        return !(entry && entry.section === true)
    }

    signal edgeUp
    signal edgeDown
    signal accepted(int index)
    signal dismissed

    focus: true
    clip: true
    keyNavigationEnabled: false
    boundsBehavior: Flickable.StopAtBounds

    function entryAt(index) {
        if (index < 0 || index >= count)
            return null
        if (entryProvider)
            return entryProvider(index)
        if (model && model.get)
            return model.get(index)
        return null
    }

    function isRowEnabled(index) {
        return index >= 0 && index < count && rowEnabled(entryAt(index), index)
    }

    function firstEnabled(start, step) {
        const direction = step < 0 ? -1 : 1
        for (let index = Math.max(0, Math.min(count - 1, start)); index >= 0 && index < count; index += direction)
            if (isRowEnabled(index))
                return index
        return -1
    }

    function clampEnabled() {
        if (isRowEnabled(currentIndex))
            return true
        currentIndex = firstEnabled(Math.max(0, currentIndex), 1)
        if (currentIndex < 0)
            currentIndex = firstEnabled(count - 1, -1)
        return currentIndex >= 0
    }

    function moveSelection(step) {
        if (!clampEnabled())
            return true
        const next = firstEnabled(currentIndex + step, step)
        if (next >= 0) {
            currentIndex = next
            positionViewAtIndex(currentIndex, ListView.Contain)
        } else if (step < 0) {
            if (edgeEscapeItem)
                InputKeys.focus(edgeEscapeItem)
            else
                edgeUp()
        } else {
            edgeDown()
        }
        return true
    }

    function routeKey(key, phase, repeat) {
        if (phase === "release")
            return true
        if (dismissOnBack && InputKeys.isBack(key, false, false)) {
            dismissed()
            return true
        }
        if (dismissOnHorizontal && InputKeys.isHorizontal(key)) {
            dismissed()
            return true
        }
        if (key === Qt.Key_Up)
            return moveSelection(-1)
        if (key === Qt.Key_Down)
            return moveSelection(1)
        return false
    }

    function activate() {
        if (clampEnabled())
            accepted(currentIndex)
    }

    function back() {
        if (!dismissOnBack)
            return false
        dismissed()
        return true
    }

    onCountChanged: clampEnabled()
    onCurrentIndexChanged: if (currentIndex >= 0)
                               positionViewAtIndex(currentIndex, ListView.Contain)

    FastWheelHandler {
        flickable: root
    }
}
