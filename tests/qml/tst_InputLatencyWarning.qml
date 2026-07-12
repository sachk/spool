import QtQuick
import QtTest
import "../../qml/shell" as Shell

TestCase {
    name: "InputLatencyWarning"
    when: windowShown
    visible: true
    width: 800
    height: 600

    QtObject {
        id: monitorDouble

        property bool warningVisible: false
        property string warningText: ""
        property string warningStage: ""
    }

    Shell.InputLatencyWarning {
        id: warning
        monitor: monitorDouble
    }

    function descendantTexts(item) {
        let texts = []
        for (let index = 0; index < item.children.length; ++index) {
            const child = item.children[index]
            if (child.text !== undefined)
                texts.push(child.text)
            texts = texts.concat(descendantTexts(child))
        }
        return texts
    }

    function init() {
        monitorDouble.warningVisible = false
        monitorDouble.warningText = ""
        monitorDouble.warningStage = ""
    }

    function test_warningFollowsMonitorState() {
        compare(warning.visible, false)

        monitorDouble.warningText = "Input response missed frame budget: 20.00 ms / 16.67 ms"
        monitorDouble.warningStage = "present_queued"
        monitorDouble.warningVisible = true

        compare(warning.visible, true)
        let texts = descendantTexts(warning)
        verify(texts.includes(monitorDouble.warningText))
        verify(texts.includes(monitorDouble.warningStage))

        const previousWarningText = monitorDouble.warningText
        const previousWarningStage = monitorDouble.warningStage
        monitorDouble.warningText = "Input response missed frame budget: 24.50 ms / 16.67 ms"
        monitorDouble.warningStage = "rendering"

        texts = descendantTexts(warning)
        verify(texts.includes(monitorDouble.warningText))
        verify(texts.includes(monitorDouble.warningStage))
        verify(!texts.includes(previousWarningText))
        verify(!texts.includes(previousWarningStage))

        monitorDouble.warningVisible = false
        compare(warning.visible, false)
    }
}
