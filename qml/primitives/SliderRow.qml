import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import "../theme"

T.Control {
    id: root

    property string title: ""
    property string description: ""
    property real value: 1.0
    property real from: 0.75
    property real to: 1.5
    property real step: 0.05
    property int decimals: 2
    property string unitText: "x"
    property int settingIndex: -1
    property bool selected: false
    readonly property bool rowFocus: selected || activeFocus || valueField.activeFocus || valueSlider.activeFocus
    property bool handledNavigationPress: false
    property int valueBoxWidth: 86
    property int sliderPreferredWidth: 300
    property real controlValue: value

    signal valueEdited(real value)

    focus: true
    focusPolicy: Qt.StrongFocus
    implicitHeight: Math.max(74, contentRow.implicitHeight + 24)
    leftPadding: 12
    rightPadding: 12
    topPadding: 12
    bottomPadding: 12
    readonly property int textWidth: root.Window.window ? root.Window.window.width : 1920
    readonly property int valueFontPx: Metrics.metaPx(textWidth) + 1

    background: Surface {
        focused: root.rowFocus
        hovered: hover.hovered
    }

    function clamp(value) {
        return Math.max(from, Math.min(to, Number(value)))
    }

    function rounded(value) {
        const safeStep = Math.max(0.0001, Number(step || 1))
        return clamp(Math.round(Number(value || 0) / safeStep) * safeStep)
    }

    function formatValue(value) {
        return Number(value).toFixed(decimals)
    }

    function setSliderValue(next) {
        const roundedValue = rounded(next)
        controlValue = roundedValue
        valueField.text = formatValue(roundedValue)
        valueEdited(roundedValue)
    }

    function adjust(dir) {
        setSliderValue(controlValue + step * dir)
    }

    function handleNavigationKey(key) {
        if (key === Qt.Key_Left || key === Qt.Key_Right) {
            handledNavigationPress = true
            adjust(key === Qt.Key_Right ? 1 : -1)
            return true
        }
        if (InputKeys.isAccept(key)) {
            valueSlider.forceActiveFocus()
            return true
        }
        return false
    }

    onValueChanged: {
        const formatted = formatValue(value)
        controlValue = value
        if (valueField.text !== formatted)
            valueField.text = formatted
    }

    HoverHandler { id: hover }
    TapHandler {
        onTapped: root.forceActiveFocus()
    }

    contentItem: RowLayout {
        id: contentRow
        spacing: 14

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            AppText {
                Layout.fillWidth: true
                text: root.title
                font.pixelSize: Metrics.bodyPx(root.textWidth)
                font.weight: Font.Medium
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            AppText {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.description
                color: Theme.textMuted
                font.pixelSize: Metrics.metaPx(root.textWidth)
                maximumLineCount: 1
                elide: Text.ElideRight
            }
        }

        T.TextField {
            id: valueField
            Layout.preferredWidth: root.valueBoxWidth
            Layout.preferredHeight: 30
            text: root.formatValue(root.controlValue)
            horizontalAlignment: TextInput.AlignRight
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            validator: DoubleValidator {
                bottom: root.from
                top: root.to
                decimals: root.decimals
                notation: DoubleValidator.StandardNotation
            }
            color: Theme.textPrimary
            selectedTextColor: Theme.textPrimary
            selectionColor: Theme.accentDim
            font.pixelSize: root.valueFontPx
            font.weight: Font.DemiBold
            background: Rectangle {
                radius: Theme.radiusSmall
                color: Theme.bgRaised
                border.width: valueField.activeFocus ? 2 : 1
                border.color: valueField.activeFocus ? Theme.accent : Theme.border
            }
            onAccepted: root.setSliderValue(text)
            onEditingFinished: root.setSliderValue(text)
            Keys.onReleased: (event) => {
                if (event.key === Qt.Key_Right) {
                    valueSlider.forceActiveFocus()
                    event.accepted = true
                }
            }
        }

        AppText {
            visible: root.unitText.length > 0
            Layout.preferredWidth: visible ? Math.max(18, implicitWidth) : 0
            text: root.unitText
            color: Theme.textPrimary
            font.pixelSize: root.valueFontPx
            font.weight: Font.DemiBold
            maximumLineCount: 1
            verticalAlignment: Text.AlignVCenter
        }

        T.Slider {
            id: valueSlider
            Layout.preferredWidth: Math.min(root.sliderPreferredWidth, Math.max(180, root.width * 0.28))
            from: root.from
            to: root.to
            stepSize: root.step
            value: root.controlValue
            focusPolicy: Qt.StrongFocus

            background: Rectangle {
                x: valueSlider.leftPadding
                y: valueSlider.topPadding + valueSlider.availableHeight / 2 - height / 2
                width: valueSlider.availableWidth
                height: valueSlider.activeFocus ? 9 : 7
                radius: height / 2
                color: Theme.borderStrong

                Rectangle {
                    width: valueSlider.visualPosition * parent.width
                    height: parent.height
                    radius: parent.radius
                    color: Theme.accent
                }
            }

            handle: Rectangle {
                x: valueSlider.leftPadding + valueSlider.visualPosition * (valueSlider.availableWidth - width)
                y: valueSlider.topPadding + valueSlider.availableHeight / 2 - height / 2
                width: valueSlider.activeFocus ? 24 : 20
                height: width
                radius: width / 2
                color: Theme.textPrimary
                border.width: 3
                border.color: Theme.accent
            }

            onMoved: root.setSliderValue(value)
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
        } else if (InputKeys.isAccept(event.key, false)) {
            valueSlider.forceActiveFocus()
            event.accepted = true
        }
    }
}
