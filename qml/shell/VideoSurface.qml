import QtQuick
import JellyfinWebOS
import "../primitives"

FocusScope {
    id: root

    property bool active: false
    property bool mediaInfoVisible: false
    property bool diagnosticsVisible: false
    readonly property bool directionRelease: true

    // KeyRouter defers activation to key release for any target that merely
    // owns a longPress member, so this is advertised only while the queue panel
    // wants it. Everywhere else OK-to-pause stays a press-time action.
    property var longPress: playerOverlay.queuePanelVisible ? root.queuePanelLongPress : undefined

    signal playbackBackRequested(var item)

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

    function queuePanelLongPress() {
        return playerOverlay.queuePanelLongPress()
    }

    function finishOpeningGesture() {
        playerOverlay.queuePanelFinishGesture()
    }

    function openPlaybackSettings() {
        playerOverlay.openMenu("debug")
        focusInput()
    }

    function toggleOsd() {
        if (playerOverlay.controlsVisible)
            playerOverlay.hideControls()
        else
            playerOverlay.showControls("timeline")
        focusInput()
    }

    // Registered dynamically by main.cpp, so no static .qmltypes entry exists.
    // qmllint disable import unresolved-type
    MpvVideoItem {
        anchors.fill: parent
        visible: root.active && Player.embeddedVideoOutput
        z: 0
    }
    // qmllint enable import unresolved-type

    Image {
        x: NativeWindow.overlayX
        y: NativeWindow.overlayY
        width: NativeWindow.overlayWidth
        height: NativeWindow.overlayHeight
        visible: root.active && width > 0 && height > 0
        source: visible ? "image://mpv-overlay/live?rev=" + NativeWindow.overlayRevision : ""
        cache: false
        fillMode: Image.Stretch
        z: 1
    }

    Loader {
        anchors.fill: parent
        active: root.active && Player.mediaKind === "audio"
        z: 0
        sourceComponent: PlayerMusicStage {
            overlay: playerOverlay
        }
    }

    PlayerOverlayPage {
        id: playerOverlay
        anchors.fill: parent
        visible: root.active
        onPlaybackBackRequested: item => root.playbackBackRequested(item)
        z: 2
    }

    FocusScope {
        id: inputShield
        anchors.fill: parent
        visible: root.active
        enabled: visible && !playerOverlay.subtitleSettingsVisible && !playerOverlay.queuePanelVisible
        focus: enabled
        z: 3

        onVisibleChanged: if (enabled)
                              InputKeys.focus(inputShield)
        onEnabledChanged: if (enabled)
                              InputKeys.focus(inputShield)
        onActiveFocusChanged: if (enabled && !activeFocus)
                                  Qt.callLater(() => InputKeys.focus(inputShield))
    }

    Loader {
        anchors.fill: parent
        active: root.active && (root.diagnosticsVisible || Player.debugOsdVisible)
        z: 4
        sourceComponent: DiagnosticsOverlay {
            route: "player"
        }
    }
}
