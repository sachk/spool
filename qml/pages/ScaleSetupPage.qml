pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root

    property var shell
    property int currentIndex: 1
    property string focusZone: "presets"
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
    readonly property real previewScale: Number(selectedPreset.percent) / 100

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
    }

    function confirm() {
        Settings.completeUiScaleSetup(selectedPreset.percent)
        Router.replace(returnRoute || (Session.authenticated ? "home" : "login"))
        if (shell)
            shell.focusContent()
    }

    function routeKey(key, phase, repeat) {
        if (key === Qt.Key_Left || key === Qt.Key_Right) {
            focusZone = "presets"
            InputKeys.focus(root)
            choose(currentIndex + (key === Qt.Key_Right ? 1 : -1))
            return true
        }
        if (key === Qt.Key_Down) {
            focusZone = "confirm"
            InputKeys.focus(confirmButton)
            return true
        }
        if (key === Qt.Key_Up) {
            focusZone = "presets"
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
        anchors.margins: Metrics.pageMarginPx
        spacing: Metrics.sectionGapPx

        AppText {
            Layout.fillWidth: true
            text: "Choose a comfortable zoom"
            font.pixelSize: Metrics.titleSizePx + Metrics.scaled(18)
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Metrics.gapPx

            ColumnLayout {
                Layout.preferredWidth: Metrics.scaled(260)
                spacing: Metrics.scaled(12)

                Repeater {
                    model: root.presets
                    ActionButton {
                        required property int index
                        required property var modelData
                        Layout.fillWidth: true
                        kind: index === root.currentIndex ? "primary" : "secondary"
                        text: modelData.name + "  " + modelData.percent + "%"
                        onClicked: {
                            root.focusZone = "presets"
                            root.choose(index)
                            InputKeys.focus(root)
                        }
                    }
                }
            }

            Surface {
                Layout.fillWidth: true
                Layout.fillHeight: true
                focused: root.focusZone === "presets"
                elevated: true
                baseColor: Theme.bgPanel

                Column {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - Metrics.scaled(48), Metrics.scaled(720))
                    spacing: Metrics.scaled(12) * root.previewScale

                    AppText {
                        width: parent.width
                        text: root.selectedPreset.name + " preview"
                        font.pixelSize: Math.round(Metrics.bodySizePx * root.previewScale)
                        font.weight: Font.DemiBold
                    }
                    Rectangle {
                        width: parent.width
                        height: Metrics.scaled(150) * root.previewScale
                        radius: Theme.radiusMedium
                        color: Theme.accentDim
                    }
                    AppText {
                        text: "Recently Added"
                        font.pixelSize: Math.round(Metrics.bodySizePx * root.previewScale)
                        font.weight: Font.DemiBold
                    }
                    Row {
                        width: parent.width
                        height: Metrics.scaled(150) * root.previewScale
                        spacing: Metrics.scaled(10) * root.previewScale
                        Repeater {
                            model: 4
                            Rectangle {
                                required property int index
                                width: (parent.width - parent.spacing * 3) / 4
                                height: parent.height
                                radius: Theme.radiusSmall
                                color: index === 0 ? Theme.jellyfinPurple : Theme.bgHover
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            MonoText {
                Layout.fillWidth: true
                text: "Left / Right to compare  •  Enter to continue"
                color: Theme.textMuted
                font.pixelSize: Metrics.metaSizePx
            }
            ActionButton {
                id: confirmButton
                Layout.preferredWidth: Metrics.scaled(210)
                kind: "primary"
                iconName: "check"
                text: "Use " + root.selectedPreset.name
                onClicked: root.confirm()
            }
        }
    }
}
