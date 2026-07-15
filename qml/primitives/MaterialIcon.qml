import QtQuick

Text {
    id: root

    property string name: ""
    property color iconColor: "#EEEEEE"
    property int iconSize: 32

    text: name
    color: iconColor
    font.family: Typography.material
    font.pixelSize: iconSize
    font.hintingPreference: Font.PreferNoHinting
    renderType: Text.QtRendering
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
}
