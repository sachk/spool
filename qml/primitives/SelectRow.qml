import QtQuick
import "../theme"

SettingRow {
    id: root
    property var options: []
    property int currentIndex: 0
    property bool handledNavigationPress: false
    readonly property string selectedText: options.length > 0 ? String(options[Math.max(0, Math.min(options.length - 1, currentIndex))]) : ""
    signal selected(int index, string value)
    valueText: selectedText
    valueTextVisible: false

    function move(dir) {
        if (options.length <= 0)
            return
        currentIndex = (currentIndex + dir + options.length) % options.length
        selected(currentIndex, String(options[currentIndex]))
    }

    onClicked: move(1)

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        MaterialIcon {
            anchors.verticalCenter: parent.verticalCenter
            name: "chevron_left"
            iconSize: Math.max(18, Metrics.metaPx(root.Window.window ? root.Window.window.width : 1920) + 6)
            iconColor: root.enabled && root.options.length > 1 ? Theme.textSecondary : Theme.textDisabled
        }

        Rectangle {
            width: Math.min(Math.max(valueLabel.implicitWidth + 28, 112), Math.max(112, root.width * 0.42))
            height: Math.max(32, Metrics.metaPx(root.Window.window ? root.Window.window.width : 1920) + 18)
            radius: Theme.radiusMedium
            color: root.activeFocus ? Theme.accentPanel : Theme.bgRaised
            border.width: root.activeFocus ? 2 : 1
            border.color: root.activeFocus ? Theme.accent : Theme.border
            antialiasing: true

            AppText {
                id: valueLabel
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                text: root.selectedText
                color: Theme.textPrimary
                font.pixelSize: Metrics.metaPx(root.Window.window ? root.Window.window.width : 1920)
                font.weight: Font.Medium
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }

        MaterialIcon {
            anchors.verticalCenter: parent.verticalCenter
            name: "chevron_right"
            iconSize: Math.max(18, Metrics.metaPx(root.Window.window ? root.Window.window.width : 1920) + 6)
            iconColor: root.enabled && root.options.length > 1 ? Theme.textSecondary : Theme.textDisabled
        }
    }

    function handleNavigationKey(key) {
        if ((key === Qt.Key_Left || key === Qt.Key_Right) && options.length > 0) {
            handledNavigationPress = true
            move(key === Qt.Key_Right ? 1 : -1)
            return true
        }
        return false
    }

    Keys.onReleased: (event) => {
        if ((event.key === Qt.Key_Left || event.key === Qt.Key_Right) && options.length > 0) {
            if (handledNavigationPress) {
                handledNavigationPress = false
                event.accepted = true
                return
            }
            const dir = event.key === Qt.Key_Right ? 1 : -1
            move(dir)
            event.accepted = true
        }
    }
}
