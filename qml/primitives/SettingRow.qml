import QtQuick
import QtQuick.Layouts
import "../theme"

Surface {
    id: root
    property string title: ""
    property string description: ""
    property string valueText: ""
    property bool rowFocus: activeFocus
    property int settingIndex: -1
    property bool pointerActivationEnabled: true
    signal clicked()
    focused: rowFocus
    focus: true
    implicitHeight: 68

    HoverHandler { id: hover }
    TapHandler {
        enabled: root.pointerActivationEnabled
        onTapped: {
            root.forceActiveFocus()
            root.clicked()
        }
    }

    hovered: hover.hovered

    RowLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 18

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3
            AppText { text: root.title; font.pixelSize: Metrics.bodyPx(root.Window.window ? root.Window.window.width : 1920); font.weight: Font.Medium }
            AppText { text: root.description; visible: text.length > 0; color: Theme.textMuted; font.pixelSize: Metrics.metaPx(root.Window.window ? root.Window.window.width : 1920); elide: Text.ElideRight; Layout.fillWidth: true }
        }

        MonoText {
            text: root.valueText
            color: Theme.textSecondary
            font.pixelSize: Metrics.metaPx(root.Window.window ? root.Window.window.width : 1920)
        }
    }
}
