import QtQuick
import QtQuick.Layouts
import "../theme"

FocusScope {
    id: root

    property string title: "Confirm"
    property string message: ""
    property string confirmText: "Confirm"
    property bool destructive: false

    signal accepted
    signal dismissed

    anchors.fill: parent
    focus: true
    z: 200

    function routeKey(key, phase, repeat) {
        if (InputKeys.isBack(key, false, false)) {
            if (phase === "release")
                dismissed()
            return true
        }
        if (!InputKeys.isDirection(key))
            return InputKeys.isAccept(key)
        if (phase === "press" && InputKeys.isHorizontal(key))
            InputKeys.focus(cancelButton.activeFocus ? confirmButton : cancelButton)
        return true
    }

    function activate() {
        if (confirmButton.activeFocus)
            accepted()
        else
            dismissed()
    }

    function back() {
        dismissed()
        return true
    }

    Component.onCompleted: Qt.callLater(function () {
        InputKeys.focus(cancelButton)
    })

    Rectangle {
        anchors.fill: parent
        color: "#99000000"

        TapHandler {
            onTapped: root.dismissed()
        }
    }

    Surface {
        anchors.centerIn: parent
        width: Math.min(parent.width - Metrics.scaled(96), Metrics.scaled(560))
        height: content.implicitHeight + Metrics.scaled(48)
        elevated: true
        baseColor: Theme.floatingPanel

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: Metrics.scaled(24)
            spacing: Metrics.scaled(16)

            AppText {
                Layout.fillWidth: true
                text: root.title
                font.pixelSize: Metrics.titleSizePx
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }

            AppText {
                Layout.fillWidth: true
                text: root.message
                color: Theme.textSecondary
                font.pixelSize: Metrics.bodySizePx
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Metrics.scaled(8)
                spacing: Metrics.scaled(12)

                Item {
                    Layout.fillWidth: true
                }

                ActionButton {
                    id: cancelButton
                    text: "Cancel"
                    onClicked: root.dismissed()
                }

                ActionButton {
                    id: confirmButton
                    text: root.confirmText
                    kind: root.destructive ? "danger" : "primary"
                    onClicked: root.accepted()
                }
            }
        }
    }
}
