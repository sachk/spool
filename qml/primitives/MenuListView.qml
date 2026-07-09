import QtQuick

NavList {
    id: root

    property bool dismissOnBack: true
    property bool dismissOnHorizontal: true
    property Item edgeEscapeItem: null
    property var rowEnabled: function (entry, index) {
        return !(entry && entry.section === true)
    }

    signal dismissed

    clip: true
    keyNavigationEnabled: false
    boundsBehavior: Flickable.StopAtBounds

    function entryAt(index) {
        if (index < 0 || index >= count)
            return null
        if (model && model.get)
            return model.get(index)
        if (Array.isArray(model))
            return model[index]
        return null
    }

    function isRowEnabled(index) {
        return index >= 0 && index < count && rowEnabled(entryAt(index), index)
    }

    function firstEnabled(start, step) {
        if (count <= 0)
            return -1
        const direction = step < 0 ? -1 : 1
        let index = Math.max(0, Math.min(count - 1, start))
        while (index >= 0 && index < count) {
            if (isRowEnabled(index))
                return index
            index += direction
        }
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

    function move(step) {
        if (!clampEnabled())
            return true
        const next = firstEnabled(currentIndex + step, step)
        if (next >= 0) {
            currentIndex = next
            positionViewAtIndex(currentIndex, ListView.Contain)
            return true
        }
        if (step < 0) {
            if (edgeEscapeItem)
                InputKeys.focus(edgeEscapeItem)
            else
                edgeUp()
        } else {
            edgeDown()
        }
        return true
    }

    function handleKey(key) {
        if (dismissOnBack && InputKeys.isBack(key, false, false)) {
            dismissed()
            return true
        }
        if (dismissOnHorizontal && InputKeys.isHorizontal(key)) {
            dismissed()
            return true
        }
        if (key === Qt.Key_Up)
            return move(-1)
        if (key === Qt.Key_Down)
            return move(1)
        if (InputKeys.isAccept(key, false)) {
            if (clampEnabled())
                accepted(currentIndex)
            return true
        }
        return false
    }

    onCountChanged: clampEnabled()
    onCurrentIndexChanged: if (currentIndex >= 0)
                               positionViewAtIndex(currentIndex, ListView.Contain)

    FastWheelHandler {
        flickable: root
    }

    // Direction moves on press (so the shell's press dispatch never races a
    // second move on release), accept fires on release — and only after a
    // fresh press seen while this list was focused, never the tail end of the
    // long-press that opened the menu.
    property bool acceptPressArmed: false
    property double focusGainedAtMs: 0

    onActiveFocusChanged: {
        acceptPressArmed = false
        if (activeFocus)
            focusGainedAtMs = Date.now()
    }

    Keys.onPressed: event => {
                        if (InputKeys.isVertical(event.key)) {
                            move(event.key === Qt.Key_Up ? -1 : 1)
                            event.accepted = true
                            return
                        }
                        if (InputKeys.isAccept(event.key, false)) {
                            if (!event.isAutoRepeat && Date.now() - focusGainedAtMs > 250)
                            acceptPressArmed = true
                            event.accepted = true
                        }
                    }

    Keys.onReleased: event => {
                         if (event.isAutoRepeat) {
                             event.accepted = InputKeys.isVertical(event.key) || InputKeys.isAccept(event.key, false)
                             return
                         }
                         if (InputKeys.isVertical(event.key)) {
                             event.accepted = true
                             return
                         }
                         if (InputKeys.isAccept(event.key, false)) {
                             event.accepted = true
                             if (!acceptPressArmed)
                             return
                             acceptPressArmed = false
                             if (clampEnabled())
                             accepted(currentIndex)
                             return
                         }
                         if (root.handleKey(event.key))
                         event.accepted = true
                     }
}
