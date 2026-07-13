import QtQuick
import "../primitives"

FocusScope {
    id: root

    property bool active: false
    property bool mediaInfoVisible: false
    property bool diagnosticsVisible: false
    readonly property bool directionRelease: true

    visible: active
    enabled: active
    focus: active

    function focusInput() {
        InputKeys.focus(inputShield)
    }

    function back() {
        return playerOverlay.back()
    }

    function routeKey(key, phase, repeat) {
        return playerOverlay.routeKey(key, phase, repeat)
    }

    function activate() {
        playerOverlay.activate()
    }

    Image {
        anchors.fill: parent
        visible: root.active
        source: visible ? "image://mpv-overlay/live?rev=" + NativeWindow.overlayRevision : ""
        cache: false
        fillMode: Image.Stretch
        z: 0
    }

    PlaybackDiagnosticsOverlay {
        anchors.fill: parent
        visible: root.active && Player.debugOsdVisible
        stats: Player.debugStats
        z: 1
    }

    PlayerOverlayPage {
        id: playerOverlay
        anchors.fill: parent
        visible: root.active
        z: 2
    }

    FocusScope {
        id: inputShield
        anchors.fill: parent
        visible: root.active
        enabled: visible
        focus: visible
        z: 3

        onVisibleChanged: if (visible)
                              InputKeys.focus(inputShield)
        onActiveFocusChanged: if (visible && !activeFocus)
                                  Qt.callLater(() => InputKeys.focus(inputShield))
    }
}
