import QtQuick
import QtTest
import "../../qml/primitives" as Primitives

TestCase {
    id: testCase
    name: "OptionPickerInput"
    width: 1280
    height: 720
    visible: true
    when: windowShown
    property int underlayClicks: 0

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

    QtObject {
        id: metricsStub
        readonly property int controlHeightPx: 44
        function scaled(value) {
            return value
        }
    }

    Item {
        id: stage
        anchors.fill: parent
    }

    MouseArea {
        parent: stage
        anchors.fill: parent
        onClicked: testCase.underlayClicks++
    }

    Rectangle {
        id: pickerAnchor
        parent: stage
        x: 200
        y: 120
        width: 360
        height: 60
    }

    Primitives.OptionPickerDialog {
        id: picker
        parent: stage
        visible: true
        options: ["First", "Second", "Third"]
        currentIndex: 0
        inputKeys: inputKeysStub
        metrics: metricsStub
        anchorItem: pickerAnchor
    }

    SignalSpy {
        id: selectedSpy
        target: picker
        signalName: "selected"
    }

    function init() {
        selectedSpy.clear()
        underlayClicks = 0
        picker.currentIndex = 0
        pickerAnchor.width = 360
    }

    function test_directionMovesOnPressOnly() {
        verify(picker.routeKey(Qt.Key_Down, "press", false))
        verify(picker.routeKey(Qt.Key_Down, "release", false))
        picker.activate()
        compare(selectedSpy.count, 1)
        compare(selectedSpy.signalArguments[0][0], 1)
    }

    function test_wideAnchorMakesLongOptionsReadable() {
        pickerAnchor.width = 680
        verify(picker.panelWidth >= 520)
    }

    function test_pointerSelectionDoesNotClickThrough() {
        picker.completePresentation()
        picker.completePresentation()
        verify(picker.placementReady)
        const panel = findChild(picker, "optionPickerPanel")
        verify(panel)
        mouseClick(testCase, panel.x + 20, panel.y + 8 + picker.rowHeight / 2, Qt.LeftButton)
        compare(selectedSpy.count, 1)
        compare(selectedSpy.signalArguments[0][0], 0)
        compare(underlayClicks, 0)
    }
}
