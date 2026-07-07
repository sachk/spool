pragma Singleton

import QtQuick

QtObject {
    function focus(item) {
        if (item)
            item.forceActiveFocus()
    }

    function isAccept(key, includeSpace) {
        return key === Qt.Key_Return
                || key === Qt.Key_Enter
                || key === Qt.Key_Select
                || (includeSpace !== false && key === Qt.Key_Space)
    }

    function isBack(key, includeBackspace, includeBrowserBack) {
        return key === Qt.Key_Back
                || key === Qt.Key_Escape
                || (includeBackspace !== false && key === Qt.Key_Backspace)
                || (includeBrowserBack !== false && key === Qt.Key_BrowserBack)
    }

    function hasProperty(item, name) {
        return item && typeof item[name] !== "undefined"
    }

    function isTextInputItem(item) {
        return hasProperty(item, "cursorPosition")
                && hasProperty(item, "selectedText")
                && hasProperty(item, "text")
                && (hasProperty(item, "echoMode")
                    || hasProperty(item, "inputMethodHints")
                    || hasProperty(item, "textFormat"))
    }

    function isBackEvent(event, includeBackspace, includeBrowserBack) {
        const scanCode = Number(event.nativeScanCode || 0)
        const virtualKey = Number(event.nativeVirtualKey || 0)
        const key = Number(event.key || 0)
        return isBack(event.key, includeBackspace === true, includeBrowserBack)
                || event.key === 0x01200003
                || key === 461
                || scanCode === 420
                || scanCode === 461
                || virtualKey === 420
                || virtualKey === 461
    }

    function isDirection(key) {
        return isHorizontal(key) || isVertical(key)
    }

    function isHorizontal(key) {
        return key === Qt.Key_Left || key === Qt.Key_Right
    }

    function isVertical(key) {
        return key === Qt.Key_Up || key === Qt.Key_Down
    }

    function isMedia(key) {
        return key === Qt.Key_MediaPlay || key === Qt.Key_Play
    }

    function isMediaNext(key) {
        return key === Qt.Key_MediaNext
    }

    function isMediaPrevious(key) {
        return key === Qt.Key_MediaPrevious
    }

    function isColor(key) {
        return key === Qt.Key_Red
                || key === Qt.Key_Green
                || key === Qt.Key_Yellow
                || key === Qt.Key_Blue
    }

    function isIgnoredPlayerNoise(event) {
        const scanCode = Number(event.nativeScanCode || 0)
        return event.key === 0 && (scanCode === 1206 || scanCode === 1207)
    }
}
