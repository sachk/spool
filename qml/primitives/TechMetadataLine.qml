import QtQuick
import "../theme"

MonoText {
    id: root
    property string metadata: ""
    text: metadata.length > 0 ? metadata : "Technical metadata unavailable"
    color: metadata.length > 0 ? Theme.textSecondary : Theme.textMuted
    wrapMode: Text.Wrap
    maximumLineCount: 2
    elide: Text.ElideRight
    font.pixelSize: Metrics.metaSizePx
    lineHeight: 1.12
}
