pragma Singleton
import QtQuick

QtObject {
    id: root

    property int sansHinting: Font.PreferFullHinting
    readonly property string sans: "PT Root UI VF"
    readonly property string fallbackSans: "IBM Plex Sans"
    readonly property string subtitle: "Atkinson Hyperlegible"
    readonly property string material: "Material Icons"
}
