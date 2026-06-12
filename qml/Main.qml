import QtQuick
import "shell"

FocusScope {
    id: root
    width: 1920
    height: 1080
    focus: true

    AppShell {
        anchors.fill: parent
    }

    Loader {
        id: virtualKeyboardLoader
        anchors.fill: parent
        z: 99
        active: isWebOS
        source: active ? "shell/VirtualKeyboardPanel.qml" : ""
    }
}
