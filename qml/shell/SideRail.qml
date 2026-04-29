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
                { label: "Libraries", route: "libraries", icon: "L" },
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
                    selected: root.currentRoute === modelData.route
                    onClicked: root.navigate(modelData.route)
                    Keys.onReleased: (event) => {
                        if (event.key === Qt.Key_Right) {
                            root.contentRequested()
                            event.accepted = true
                        }
                    }
                }
                Rectangle {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: 3
                    height: 24
                    color: Theme.accent
                    visible: root.currentRoute === modelData.route
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
