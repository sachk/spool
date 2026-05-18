import QtQuick
import "../theme"

SettingRow {
    id: root
    property var options: []
    property int currentIndex: 0
    property bool handledNavigationPress: false
    signal selected(int index, string value)
    valueText: options.length > 0 ? String(options[Math.max(0, Math.min(options.length - 1, currentIndex))]) : ""
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
        spacing: 6

        Repeater {
            model: root.options.length
            delegate: Rectangle {
                required property int index
                readonly property bool selected: index === root.currentIndex
                width: Math.max(label.implicitWidth + 22, 44)
                height: 30
                radius: 15
                color: selected ? Theme.accentPanel : "#24151C24"
                border.width: selected ? 2 : 1
                border.color: selected ? Theme.accent : Theme.border

                AppText {
                    id: label
                    anchors.centerIn: parent
                    text: String(root.options[index])
                    color: selected ? Theme.textPrimary : Theme.textSecondary
                    font.pixelSize: Metrics.metaPx(root.Window.window ? root.Window.window.width : 1920)
                    font.weight: selected ? Font.DemiBold : Font.Medium
                }
            }
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
