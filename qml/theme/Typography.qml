pragma Singleton
import QtQuick

QtObject {
    id: root

    readonly property FontLoader plexSans: FontLoader {
        source: Qt.resolvedUrl("../fonts/IBMPlexSans-Variable.ttf")
    }
    // TRIAL: PT Sans as the UI face. Revert with
    // `git checkout qml/theme/Typography.qml CMakeLists.txt && rm qml/fonts/PTSans-*.ttf`.
    readonly property FontLoader uiSans: FontLoader {
        source: Qt.resolvedUrl("../fonts/PTSans-Regular.ttf")
        onStatusChanged: root.reportFontStatus(status, name)
    }
    readonly property FontLoader uiSansBold: FontLoader {
        source: Qt.resolvedUrl("../fonts/PTSans-Bold.ttf")
    }
    readonly property FontLoader subtitleFont: FontLoader {
        source: Qt.resolvedUrl("../fonts/AtkinsonHyperlegible-Regular.otf")
    }
    readonly property FontLoader materialIcons: FontLoader {
        source: Qt.resolvedUrl("../fonts/MaterialIcons-Regular.ttf")
    }

    readonly property string sans: uiSans.status === FontLoader.Ready ? uiSans.name : plexSans.status === FontLoader.Ready
                                                                        ? plexSans.name : "sans-serif"
    readonly property string subtitle: subtitleFont.status === FontLoader.Ready ? subtitleFont.name : sans
    readonly property string material: materialIcons.status === FontLoader.Ready ? materialIcons.name : "Material Icons"

    function reportFontStatus(status, family) {
        if (status === FontLoader.Ready)
            console.info("typography: loaded UI font as " + family)
        else if (status === FontLoader.Error)
            console.warn("typography: failed to load the UI font")
    }
}
