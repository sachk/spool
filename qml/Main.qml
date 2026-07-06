import QtQuick
import "shell"
import "primitives"

FocusScope {
    id: root
    width: 1920
    height: 1080
    focus: true

    property bool virtualKeyboardRequested: false

    function requestKeyboard() {
        if (isWebOS)
            virtualKeyboardRequested = true
    }

    Connections {
        target: Qt.inputMethod
        function onVisibleChanged() {
            if (Qt.inputMethod.visible)
                root.requestKeyboard()
        }
        function onAnchorRectangleChanged() {
            const window = root.Window.window
            if (window && InputKeys.isTextInputItem(window.activeFocusItem))
                root.requestKeyboard()
        }
    }

    Connections {
        target: root.Window.window
        function onActiveFocusItemChanged() {
            if (InputKeys.isTextInputItem(root.Window.window.activeFocusItem))
                root.requestKeyboard()
        }
    }

    Connections {
        target: appController
        function onAggressiveMemoryPressure() {
            Qt.inputMethod.hide()
            root.virtualKeyboardRequested = false
        }
    }

    AppShell {
        anchors.fill: parent
    }

    Loader {
        id: virtualKeyboardLoader
        anchors.fill: parent
        z: 99
        active: isWebOS && root.virtualKeyboardRequested
        source: active ? "shell/VirtualKeyboardPanel.qml" : ""
    }
}
