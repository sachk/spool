pragma Singleton
import QtQuick

QtObject {
    readonly property FontLoader interFont: FontLoader {
        source: Qt.resolvedUrl("../fonts/Inter-Variable.ttf")
    }

    readonly property string sans: interFont.name.length > 0 ? interFont.name : "sans-serif"
    readonly property string mono: "monospace"
}
