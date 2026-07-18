import QtQuick
import QtTest
import "../../qml/shell" as Shell

TestCase {
    id: testCase
    name: "TlsTrustDialogs"
    width: 1280
    height: 720
    when: windowShown

    property int cancelCalls: 0
    property int onceCalls: 0
    property int rememberCalls: 0
    property string removedKey: ""

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
            return isHorizontal(key) || key === Qt.Key_Up || key === Qt.Key_Down
        }
        function isHorizontal(key) {
            return key === Qt.Key_Left || key === Qt.Key_Right
        }
    }

    QtObject {
        id: trustStub
        property string pendingSource: "TLS fixture"
        property string pendingAuthority: "https://localhost:443"
        property string pendingIssuer: "localhost"
        property string pendingFingerprint: "AA:BB"
        property string pendingErrors: "Self-signed certificate"
        property var rememberedCertificates: [
            {
                "key": "tls/trusted/example",
                "authority": "https://localhost:443",
                "issuer": "localhost",
                "fingerprint": "AA:BB"
            }
        ]
        function cancel() {
            ++testCase.cancelCalls
        }
        function trustOnce() {
            ++testCase.onceCalls
        }
        function remember() {
            ++testCase.rememberCalls
        }
        function removeRemembered(key) {
            testCase.removedKey = key
        }
    }

    Shell.TlsTrustDialog {
        id: decisionDialog
        visible: true
        trustController: trustStub
        inputKeys: inputKeysStub
    }

    Shell.RememberedCertificatesDialog {
        id: managerDialog
        visible: false
        trustController: trustStub
        inputKeys: inputKeysStub
    }

    SignalSpy {
        id: dismissedSpy
        target: managerDialog
        signalName: "dismissed"
    }

    function init() {
        cancelCalls = 0
        onceCalls = 0
        rememberCalls = 0
        removedKey = ""
        dismissedSpy.clear()
        managerDialog.visible = false
        decisionDialog.visible = false
        decisionDialog.visible = true
        wait(0)
    }

    function test_decisionActionsAndBack() {
        verify(decisionDialog.routeKey(Qt.Key_A, "press", false))
        verify(decisionDialog.routeKey(Qt.Key_Right, "press", false))
        decisionDialog.activate()
        compare(onceCalls, 1)

        decisionDialog.visible = false
        decisionDialog.visible = true
        wait(0)
        decisionDialog.routeKey(Qt.Key_Right, "press", false)
        decisionDialog.routeKey(Qt.Key_Right, "press", false)
        decisionDialog.activate()
        compare(rememberCalls, 1)

        verify(decisionDialog.routeKey(Qt.Key_Back, "press", false))
        verify(decisionDialog.routeKey(Qt.Key_Back, "release", false))
        compare(cancelCalls, 1)
    }

    function test_managerForgetsSelectedCertificate() {
        decisionDialog.visible = false
        managerDialog.visible = true
        wait(0)
        verify(managerDialog.routeKey(Qt.Key_Down, "press", false))
        managerDialog.activate()
        managerDialog.activate()
        compare(removedKey, "tls/trusted/example")
    }

    function test_managerBackDismissesOnRelease() {
        decisionDialog.visible = false
        managerDialog.visible = true
        wait(0)
        verify(managerDialog.routeKey(Qt.Key_Back, "press", false))
        compare(dismissedSpy.count, 0)
        verify(managerDialog.routeKey(Qt.Key_Back, "release", false))
        compare(dismissedSpy.count, 1)
    }
}
