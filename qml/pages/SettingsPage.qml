import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root
    property var shell
    focus: true
    RowLayout { anchors.fill: parent; anchors.margins: Metrics.pageMargin(width); spacing: 18
        ListView { Layout.preferredWidth: 230; Layout.fillHeight: true; model: ["General", "Playback", "Appearance", "Libraries", "Network", "Input", "Diagnostics", "Subtitles", "About"]; spacing: 8; delegate: MetadataChip { required property string modelData; text: modelData; width: 220; height: 38 } }
        Flickable { Layout.fillWidth: true; Layout.fillHeight: true; contentHeight: settings.implicitHeight; clip: true
            ColumnLayout { id: settings; width: parent.width; spacing: 10
                SectionHeader { Layout.fillWidth: true; title: "Settings" }
                SelectRow { Layout.fillWidth: true; title: "Theme"; description: "Neutral Jellyfin dark appliance UI"; options: ["Jellyfin Dark"]; currentIndex: 0 }
                SelectRow { Layout.fillWidth: true; title: "Accent"; options: ["Jellyfin Blue", "Jellyfin Purple", "Blue-Purple"] }
                SliderRow { Layout.fillWidth: true; title: "UI Scale"; description: "Runtime type and spacing scale"; value: Metrics.userUiScale; onValueEdited: Metrics.userUiScale = value }
                SelectRow { Layout.fillWidth: true; title: "Poster Size"; options: ["Compact", "Normal", "Large"]; currentIndex: Metrics.userPosterSizeBias + 1; onSelected: (i, v) => Metrics.userPosterSizeBias = i - 1 }
                SelectRow { Layout.fillWidth: true; title: "Grid Columns"; options: ["Auto", "4", "5", "6", "7", "8", "9"]; onSelected: (i, v) => Metrics.userColumnOverride = i === 0 ? 0 : Number(v) }
                SelectRow { Layout.fillWidth: true; title: "Side Rail Labels"; options: ["Never", "On focus", "Always"]; currentIndex: 1; onSelected: (i, v) => Theme.sideRailLabels = v }
                ToggleRow { Layout.fillWidth: true; title: "Reduced Motion"; checked: Theme.reducedMotion; onToggled: Theme.reducedMotion = checked }
                SelectRow { Layout.fillWidth: true; title: "Text Render Mode"; options: ["Auto", "QtRendering", "CurveRendering"]; currentIndex: 1; onSelected: (i, v) => Theme.normalTextRenderType = v === "CurveRendering" ? Text.CurveRendering : Text.QtRendering }
                ToggleRow { Layout.fillWidth: true; title: "Antialiased Text"; checked: Theme.antialiasedText; onToggled: Theme.antialiasedText = checked }
                SelectRow { Layout.fillWidth: true; title: "Show Technical Metadata"; options: ["Always", "On details only", "Hidden"]; onSelected: (i, v) => Theme.technicalMetadataMode = v }
                ToggleRow { Layout.fillWidth: true; title: "Night mode"; description: "Dialogue lift and late-night dynamic range"; checked: appController.nightModeEnabled; onToggled: appController.setNightModeEnabled(checked) }
                ToggleRow { Layout.fillWidth: true; title: "Diagnostics overlay"; checked: shell.diagnosticsVisible; onToggled: shell.diagnosticsVisible = checked }
                SelectRow { Layout.fillWidth: true; title: "Maximum remote bitrate"; options: ["Auto", "20 Mbps", "40 Mbps", "80 Mbps", "Unlimited"] }
                ToggleRow { Layout.fillWidth: true; title: "Prefer remux over transcode"; checked: true }
                ToggleRow { Layout.fillWidth: true; title: "Keyboard shortcuts enabled"; checked: true }
            }
        }
    }
}
