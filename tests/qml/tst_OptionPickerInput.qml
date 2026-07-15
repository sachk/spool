import QtQuick
import QtTest
import "../../qml/primitives" as Primitives

TestCase {
    id: testCase
    name: "OptionPickerInput"
    width: 1280
    height: 720
    when: windowShown

    QtObject {
        id: inputKeysStub

        function focus(item) {
            if (item)
                item.forceActiveFocus()
        }

        function isBack(key) {
            return key === Qt.Key_Back || key === Qt.Key_Escape
        }

        function isDirection(key) {
            return key === Qt.Key_Up || key === Qt.Key_Down || key === Qt.Key_Left || key === Qt.Key_Right
        }

        function isAccept(key) {
            return key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select
        }
    }

    Primitives.OptionPickerDialog {
        id: picker
        visible: true
        options: ["First", "Second", "Third"]
        currentIndex: 0
        inputKeys: inputKeysStub
    }

    SignalSpy {
        id: selectedSpy
        target: picker
        signalName: "selected"
    }

    function init() {
        selectedSpy.clear()
        picker.currentIndex = 0
        picker.visible = false
        picker.visible = true
        wait(0)
    }

    function test_directionMovesOnPressOnly() {
        verify(picker.routeKey(Qt.Key_Down, "press", false))
        verify(picker.routeKey(Qt.Key_Down, "release", false))
        picker.activate()
        compare(selectedSpy.count, 1)
        compare(selectedSpy.signalArguments[0][0], 1)
    }
}
