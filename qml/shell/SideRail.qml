import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    property string currentRoute: "home"
    signal navigate(string route)
    signal contentRequested()

    function focusCurrent() {
        for (let i = 0; i < railRepeater.count; ++i) {
            const item = railRepeater.itemAt(i)
            if (item && item.route === currentRoute) {
                item.forceButtonFocus()
                return
            }
        }
        const first = railRepeater.itemAt(0)
        if (first) first.forceButtonFocus()
    }

    function focusedIndex() {
        for (let i = 0; i < railRepeater.count; ++i) {
            const item = railRepeater.itemAt(i)
            if (item && item.hasButtonFocus())
                return i
        }
        return 0
    }

    function handleNavigationKey(key) {
        if (key === Qt.Key_Right) {
            contentRequested()
            return true
        }
        if (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Select || key === Qt.Key_Space) {
            const item = railRepeater.itemAt(focusedIndex())
            if (item) {
                navigate(item.route)
                return true
            }
        }
        if (key === Qt.Key_Up || key === Qt.Key_Down) {
            const next = Math.max(0, Math.min(railRepeater.count - 1, focusedIndex() + (key === Qt.Key_Down ? 1 : -1)))
            const item = railRepeater.itemAt(next)
            if (item) item.forceButtonFocus()
            return true
        }
        return false
    }

    Rectangle { anchors.fill: parent; color: Theme.bgRaised; border.width: 0 }
    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.border }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 12
        anchors.bottomMargin: 12
        spacing: 8

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            Rectangle {
                anchors.centerIn: parent
                width: 30; height: 30; radius: 6
                gradient: Gradient { GradientStop { position: 0; color: Theme.jellyfinBlue } GradientStop { position: 1; color: Theme.jellyfinPurple } }
            }
        }

        Repeater {
            id: railRepeater
            model: [
                { label: "Home", route: "home", icon: "H" },
                { label: "Search", route: "search", icon: "/" },
                { label: "Settings", route: "settings", icon: "S" }
            ]

            delegate: Item {
                id: railDelegate
                required property var modelData
                readonly property string route: modelData.route
                function forceButtonFocus() { button.forceActiveFocus() }
                function hasButtonFocus() { return button.activeFocus }
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                IconButton {
                    id: button
                    anchors.centerIn: parent
                    iconText: modelData.icon
                    railStyle: true
                    selected: root.currentRoute === modelData.route
                    onClicked: root.navigate(modelData.route)
                    Keys.onReleased: (event) => {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Select || event.key === Qt.Key_Space) {
                            root.navigate(modelData.route)
                            event.accepted = true
                            return
                        }
                        if (event.key === Qt.Key_Right) {
                            root.contentRequested()
                            event.accepted = true
                        }
                    }
                }
                Rectangle {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: root.currentRoute === modelData.route ? 5 : 3
                    height: button.activeFocus ? 34 : root.currentRoute === modelData.route ? 26 : 18
                    radius: width / 2
                    color: Theme.accent
                    opacity: button.activeFocus ? 1.0 : root.currentRoute === modelData.route ? 0.8 : 0.0
                    visible: opacity > 0
                    Behavior on height { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
                    Behavior on opacity { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
                }
                Rectangle {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: 3
                    height: button.activeFocus ? 30 : 0
                    radius: width / 2
                    color: Theme.textPrimary
                    opacity: button.activeFocus ? 0.9 : 0.0
                    visible: opacity > 0
                    Behavior on height { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
                    Behavior on opacity { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
