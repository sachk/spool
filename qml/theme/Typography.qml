pragma Singleton
import QtQuick

QtObject {
    id: root

    readonly property FontLoader interRegular: FontLoader {
        source: Qt.resolvedUrl("../fonts/Inter-Regular.ttf")
        onStatusChanged: root.reportFontStatus("regular", status, name)
    }
    readonly property FontLoader interMedium: FontLoader {
        source: Qt.resolvedUrl("../fonts/Inter-Medium.ttf")
        onStatusChanged: root.reportFontStatus("medium", status, name)
    }
    readonly property FontLoader interSemiBold: FontLoader {
        source: Qt.resolvedUrl("../fonts/Inter-SemiBold.ttf")
        onStatusChanged: root.reportFontStatus("semibold", status, name)
    }
    readonly property FontLoader interBold: FontLoader {
        source: Qt.resolvedUrl("../fonts/Inter-Bold.ttf")
        onStatusChanged: root.reportFontStatus("bold", status, name)
    }
    readonly property FontLoader materialIcons: FontLoader {
        source: Qt.resolvedUrl("../fonts/MaterialIcons-Regular.ttf")
    }

    readonly property string sans: interRegular.status === FontLoader.Ready ? interRegular.name : "sans-serif"
    readonly property string material: materialIcons.status === FontLoader.Ready ? materialIcons.name : "Material Icons"

    function reportFontStatus(face, status, family) {
        if (status === FontLoader.Ready)
            console.info("typography: loaded Inter " + face + " as " + family)
        else if (status === FontLoader.Error)
            console.warn("typography: failed to load Inter " + face)
    }
    readonly property string mono: "monospace"
}
