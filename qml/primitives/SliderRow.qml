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
    property int valueBoxWidth: 86
    property int sliderPreferredWidth: 300
    property real controlValue: value
    property int metricsWidth: 1920

    signal valueEdited(real value)

    focus: true
    focusPolicy: Qt.StrongFocus
    implicitHeight: Math.max(Metrics.scaled(74), contentRow.implicitHeight + Metrics.scaled(24))
    leftPadding: Metrics.scaled(12)
    rightPadding: Metrics.scaled(12)
    topPadding: Metrics.scaled(12)
    bottomPadding: Metrics.scaled(12)
    readonly property int valueFontPx: Metrics.metaPx(metricsWidth) + 1

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

    onValueChanged: {
        const formatted = formatValue(value)
        controlValue = value
        if (valueField.text !== formatted)
            valueField.text = formatted
    }

    HoverHandler {
        id: hover
    }
    TapHandler {
        onTapped: InputKeys.focus(root)
    }

    contentItem: RowLayout {
        id: contentRow
        spacing: Metrics.scaled(14)

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Metrics.scaled(3)

            AppText {
                Layout.fillWidth: true
                text: root.title
                font.pixelSize: Metrics.bodyPx(root.metricsWidth)
                font.weight: Font.Medium
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            AppText {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.description
                color: Theme.textMuted
                font.pixelSize: Metrics.metaPx(root.metricsWidth)
                maximumLineCount: 1
                elide: Text.ElideRight
            }
        }

        T.TextField {
            id: valueField
            Layout.preferredWidth: Metrics.scaled(root.valueBoxWidth)
            Layout.preferredHeight: Metrics.scaled(30)
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
                border.width: valueField.activeFocus ? Theme.focusBorderWidth : Theme.hoverBorderWidth
                border.color: valueField.activeFocus ? Theme.accent : Theme.border
            }
            onAccepted: root.setSliderValue(text)
            onEditingFinished: root.setSliderValue(text)
        }

        AppText {
            visible: root.unitText.length > 0
            Layout.preferredWidth: visible ? Math.max(Metrics.scaled(18), implicitWidth) : 0
            text: root.unitText
            color: Theme.textPrimary
            font.pixelSize: root.valueFontPx
            font.weight: Font.DemiBold
            maximumLineCount: 1
            verticalAlignment: Text.AlignVCenter
        }

        T.Slider {
            id: valueSlider
            Layout.preferredWidth: Math.min(Metrics.scaled(root.sliderPreferredWidth), Math.max(Metrics.scaled(180),
                                                                                                root.width * 0.28))
            from: root.from
            to: root.to
            stepSize: root.step
            value: root.controlValue
            focusPolicy: Qt.StrongFocus

            background: Rectangle {
                x: valueSlider.leftPadding
                y: valueSlider.topPadding + valueSlider.availableHeight / 2 - height / 2
                width: valueSlider.availableWidth
                height: valueSlider.activeFocus ? Metrics.scaled(9) : Metrics.scaled(7)
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
                width: Metrics.scaled(valueSlider.activeFocus ? 24 : 20)
                height: width
                radius: width / 2
                color: Theme.textPrimary
                border.width: Theme.focusBorderWidth
                border.color: Theme.accent
            }

            onMoved: root.setSliderValue(value)
        }
    }
}
