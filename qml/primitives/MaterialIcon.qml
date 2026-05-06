import QtQuick

Text {
    id: root

    property string name: ""
    property color iconColor: "#EEEEEE"
    property int iconSize: 32

    text: name
    color: iconColor
    font.family: materialIconsFont.name.length > 0 ? materialIconsFont.name : "Material Icons"
    font.pixelSize: iconSize
    font.hintingPreference: Font.PreferNoHinting
    renderType: Text.QtRendering
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter

    FontLoader {
        id: materialIconsFont
        source: Qt.resolvedUrl("../fonts/MaterialIcons-Regular.ttf")
    }
}
