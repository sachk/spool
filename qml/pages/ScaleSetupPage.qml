pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root

    property var shell
    property int currentIndex: 1
    property string focusZone: "choices"
    readonly property string returnRoute: shell && shell.routeArgs ? String(shell.routeArgs.returnRoute || "") : ""
    readonly property var presets: [
        {
            "name": "Compact",
            "percent": 90
        },
        {
            "name": "Balanced",
            "percent": 110
        },
        {
            "name": "Large",
            "percent": 160
        }
    ]
    readonly property var selectedPreset: presets[currentIndex]

    focus: true

    Component.onCompleted: {
        let bestDistance = 1000
        for (let index = 0; index < presets.length; ++index) {
            const distance = Math.abs(presets[index].percent - Settings.uiScalePercent)
            if (distance < bestDistance) {
                currentIndex = index
                bestDistance = distance
            }
        }
        InputKeys.focus(root)
    }

    function choose(index) {
        currentIndex = Math.max(0, Math.min(presets.length - 1, index))
        focusZone = "choices"
        InputKeys.focus(root)
    }

    function confirm() {
        Settings.completeUiScaleSetup(selectedPreset.percent)
        Router.replace(returnRoute || (Session.authenticated ? "home" : "login"))
        if (shell)
            shell.focusContent()
    }

    function routeKey(key, phase, repeat) {
        if (key === Qt.Key_Left || key === Qt.Key_Right) {
            choose(currentIndex + (key === Qt.Key_Right ? 1 : -1))
            return true
        }
        if (key === Qt.Key_Down) {
            focusZone = "confirm"
            InputKeys.focus(confirmButton)
            return true
        }
        if (key === Qt.Key_Up) {
            focusZone = "choices"
            InputKeys.focus(root)
            return true
        }
        return false
    }

    function activate() {
        confirm()
    }

    function back() {
        if (returnRoute.length <= 0)
            return true
        Router.replace(returnRoute)
        if (shell)
            shell.focusContent()
        return true
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Math.round(root.width * 0.045)
        anchors.rightMargin: Math.round(root.width * 0.045)
        anchors.topMargin: Math.round(root.height * 0.055)
        anchors.bottomMargin: Math.round(root.height * 0.045)
        spacing: Math.round(root.height * 0.025)

        AppText {
            Layout.fillWidth: true
            text: "A comfortable zoom"
            font.pixelSize: Math.round(root.height * 0.055)
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Math.round(root.width * 0.014)

            Repeater {
                model: root.presets

                Rectangle {
                    id: choice

                    required property int index
                    required property var modelData
                    readonly property bool selected: index === root.currentIndex
                    readonly property real previewScale: Number(modelData.percent) / 110

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 24
                    color: selected ? Theme.accentDim : Theme.bgPanel
                    border.width: selected && root.focusZone === "choices" ? 5 : 2
                    border.color: selected ? Theme.textPrimary : Theme.border

                    Behavior on color {
                        ColorAnimation {
                            duration: 100
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 22
                        spacing: 16

                        AppText {
                            Layout.fillWidth: true
                            text: choice.modelData.name
                            font.pixelSize: 30
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Rectangle {
                            id: preview

                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 14
                            color: Theme.bg
                            border.width: 1
                            border.color: Theme.border
                            clip: true

                            Item {
                                id: sampleUi

                                width: preview.width / choice.previewScale
                                height: preview.height / choice.previewScale
                                scale: choice.previewScale
                                transformOrigin: Item.TopLeft

                                Rectangle {
                                    width: parent.width
                                    height: 54
                                    color: Theme.bgRaised

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.leftMargin: 24
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 92
                                        height: 14
                                        radius: 7
                                        color: Theme.jellyfinPurple
                                    }

                                    Row {
                                        anchors.right: parent.right
                                        anchors.rightMargin: 22
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 12

                                        Repeater {
                                            model: 3

                                            Rectangle {
                                                required property int index
                                                width: 14
                                                height: 14
                                                radius: 7
                                                color: index === 2 ? Theme.accent : Theme.textMuted
                                            }
                                        }
                                    }
                                }

                                Column {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.leftMargin: 26
                                    anchors.rightMargin: 26
                                    anchors.topMargin: 82
                                    spacing: 18

                                    Rectangle {
                                        width: 156
                                        height: 19
                                        radius: 9
                                        color: Theme.textPrimary
                                    }

                                    Row {
                                        spacing: 14

                                        Repeater {
                                            model: 6

                                            Column {
                                                required property int index
                                                spacing: 8

                                                Rectangle {
                                                    width: 116
                                                    height: 166
                                                    radius: 8
                                                    color: index === 0 ? Theme.jellyfinPurple : index % 2
                                                                         ? Theme.bgHover : Theme.bgRaised

                                                    Rectangle {
                                                        anchors.left: parent.left
                                                        anchors.right: parent.right
                                                        anchors.bottom: parent.bottom
                                                        height: 6
                                                        color: index < 2 ? Theme.accent : "transparent"
                                                    }
                                                }

                                                Rectangle {
                                                    width: index % 2 ? 78 : 98
                                                    height: 11
                                                    radius: 5
                                                    color: Theme.textSecondary
                                                }
                                            }
                                        }
                                    }

                                    Rectangle {
                                        width: 128
                                        height: 17
                                        radius: 8
                                        color: Theme.textPrimary
                                    }

                                    Row {
                                        spacing: 14

                                        Repeater {
                                            model: 5

                                            Rectangle {
                                                required property int index
                                                width: 178
                                                height: 100
                                                radius: 8
                                                color: index === 1 ? Theme.accentDim : Theme.bgRaised
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        AppText {
                            Layout.fillWidth: true
                            text: choice.modelData.percent + "%"
                            color: choice.selected ? Theme.textPrimary : Theme.textMuted
                            font.pixelSize: 22
                            font.weight: Font.Medium
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    TapHandler {
                        onTapped: root.choose(choice.index)
                    }
                }
            }
        }

        ActionButton {
            id: confirmButton

            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Math.round(root.width * 0.18)
            Layout.preferredHeight: Math.round(root.height * 0.06)
            kind: "primary"
            iconName: "check"
            text: "Use " + root.selectedPreset.name
            onClicked: root.confirm()
        }
    }
}
