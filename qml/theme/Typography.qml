pragma Singleton
import QtQuick

QtObject {
    id: root

    // Full hinting keeps small UI text aligned to the pixel grid on FreeType
    // platforms such as webOS. DirectWrite's full horizontal snapping
    // distorts this face, so Windows keeps vertical-only hinting.
    readonly property int sansHinting: Qt.platform.os === "windows" ? Font.PreferVerticalHinting :
                                                                      Font.PreferFullHinting
    readonly property string sans: "PT Root UI"
    readonly property string fallbackSans: "IBM Plex Sans"
    readonly property string subtitle: "Atkinson Hyperlegible"
    readonly property string material: "Material Icons"
}
