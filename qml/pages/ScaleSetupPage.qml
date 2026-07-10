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
            "percent": 85,
            "description": "More titles on screen"
        },
        {
            "name": "Balanced",
            "percent": 100,
            "description": "The recommended layout"
        },
        {
            "name": "Relaxed",
            "percent": 115,
            "description": "Larger type and controls"
        }
    ]

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
        Settings.completeUiScaleSetup(presets[currentIndex].percent)
        Router.replace(returnRoute || "home")
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

        Rectangle {
            width: parent.width * 0.72
            height: width
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.top
            anchors.verticalCenterOffset: -height * 0.28
            radius: width / 2
            color: Theme.accentPanel
            opacity: 0.36
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Metrics.pageMargin(width) * 1.5
        anchors.rightMargin: Metrics.pageMargin(width) * 1.5
        anchors.topMargin: Metrics.scaled(46)
        anchors.bottomMargin: Metrics.scaled(36)
        spacing: Metrics.sectionGap(root.width)

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Metrics.scaled(8)

            AppText {
                Layout.fillWidth: true
                text: "Make Jellyfin yours"
                font.pixelSize: Metrics.titlePx(root.width) + Metrics.scaled(8)
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }

            AppText {
                Layout.fillWidth: true
                text: "Choose the density that feels right from your seat. You can change it later in Settings."
                color: Theme.textSecondary
                font.pixelSize: Metrics.bodyPx(root.width)
                horizontalAlignment: Text.AlignHCenter
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Metrics.gap(root.width)

            Repeater {
                model: root.presets

                Surface {
                    id: presetCard

                    required property int index
                    required property var modelData
                    readonly property real previewScale: Number(modelData.percent) / 100
                    readonly property bool selected: index === root.currentIndex

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumWidth: Metrics.scaled(250)
                    focused: selected && root.focusZone === "presets"
                    hovered: hover.hovered
                    elevated: selected
                    baseColor: selected ? Theme.accentPanel : Theme.bgPanel

                    TapHandler {
                        onTapped: {
                            root.focusZone = "presets"
                            root.choose(presetCard.index)
                            InputKeys.focus(root)
                        }
                    }

                    HoverHandler {
                        id: hover
                        onHoveredChanged: if (hovered)
                        root.choose(presetCard.index)
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Metrics.scaled(18)
                        spacing: Metrics.scaled(12)

                        RowLayout {
                            Layout.fillWidth: true

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Metrics.scaled(2)

                                AppText {
                                    text: presetCard.modelData.name
                                    font.pixelSize: Metrics.bodyPx(root.width) + Metrics.scaled(3)
                                    font.weight: Font.DemiBold
                                }

                                MonoText {
                                    text: presetCard.modelData.description
                                    color: Theme.textSecondary
                                    font.pixelSize: Metrics.metaPx(root.width)
                                }
                            }

                            Surface {
                                Layout.preferredWidth: Metrics.scaled(58)
                                Layout.preferredHeight: Metrics.scaled(30)
                                baseColor: presetCard.selected ? Theme.accent : Theme.bgRaised

                                MonoText {
                                    anchors.centerIn: parent
                                    text: presetCard.modelData.percent + "%"
                                    color: presetCard.selected ? Theme.accentText : Theme.textSecondary
                                    font.weight: Font.DemiBold
                                }
                            }
                        }

                        Surface {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            baseColor: Theme.bg
                            clip: true

                            Column {
                                anchors.fill: parent
                                anchors.margins: Metrics.scaled(12)
                                spacing: Metrics.scaled(9) * presetCard.previewScale

                                Row {
                                    width: parent.width
                                    height: Metrics.scaled(24) * presetCard.previewScale
                                    spacing: Metrics.scaled(6) * presetCard.previewScale

                                    Rectangle {
                                        width: height
                                        height: parent.height
                                        radius: height / 2
                                        color: Theme.accent
                                    }

                                    Repeater {
                                        model: 3
                                        Rectangle {
                                            required property int index
                                            width: Metrics.scaled(38) * presetCard.previewScale
                                            height: parent.height * 0.42
                                            anchors.verticalCenter: parent.verticalCenter
                                            radius: height / 2
                                            color: index === 0 ? Theme.textSecondary : Theme.borderStrong
                                        }
                                    }
                                }

                                Rectangle {
                                    width: parent.width
                                    height: parent.height * 0.28
                                    radius: Theme.radiusMedium
                                    gradient: Gradient {
                                        orientation: Gradient.Horizontal
                                        GradientStop {
                                            position: 0
                                            color: Theme.accentDim
                                        }
                                        GradientStop {
                                            position: 1
                                            color: Theme.jellyfinPurple
                                        }
                                    }

                                    Column {
                                        anchors.left: parent.left
                                        anchors.bottom: parent.bottom
                                        anchors.margins: Metrics.scaled(12) * presetCard.previewScale
                                        spacing: Metrics.scaled(3) * presetCard.previewScale

                                        Rectangle {
                                            width: Metrics.scaled(112) * presetCard.previewScale
                                            height: Metrics.scaled(9) * presetCard.previewScale
                                            radius: height / 2
                                            color: Theme.textPrimary
                                        }
                                        Rectangle {
                                            width: Metrics.scaled(78) * presetCard.previewScale
                                            height: Metrics.scaled(6) * presetCard.previewScale
                                            radius: height / 2
                                            color: Theme.textSecondary
                                        }
                                    }
                                }

                                AppText {
                                    width: parent.width
                                    text: "Recently Added"
                                    font.pixelSize: Math.round(15 * presetCard.previewScale)
                                    font.weight: Font.DemiBold
                                }

                                Row {
                                    width: parent.width
                                    height: Math.max(0, parent.height - y)
                                    spacing: Metrics.scaled(8) * presetCard.previewScale

                                    Repeater {
                                        model: presetCard.modelData.percent >= 115 ? 3 : 4

                                        Column {
                                            required property int index
                                            width: (parent.width - parent.spacing * (presetCard.modelData.percent
                                                                                     >= 115 ? 2 : 3)) / (
                                                       presetCard.modelData.percent >= 115 ? 3 : 4)
                                            spacing: Metrics.scaled(4) * presetCard.previewScale

                                            Rectangle {
                                                width: parent.width
                                                height: Math.min(parent.parent.height * 0.72, width * 1.5)
                                                radius: Theme.radiusSmall
                                                color: index === 0 ? Theme.accentDim : index === 1 ? Theme.bgHover :
                                                                                                     Theme.border
                                            }
                                            Rectangle {
                                                width: parent.width * 0.76
                                                height: Metrics.scaled(5) * presetCard.previewScale
                                                radius: height / 2
                                                color: Theme.textSecondary
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.gap(root.width)

            MonoText {
                Layout.fillWidth: true
                text: "Left / Right to compare  •  Enter to continue"
                color: Theme.textMuted
                font.pixelSize: Metrics.metaPx(root.width)
            }

            ActionButton {
                id: confirmButton

                Layout.preferredWidth: Metrics.scaled(210)
                kind: "primary"
                iconName: "check"
                text: "Use " + root.presets[root.currentIndex].name
                onClicked: root.confirm()
            }
        }
    }
}
