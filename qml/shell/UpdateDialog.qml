import QtQuick
import QtQuick.Layouts
import "../theme"
import "../primitives"

FocusScope {
    id: root

    required property var updater
    readonly property string stage: updater ? updater.stage : "idle"
    readonly property string updateVersion: updater ? updater.version : ""
    readonly property string updateErrorText: updater ? updater.errorText : ""
    readonly property real updateProgress: updater ? updater.progress : 0
    readonly property double updateReceivedBytes: updater ? updater.receivedBytes : 0
    readonly property double updateTotalBytes: updater ? updater.totalBytes : 0
    readonly property double updateBytesPerSecond: updater ? updater.bytesPerSecond : 0
    readonly property bool open: stage === "available" || stage === "downloading" || stage === "ready" || stage === "permission"
                                 || stage === "error"
    property int actionIndex: 0

    anchors.fill: parent
    visible: open
    focus: visible

    function formatMegabytes(bytes) {
        return (Math.max(0, Number(bytes)) / 1000000).toFixed(1) + " MB"
    }

    function visibleActions() {
        if (stage === "available")
            return [changelogButton, secondaryButton, primaryButton]
        return [secondaryButton, primaryButton].filter(function (button) {
            return button.visible
        })
    }

    function focusSafeAction() {
        const actions = visibleActions()
        if (actions.length === 0)
            return
        actionIndex = stage === "available" ? 1 : 0
        InputKeys.focus(actions[Math.min(actionIndex, actions.length - 1)])
    }

    function routeKey(key, phase, repeat) {
        if (InputKeys.isBack(key, false, false)) {
            if (phase === "release")
                back()
            return true
        }
        if (!InputKeys.isDirection(key))
            return InputKeys.isAccept(key)
        if (phase !== "press")
            return true
        const actions = visibleActions()
        if (actions.length === 0)
            return true
        if (stage === "available" && actionIndex === 0 && (key === Qt.Key_Up || key === Qt.Key_Down)) {
            const maximumY = Math.max(0, notesFlickable.contentHeight - notesFlickable.height)
            notesFlickable.contentY = Math.max(0, Math.min(maximumY, notesFlickable.contentY + (key === Qt.Key_Up ?
                                                                                                    -Metrics.scaled(72) :
                                                                                                    Metrics.scaled(
                                                                                                        72))))
            return true
        }
        if (InputKeys.isHorizontal(key))
            actionIndex = Math.max(0, Math.min(actions.length - 1, actionIndex + (key === Qt.Key_Left ? -1 : 1)))
        else if (key === Qt.Key_Up)
            actionIndex = 0
        else if (key === Qt.Key_Down)
            actionIndex = actions.length - 1
        InputKeys.focus(actions[actionIndex])
        return true
    }

    function activate() {
        const actions = visibleActions()
        if (actions.length > 0)
            actions[Math.min(actionIndex, actions.length - 1)].clicked()
    }

    function back() {
        if (!updater)
            return true
        if (stage === "downloading")
            updater.cancelDownload()
        else
            updater.decline()
        return true
    }

    onStageChanged: if (open)
                        Qt.callLater(focusSafeAction)

    Rectangle {
        anchors.fill: parent
        color: Theme.overlayScrimStrong
    }

    Surface {
        anchors.centerIn: parent
        width: Math.min(parent.width - Metrics.scaled(40), Metrics.scaled(680))
        height: Math.min(parent.height - Metrics.scaled(40), content.implicitHeight + Metrics.scaled(48))
        elevated: true
        baseColor: Theme.floatingPanel

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: Metrics.scaled(24)
            spacing: Metrics.scaled(14)

            AppText {
                Layout.fillWidth: true
                text: root.stage === "available" ? "Update available" : root.stage === "downloading"
                                                   ? "Downloading update" : root.stage === "ready" ? "Done" :
                                                                                                     root.stage
                                                                                                     === "permission"
                                                                                                     ? "Allow update installation" :
                                                                                                       "Update failed"
                color: Theme.textPrimary
                font.pixelSize: Metrics.titleSizePx
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }

            AppText {
                Layout.fillWidth: true
                visible: root.stage !== "available"
                text: root.stage === "downloading" ? "Spool " + root.updateVersion + " is downloading." : root.stage
                                                     === "ready" ? "Spool " + root.updateVersion
                                                                   + " was downloaded and its SHA-256 checksum verified. Install it when you are ready." :
                                                                   root.stage === "permission"
                                                                   ? "Android needs your permission before Spool can open the installer. On the next screen, enable “Allow from this source”, then return here. You will still choose Install before Android asks for final confirmation." :
                                                                     root.updateErrorText
                color: root.stage === "error" ? Theme.errorText : Theme.textSecondary
                font.pixelSize: Metrics.bodySizePx
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                visible: root.stage === "available"
                spacing: Metrics.scaled(12)

                AppText {
                    Layout.fillWidth: true
                    text: "Spool " + root.updateVersion + " is ready. Would you like to update now?"
                    color: Theme.textSecondary
                    font.pixelSize: Metrics.bodySizePx
                    wrapMode: Text.Wrap
                }

                ActionButton {
                    id: changelogButton
                    text: "Changelog"
                    kind: "flat"
                    iconName: "open_in_new"
                    onActiveFocusChanged: if (activeFocus)
                                              root.actionIndex = 0
                    onClicked: Qt.openUrlExternally(root.updater.releaseUrl)
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(Metrics.scaled(250), Math.max(Metrics.scaled(120), notesText.contentHeight
                                                                               + Metrics.scaled(24)))
                visible: root.stage === "available"
                radius: Theme.radiusMedium
                color: Theme.bgRaised
                border.width: 1
                border.color: Theme.border
                clip: true

                Flickable {
                    id: notesFlickable
                    anchors.fill: parent
                    anchors.margins: Metrics.scaled(12)
                    contentWidth: width
                    contentHeight: notesText.contentHeight
                    boundsBehavior: Flickable.StopAtBounds
                    flickableDirection: Flickable.VerticalFlick
                    clip: true

                    TextEdit {
                        id: notesText
                        width: notesFlickable.width
                        text: root.updater ? root.updater.notes : ""
                        textFormat: TextEdit.MarkdownText
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.Wrap
                        color: Theme.textSecondary
                        selectionColor: Theme.accentDim
                        font.pixelSize: Metrics.bodySizePx - Metrics.scaled(1)
                        onLinkActivated: link => Qt.openUrlExternally(link)
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: root.stage === "downloading"
                spacing: Metrics.scaled(8)

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Metrics.scaled(12)
                    radius: height / 2
                    color: Theme.bgRaised
                    clip: true

                    Rectangle {
                        width: parent.width * root.updateProgress
                        height: parent.height
                        radius: parent.radius
                        color: Theme.accent
                    }
                }

                AppText {
                    Layout.fillWidth: true
                    text: root.formatMegabytes(root.updateReceivedBytes) + " of " + root.formatMegabytes(
                              root.updateTotalBytes) + "  ·  " + (root.updateBytesPerSecond > 0 ? (
                                                                                                      root.updateBytesPerSecond
                                                                                                      / 1000000).toFixed(
                                                                                                      1) + " MB/s" :
                                                                                                  "Starting…")
                    color: Theme.textMuted
                    font.pixelSize: Metrics.metaSizePx
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Metrics.scaled(8)
                spacing: Metrics.scaled(12)

                Item {
                    Layout.fillWidth: true
                }

                ActionButton {
                    id: secondaryButton
                    visible: root.stage !== "downloading"
                    text: root.stage === "available" ? "Not now" : root.stage === "downloading" ? "Cancel" : root.stage
                                                                                                  === "ready" ? "Later" :
                                                                                                                root.stage
                                                                                                                === "permission"
                                                                                                                ? "Not now" :
                                                                                                                  "Close"
                    onActiveFocusChanged: if (activeFocus)
                                              root.actionIndex = root.stage === "available" ? 1 : 0
                    onClicked: {
                        if (root.stage === "downloading")
                            root.updater.cancelDownload()
                        else
                            root.updater.decline()
                    }
                }

                ActionButton {
                    id: primaryButton
                    visible: true
                    text: root.stage === "available" ? "Download" : root.stage === "downloading" ? "Cancel download" :
                                                                                                   root.stage
                                                                                                   === "ready"
                                                                                                   ? "Install" :
                                                                                                     root.stage
                                                                                                     === "permission"
                                                                                                     ? "Open settings" :
                                                                                                       "Try again"
                    kind: root.stage === "downloading" ? "secondary" : "primary"
                    onActiveFocusChanged: if (activeFocus)
                                              root.actionIndex = root.stage === "available" ? 2 : root.stage
                                                                                              === "downloading" ? 0 : 1
                    onClicked: {
                        if (root.stage === "available")
                            root.updater.download()
                        else if (root.stage === "downloading")
                            root.updater.cancelDownload()
                        else if (root.stage === "ready")
                            root.updater.install()
                        else if (root.stage === "permission")
                            root.updater.openInstallSettings()
                        else
                            root.updater.retry()
                    }
                }
            }
        }
    }
}
