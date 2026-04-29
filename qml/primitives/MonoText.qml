import QtQuick
import "../theme"

Text {
    color: Theme.textSecondary
    textFormat: Text.PlainText
    antialiasing: Theme.antialiasedText
    renderType: Theme.normalTextRenderType
    font.family: Typography.mono
    font.preferTypoLineMetrics: true
}
