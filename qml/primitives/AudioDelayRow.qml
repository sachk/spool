import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../theme"

Surface {
    id: root
    property string title: "Audio delay"
    property string description: ""
    property int valueMs: 0
    property int settingIndex: -1
    property bool rowFocus: activeFocus || delayField.activeFocus || delaySlider.activeFocus
    property bool handledNavigationPress: false
    signal valueEdited(int valueMs)
    focused: rowFocus
    focus: true
    implicitHeight: 78
    readonly property int valueFontPx: Metrics.bodyPx(root.Window.window ? root.Window.window.width : 1920) + 3

    function clampMs(value) {
        return Math.max(-2000, Math.min(2000, Math.round(Number(value) || 0)))
    }

    function setDelay(value) {
        const next = clampMs(value)
        delayField.text = String(next)
        valueEdited(next)
    }

    function adjust(steps) {
        setDelay(valueMs + steps * 10)
    }

    function handleNavigationKey(key) {
        if (key === Qt.Key_Left || key === Qt.Key_Right) {
            handledNavigationPress = true
            adjust(key === Qt.Key_Right ? 1 : -1)
            return true
        }
        return false
    }

    HoverHandler { id: hover }
    hovered: hover.hovered
    onValueMsChanged: delayField.text = String(valueMs)

    TapHandler {
        onTapped: root.forceActiveFocus()
    }

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

        TextField {
            id: delayField
            Layout.preferredWidth: 136
            text: String(root.valueMs)
            horizontalAlignment: TextInput.AlignRight
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            validator: IntValidator { bottom: -2000; top: 2000 }
            color: Theme.textPrimary
            selectedTextColor: Theme.textPrimary
            selectionColor: Theme.accentDim
            font.pixelSize: root.valueFontPx
            font.weight: Font.DemiBold
            background: Rectangle {
                radius: Theme.radiusSmall
                color: Theme.bgRaised
                border.width: delayField.activeFocus ? 2 : 1
                border.color: delayField.activeFocus ? Theme.accent : Theme.border
            }
            onAccepted: root.setDelay(text)
            onEditingFinished: root.setDelay(text)
            Keys.onReleased: (event) => {
                if (event.key === Qt.Key_Right) {
                    delaySlider.forceActiveFocus()
                    event.accepted = true
                }
            }
        }

        AppText {
            text: "ms"
            color: Theme.textPrimary
            font.pixelSize: root.valueFontPx
            font.weight: Font.DemiBold
        }

        Slider {
            id: delaySlider
            Layout.preferredWidth: Math.min(360, Math.max(210, root.width * 0.3))
            from: -2000
            to: 2000
            stepSize: 10
            value: root.valueMs
            focusPolicy: Qt.StrongFocus
            background: Rectangle {
                x: delaySlider.leftPadding
                y: delaySlider.topPadding + delaySlider.availableHeight / 2 - height / 2
                width: delaySlider.availableWidth
                height: delaySlider.activeFocus ? 10 : 8
                radius: height / 2
                color: Theme.borderStrong

                Rectangle {
                    width: delaySlider.visualPosition * parent.width
                    height: parent.height
                    radius: parent.radius
                    color: Theme.accent
                }
            }
            handle: Rectangle {
                x: delaySlider.leftPadding + delaySlider.visualPosition * (delaySlider.availableWidth - width)
                y: delaySlider.topPadding + delaySlider.availableHeight / 2 - height / 2
                width: delaySlider.activeFocus ? 28 : 24
                height: width
                radius: width / 2
                color: Theme.textPrimary
                border.width: 3
                border.color: Theme.accent
            }
            onMoved: root.setDelay(Math.round(value / 10) * 10)
            Keys.onReleased: (event) => {
                if (event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
                    if (root.handledNavigationPress) {
                        root.handledNavigationPress = false
                        event.accepted = true
                        return
                    }
                    root.adjust(event.key === Qt.Key_Right ? 1 : -1)
                    event.accepted = true
                } else if (InputKeys.isAccept(event.key, false)) {
                    root.forceActiveFocus()
                    event.accepted = true
                }
            }
        }
    }

    Keys.onReleased: (event) => {
        if (event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
            if (handledNavigationPress) {
                handledNavigationPress = false
                event.accepted = true
                return
            }
            root.adjust(event.key === Qt.Key_Right ? 1 : -1)
            event.accepted = true
        } else if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
            event.accepted = false
        } else if (InputKeys.isAccept(event.key, false)) {
            delaySlider.forceActiveFocus()
            event.accepted = true
        }
    }
}
