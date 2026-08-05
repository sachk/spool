pragma Singleton
import QtQuick

QtObject {
    id: root

    readonly property FontLoader plexSans: FontLoader {
        source: Qt.resolvedUrl("../fonts/IBMPlexSans-Variable.ttf")
        onStatusChanged: root.reportFontStatus(status, name)
    }
    readonly property FontLoader subtitleFont: FontLoader {
        source: Qt.resolvedUrl("../fonts/AtkinsonHyperlegible-Regular.otf")
    }
    readonly property FontLoader materialIcons: FontLoader {
        source: Qt.resolvedUrl("../fonts/MaterialIcons-Regular.ttf")
    }

    readonly property string sans: plexSans.status === FontLoader.Ready ? plexSans.name : "sans-serif"
    readonly property string subtitle: subtitleFont.status === FontLoader.Ready ? subtitleFont.name : sans
    readonly property string material: materialIcons.status === FontLoader.Ready ? materialIcons.name : "Material Icons"

    function reportFontStatus(status, family) {
        if (status === FontLoader.Ready)
            console.info("typography: loaded IBM Plex Sans variable font as " + family)
        else if (status === FontLoader.Error)
            console.warn("typography: failed to load IBM Plex Sans variable font")
    }
}
