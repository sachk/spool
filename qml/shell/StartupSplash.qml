import QtQuick

Rectangle {
    width: 1920
    height: 1080
    color: "black"

    Image {
        anchors.fill: parent
        source: "splash.png"
        fillMode: Image.PreserveAspectFit
        asynchronous: false
        cache: true
    }
}
