import QtQuick
import "../primitives"

FocusScope {
    id: root

    property bool active: false
    property bool mediaInfoVisible: false
    property bool diagnosticsVisible: false
    signal pressed(var event)
    signal released(var event)

    visible: active
    enabled: active
    focus: active

    function focusInput() {
        InputKeys.focus(inputShield)
    }

    function handleBack() {
        return playerOverlay.handleBack()
    }

    function handlePressed(event) {
        return playerOverlay.handlePressed(event)
    }

    function handleReleased(event) {
        return playerOverlay.handleReleased(event)
    }

    Image {
        anchors.fill: parent
        visible: root.active
        source: visible ? "image://mpv-overlay/live?rev=" + nativeWindow.overlayRevision : ""
        cache: false
        fillMode: Image.Stretch
        z: 0
    }

    PlayerOverlayPage {
        id: playerOverlay
        anchors.fill: parent
        visible: root.active
        mediaInfoVisible: root.mediaInfoVisible
        diagnosticsVisible: root.diagnosticsVisible
        z: 1
    }

    FocusScope {
        id: inputShield
        anchors.fill: parent
        visible: root.active
        enabled: visible
        focus: visible
        z: 2

        onVisibleChanged: if (visible)
            InputKeys.focus(inputShield)
        onActiveFocusChanged: if (visible && !activeFocus)
            Qt.callLater(() => InputKeys.focus(inputShield))

        Keys.onPressed: event => root.pressed(event)
        Keys.onReleased: event => root.released(event)
    }
}
