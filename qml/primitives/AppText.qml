import QtQuick
import "../theme"

Text {
    color: Theme.textPrimary
    textFormat: Text.PlainText
    antialiasing: Theme.antialiasedText
    renderType: Theme.normalTextRenderType
    font.family: Typography.sans
    font.preferTypoLineMetrics: true
}
