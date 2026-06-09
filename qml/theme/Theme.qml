pragma Singleton
import QtQuick

QtObject {
    readonly property color bg: "#0E0E0E"
    readonly property color bgRaised: "#161616"
    readonly property color bgPanel: "#1C1C1C"
    readonly property color bgHover: "#242424"
    readonly property color border: "#2A2A2A"
    readonly property color borderStrong: "#3A3A3A"

    readonly property color textPrimary: "#E8E8E8"
    readonly property color textSecondary: "#A0A0A0"
    readonly property color textMuted: "#6A6A6A"
    readonly property color textDisabled: "#4A4A4A"
    readonly property color onAccent: "#061017"

    readonly property color jellyfinBlue: "#00A4DC"
    readonly property color jellyfinPurple: "#AA5CC3"
    property int accentIndex: 0
    readonly property color accent: accentIndex === 1 ? jellyfinPurple : accentIndex === 2 ? "#7E7CFF" : jellyfinBlue
    readonly property color accentDim: accentIndex === 1 ? "#78408A" : accentIndex === 2 ? "#4F4DA8" : "#0077A0"
    readonly property color accentPurple: accentIndex === 1 ? jellyfinBlue : jellyfinPurple
    readonly property color accentPanel: accentIndex === 1 ? "#2C1E31" : accentIndex === 2 ? "#20223C" : "#182A32"
    readonly property color success: "#3FB950"
    readonly property color errorPanel: "#2A1717"
    readonly property color errorText: "#FFD6D6"

    readonly property color controlOutline: "#55FFFFFF"
    readonly property color focusedFill: "#30383D"
    readonly property color floatingPanel: "#F0181818"
    readonly property color overlayScrim: "#AA000000"
    readonly property color overlayScrimStrong: "#CC000000"
    readonly property color busyScrim: "#AA0E0E0E"

    readonly property int radiusTiny: 2
    readonly property int radiusSmall: 4
    readonly property int radiusMedium: 6
    readonly property int radiusLarge: 10
    readonly property int radiusPanel: 12
    readonly property int focusBorderWidth: 2
    readonly property int hoverBorderWidth: 1

    property int normalTextRenderType: Text.QtRendering
    property int largeTextRenderType: Text.QtRendering
    property bool reducedMotion: false
    property bool antialiasedText: true
    property string technicalMetadataMode: "Always"
    property string sideRailLabels: "On focus"
}
