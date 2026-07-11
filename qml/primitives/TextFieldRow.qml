import QtQuick
import QtQuick.Templates as T
import "../theme"

// TV-friendly text input. The wrapper control is the D-pad target; the
// internal TextField only grabs focus (and the virtual keyboard) when the
// user presses Select on the row. Back releases focus and dismisses the IM.
T.Control {
    id: row

    property alias text: field.text
    property alias placeholderText: field.placeholderText
    property alias inputMethodHints: field.inputMethodHints
    property alias echoMode: field.echoMode
    property int enterKeyType: Qt.EnterKeyDefault
    property string label: ""
    readonly property bool editing: field.activeFocus

    signal textEdited(string text)
    signal accepted

    focusPolicy: Qt.StrongFocus
    implicitHeight: Metrics.scaled(68)
    implicitWidth: Metrics.scaled(400)

    function focusRow() {
        if (field.activeFocus)
            field.focus = false
        InputKeys.focus(row)
    }

    function focusField() {
        InputKeys.focus(field)
        Qt.inputMethod.show()
    }

    Keys.onPressed: event => {
                        if (InputKeys.isAccept(event.key)) {
                            row.focusField()
                            event.accepted = true
                        }
                    }

    background: Rectangle {
        radius: Theme.radiusMedium
        color: row.editing ? Theme.bgRaised : Theme.bgPanel
        border.width: (row.activeFocus || row.editing) ? Theme.focusBorderWidth : 1
        border.color: row.editing ? Theme.accent : row.activeFocus ? Theme.accent : Theme.border

        Text {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: Metrics.scaled(16)
            anchors.topMargin: Metrics.scaled(7)
            visible: row.label.length > 0
            text: row.label
            color: Theme.textMuted
            font.pixelSize: Metrics.metaPx(row.width) + Metrics.scaled(2)
            font.weight: Font.Medium
        }
    }

    T.TextField {
        id: field
        EnterKey.type: row.enterKeyType
        anchors.fill: parent
        anchors.leftMargin: Metrics.scaled(15)
        anchors.rightMargin: Metrics.scaled(15)
        anchors.topMargin: row.label.length > 0 ? Metrics.scaled(22) : Metrics.scaled(9)
        anchors.bottomMargin: Metrics.scaled(9)
        background: Item {}
        color: Theme.textPrimary
        font.pixelSize: Metrics.bodyPx(row.width) + Metrics.scaled(2)
        verticalAlignment: TextInput.AlignVCenter
        selectByMouse: true
        focus: false
        activeFocusOnTab: false

        onTextEdited: row.textEdited(text)
        onAccepted: row.accepted()

        // Catch Back / Escape before the input method consumes the press so
        // the user can cleanly exit the keyboard with the TV remote's Back.
        Keys.priority: Keys.BeforeItem
        Keys.onPressed: event => {
                            if (InputKeys.isBackEvent(event, false)) {
                                Qt.inputMethod.hide()
                                row.focusRow()
                                event.accepted = true
                                return
                            }
                            if (!Qt.inputMethod.visible && (event.key === Qt.Key_Up || event.key === Qt.Key_Down
                                                            || event.key === Qt.Key_Tab || event.key
                                                            === Qt.Key_Backtab)) {
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
