import QtQuick
import QtQuick.Controls.Basic
import "theme"
import "shell"

FocusScope {
    id: root
    width: 1920
    height: 1080
    focus: true

    AppShell {
        anchors.fill: parent
    }
}
