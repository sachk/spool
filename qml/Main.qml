import QtQuick
import QtQuick.Controls.Basic
import QtQuick.VirtualKeyboard
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

    // Virtual keyboard. Becomes visible whenever a TextField / TextInput
    // gets focus and the input method asks for it. We anchor it to the
    // bottom of the window so the focused field can scroll its content
    // up if needed. The keyboard handles D-pad navigation internally
    // (FEATURE_vkb_arrow_keynavigation in our Qt build).
    InputPanel {
        id: inputPanel
        z: 99
        y: root.height
        anchors.left: parent.left
        anchors.right: parent.right
        states: State {
            name: "visible"
            when: inputPanel.active
            PropertyChanges {
                target: inputPanel
                y: root.height - inputPanel.height
            }
        }
        transitions: Transition {
            from: ""
            to: "visible"
            reversible: true
            ParallelAnimation {
                NumberAnimation {
                    properties: "y"
                    duration: 150
                    easing.type: Easing.InOutQuad
                }
            }
        }
    }
}
