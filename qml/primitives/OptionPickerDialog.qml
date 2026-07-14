pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "../theme"

FocusScope {
    id: root

    property string title: "Choose an option"
    property var options: []
    property int currentIndex: 0
    signal selected(int index)
    signal dismissed

    anchors.fill: parent
    focus: true
    z: 100

    function focusCurrent() {
        const count = optionList.count
        optionList.currentIndex = count > 0 ? Math.max(0, Math.min(root.currentIndex, count - 1)) : -1
        if (optionList.currentIndex >= 0)
            optionList.positionViewAtIndex(optionList.currentIndex, ListView.Contain)
        InputKeys.focus(optionList)
    }

    function routeKey(key, phase, repeat) {
        if (phase !== "release")
            return InputKeys.isDirection(key) || InputKeys.isAccept(key)
        if (InputKeys.isBack(key, false, false)) {
            dismissed()
            return true
        }
        if (InputKeys.isDirection(key))
            return optionList.routeKey(key, phase, repeat)
        if (InputKeys.isAccept(key)) {
            optionList.activate()
            return true
        }
        return false
    }

    function activate() {
        optionList.activate()
    }

    function back() {
        dismissed()
        return true
    }

    onVisibleChanged: if (visible)
    Qt.callLater(focusCurrent)
    Component.onCompleted: Qt.callLater(focusCurrent)

    Rectangle {
        anchors.fill: parent
        color: "#b3000000"

        TapHandler {
            onTapped: root.dismissed()
        }
    }

    Surface {
        anchors.centerIn: parent
        width: Math.min(parent.width - Metrics.scaled(48), Metrics.scaled(760))
        height: Math.min(parent.height - Metrics.scaled(48), Math.max(Metrics.scaled(300), Math.min(Metrics.scaled(760),
                                                                                                    optionList.contentHeight
                                                                                                    + heading.implicitHeight
                                                                                                    + Metrics.scaled(
                                                                                                        92))))
        elevated: true
        baseColor: Theme.bgPanel

        MouseArea {
            anchors.fill: parent
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Metrics.scaled(24)
            spacing: Metrics.scaled(14)

            AppText {
                id: heading
                Layout.fillWidth: true
                text: root.title
                color: Theme.textPrimary
                font.pixelSize: Metrics.titlePx(root.width)
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }

            AppText {
                Layout.fillWidth: true
                text: "Choose one option"
                color: Theme.textMuted
                font.pixelSize: Metrics.metaPx(root.width)
            }

            MenuListView {
                id: optionList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: root.options
                currentIndex: root.currentIndex
                onDismissed: root.dismissed()
                onAccepted: index => root.selected(index)

                delegate: MenuRow {
                    required property int index
                    required property var modelData
                    width: optionList.width
                    label: String(modelData)
                    checked: index === root.currentIndex
                    highlighted: optionList.activeFocus && optionList.currentIndex === index
                    metricsWidth: root.width
                    onHovered: optionList.currentIndex = index
                    onActivated: root.selected(index)
                }
            }
        }
    }
}
