import QtQuick
import QtQuick.VirtualKeyboard
import "../primitives"

Item {
    id: root

    // Virtual keyboard. Becomes visible when a TextField inside a TextFieldRow
    // (TV-friendly wrapper) asks for it explicitly. The keyboard handles D-pad
    // navigation internally (FEATURE_vkb_arrow_keynavigation in our Qt build).
    // Back is intercepted at the InputPanel level so the user can close the
    // keyboard without it tunnelling through to the surrounding shell.
    InputPanel {
        id: inputPanel
        y: root.height
        anchors.left: parent.left
        anchors.right: parent.right
        externalLanguageSwitchEnabled: false

        // The IM intercepts most keys when active. We catch back/escape here
        // so they hide the panel rather than propagating up and triggering
        // a back-stack pop.
        Keys.onPressed: (event) => {
            if (InputKeys.isBack(event.key, false)) {
                Qt.inputMethod.hide()
                event.accepted = true
            }
        }

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
