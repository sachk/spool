import QtQuick
import QtQuick.Controls.Basic
import "../theme"

// TV-friendly text input. The wrapper FocusScope is the D-pad target; the
// internal TextField only grabs focus (and the virtual keyboard) when the
// user presses Select on the row. Back releases focus and dismisses the IM.
FocusScope {
    id: row

    property alias text: field.text
    property alias placeholderText: field.placeholderText
    property alias inputMethodHints: field.inputMethodHints
    property alias echoMode: field.echoMode
    property string label: ""
    readonly property bool editing: field.activeFocus

    signal textEdited(string text)
    signal accepted()

    implicitHeight: 56
    implicitWidth: 320

    function focusRow() {
        if (field.activeFocus)
            field.focus = false
        forceActiveFocus()
    }

    function focusField() {
        field.forceActiveFocus()
        Qt.inputMethod.show()
    }

    Keys.onPressed: (event) => {
        if (InputKeys.isAccept(event.key)) {
            row.focusField()
            event.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusMedium
        color: row.editing ? Theme.bgRaised : Theme.bgPanel
        border.width: (row.activeFocus || row.editing) ? Theme.focusBorderWidth : 1
        border.color: row.editing ? Theme.accent : row.activeFocus ? Theme.accent : Theme.border

        Text {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: 14
            anchors.topMargin: 6
            visible: row.label.length > 0
            text: row.label
            color: Theme.textMuted
            font.pixelSize: 11
            font.weight: Font.Medium
        }
    }

    TextField {
        id: field
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: row.label.length > 0 ? 18 : 8
        anchors.bottomMargin: 8
        background: Item {}
        color: Theme.textPrimary
        font.pixelSize: 18
        verticalAlignment: TextInput.AlignVCenter
        selectByMouse: true
        focus: false
        activeFocusOnTab: false

        onTextEdited: row.textEdited(text)
        onAccepted: row.accepted()

        // Catch Back / Escape before the input method consumes the press so
        // the user can cleanly exit the keyboard with the TV remote's Back.
        Keys.priority: Keys.BeforeItem
        Keys.onPressed: (event) => {
            if (InputKeys.isBack(event.key, false)) {
                row.focusRow()
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_Up || event.key === Qt.Key_Down
                || event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab) {
                row.focusRow()
                event.accepted = false
                return
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: row.focusField()
        propagateComposedEvents: true
    }
}
