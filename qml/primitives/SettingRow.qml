import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import "../theme"

T.Control {
    id: root
    property string title: ""
    property string description: ""
    property string valueText: ""
    property bool valueTextVisible: true
    property bool rowFocus: activeFocus
    property int settingIndex: -1
    property bool pointerActivationEnabled: true
    signal clicked()
    focusPolicy: Qt.StrongFocus
    focus: true
    implicitHeight: Math.max(68, textColumn.implicitHeight + 28)
    leftPadding: 14
    rightPadding: 14
    topPadding: 14
    bottomPadding: 14

    background: Surface {
        focused: root.rowFocus
        hovered: hover.hovered
    }

    HoverHandler { id: hover }
    TapHandler {
        enabled: root.pointerActivationEnabled
        onTapped: {
            root.forceActiveFocus()
            root.clicked()
        }
    }

    contentItem: RowLayout {
        spacing: 18

        ColumnLayout {
            id: textColumn
            Layout.fillWidth: true
            spacing: 3
            AppText { Layout.fillWidth: true; text: root.title; font.pixelSize: Metrics.bodyPx(root.Window.window ? root.Window.window.width : 1920); font.weight: Font.Medium; maximumLineCount: 1; elide: Text.ElideRight }
            AppText { text: root.description; visible: text.length > 0; color: Theme.textMuted; font.pixelSize: Metrics.metaPx(root.Window.window ? root.Window.window.width : 1920); elide: Text.ElideRight; Layout.fillWidth: true }
        }

        MonoText {
            visible: root.valueTextVisible
            text: root.valueText
            color: Theme.textSecondary
            font.pixelSize: Metrics.metaPx(root.Window.window ? root.Window.window.width : 1920)
            Layout.maximumWidth: Math.max(96, root.width * 0.42)
            maximumLineCount: 1
            elide: Text.ElideRight
        }
    }
}
