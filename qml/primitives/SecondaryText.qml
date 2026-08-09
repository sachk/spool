import QtQuick
import "../theme"

Text {
    color: Theme.textSecondary
    textFormat: Text.PlainText
    antialiasing: Theme.antialiasedText
    renderType: Theme.normalTextRenderType
    font.family: Typography.sans
    font.hintingPreference: Typography.sansHinting
    font.preferTypoLineMetrics: true
}
