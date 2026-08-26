import QtQuick
import QtQuick.Layouts
import "../theme"

// A media item as a single line: artwork, a title, a subtitle, and a few
// trailing actions that appear when the row is reached.
//
// This is the shape browsing takes when there is not room for a grid of
// cards -- a sheet beside the player, or a phone. Cards say more per item;
// rows fit more items and leave room at the end of each for the actions that
// would otherwise need a menu of their own.
Item {
    id: row

    required property var item
    // Artwork shape: "poster", "square" or "landscape".
    property string kind: "landscape"
    property string titleOverride: ""
    property string subtitleOverride: ""
    property string fallbackIcon: ""
    property bool highlighted: false
    property bool current: false
    property bool showChevron: false
    property real progress: -1

    // [{ action, icon, label }] -- shown at the end of the row.
    property var actions: []
    // Which trailing action holds focus, or -1 for the row itself. Kept here
    // rather than in the list so a row can be reached by pointer and by
    // remote without the two disagreeing about where focus is.
    property int actionIndex: -1

    readonly property int actionCount: actions ? actions.length : 0
    readonly property bool actionsVisible: highlighted || hover.hovered
    readonly property bool square: kind === "square"
    readonly property bool poster: kind === "poster"
    readonly property real artHeight: Metrics.scaled(44)
    readonly property real artWidth: square ? artHeight : poster ? Math.round(artHeight / 1.5) : Math.round(artHeight
                                                                                                            * 16 / 9)

    signal activated
    signal actionTriggered(string action)

    implicitHeight: Math.max(Metrics.controlHeightPx, artHeight + Metrics.scaled(14))

    function value(field) {
        return item && item[field] !== undefined && item[field] !== null ? String(item[field]) : ""
    }

    function titleText() {
        if (titleOverride.length > 0)
            return titleOverride
        const title = value("title")
        const series = value("seriesName")
        if (value("itemType") === "Episode" && series.length > 0 && title.length > 0)
            return title
        return title.length > 0 ? title : series
    }

    function subtitleText() {
        if (subtitleOverride.length > 0)
            return subtitleOverride
        const type = value("itemType")
        if (type === "Episode") {
            const code = value("episodeCode")
            const series = value("seriesName")
            return [series, code].filter(part => part.length > 0).join(" · ")
        }
        if (type === "Season" || type === "Series") {
            const count = Number(item && item.recursiveItemCount ? item.recursiveItemCount : 0)
            return count > 0 ? (count === 1 ? "1 item" : count + " items") : value("year")
        }
        return value("year")
    }

    // Move within this row's trailing actions. Returns false when the move
    // would leave the row, so the list can take the key instead.
    function moveAction(delta) {
        const next = actionIndex + delta
        if (next < -1 || next >= actionCount)
            return false
        actionIndex = next
        return true
    }

    function activate() {
        if (actionIndex >= 0 && actionIndex < actionCount) {
            row.actionTriggered(String(actions[actionIndex].action))
            return
        }
        row.activated()
    }

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: -Metrics.scaled(8)
        anchors.rightMargin: -Metrics.scaled(8)
        radius: Theme.radiusSmall
        color: row.highlighted ? Theme.bgHover : hover.hovered ? Qt.alpha(Theme.bgHover, 0.5) : "transparent"
        border.width: row.highlighted && row.actionIndex < 0 ? Theme.focusBorderWidth : 0
        border.color: Qt.alpha(Theme.accent, 0.55)
    }

    HoverHandler {
        id: hover
    }

    RowLayout {
        anchors.fill: parent
        spacing: Metrics.scaled(10)

        ImageCard {
            Layout.preferredWidth: row.artWidth
            Layout.preferredHeight: row.artHeight
            Layout.alignment: Qt.AlignVCenter
            imageUrl: Art.url(row.item, row.kind, Math.round(row.artWidth * 2))
            fallbackIcon: row.fallbackIcon.length > 0 ? row.fallbackIcon : row.value("itemType") === "Episode" ? "tv" :
                                                                                                                 "movie"

            fallbackTint: Theme.bgHover

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: Metrics.scaled(3)
                visible: row.progress > 0
                color: Qt.alpha("#000000", 0.55)

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: Math.round(parent.width * Math.min(1, Math.max(0, row.progress)))
                    color: Theme.accent
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            AppText {
                Layout.fillWidth: true
                text: row.titleText()
                color: row.current ? Theme.accent : Theme.textPrimary
                font.pixelSize: Metrics.bodySizePx
                font.weight: row.current ? Font.DemiBold : Font.Normal
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            SecondaryText {
                Layout.fillWidth: true
                visible: text.length > 0
                text: row.subtitleText()
                color: Theme.textSecondary
                font.pixelSize: Metrics.metaSizePx
                maximumLineCount: 1
                elide: Text.ElideRight
            }
        }

        // Trailing actions. They only appear on the row being looked at, so a
        // resting list stays a list rather than a wall of buttons.
        Repeater {
            model: row.actionsVisible ? row.actions : []

            delegate: IconButton {
                required property int index
                required property var modelData

                Layout.alignment: Qt.AlignVCenter
                focusPolicy: Qt.NoFocus
                chromeless: row.actionIndex !== index
                iconName: String(modelData.icon || "add")
                accessibleName: String(modelData.label || "")
                selected: row.actionIndex === index
                onClicked: {
                    row.actionIndex = index
                    row.actionTriggered(String(modelData.action))
                }
            }
        }

        MaterialIcon {
            Layout.alignment: Qt.AlignVCenter
            visible: row.showChevron && row.actionCount === 0
            name: "chevron_right"
            iconSize: Metrics.iconSizePx
            iconColor: Theme.textMuted
        }
    }

    TapHandler {
        onTapped: {
            row.actionIndex = -1
            row.activated()
        }
    }
}
