import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

FocusScope {
    id: root
    width: 1920
    height: 1080
    focus: true

    function isBackEvent(event) {
        const scanCode = Number(event.nativeScanCode || 0)
        return event.key === Qt.Key_Back
                || event.key === Qt.Key_Escape
                || event.key === Qt.Key_BrowserBack
                || event.key === 0x01200003
                || (event.key === 0 && scanCode === 420)
    }

    function isIgnoredPlayerNoise(event) {
        const scanCode = Number(event.nativeScanCode || 0)
        return event.key === 0 && (scanCode === 1206 || scanCode === 1207)
    }
    
    Keys.onPressed: (event) => {
        if (root.isIgnoredPlayerNoise(event) && appController.player.visible) {
            event.accepted = true
        }
    }
    
    Keys.onReleased: (event) => {
        if (root.isIgnoredPlayerNoise(event) && appController.player.visible) {
            event.accepted = true
            return
        }
        if (root.isBackEvent(event)) {
            if (appController.player.visible) {
                if (appController.player.backAllowed)
                    appController.player.stopWithReason("root-back-key")
            } else {
                appController.back()
            }
            event.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: !appController.player.visible
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#07111a" }
            GradientStop { position: 0.5; color: "#0c1c28" }
            GradientStop { position: 1.0; color: "#02070d" }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: !appController.player.visible
        color: "transparent"
        border.width: 1
        border.color: "#254c65"
        opacity: 0.25
    }

    StackLayout {
        id: pageStack
        anchors.fill: parent
        currentIndex: appController.page === "login" ? 0 : appController.page === "libraries" ? 1 : 2
        visible: !appController.player.visible

        LoginPage {
            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: pageStack.currentIndex === 0 && !appController.player.visible
        }

        LibrariesPage {
            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: pageStack.currentIndex === 1 && !appController.player.visible
        }

        LibraryPage {
            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: pageStack.currentIndex === 2 && !appController.player.visible
        }
    }

    PlayerOverlay {
        anchors.fill: parent
        visible: appController.player.visible
    }

    Image {
        anchors.fill: parent
        visible: appController.player.visible && appController.player.debugOsdVisible
        source: visible ? "image://mpv-overlay/live?" + nativeWindow.overlayRevision : ""
        fillMode: Image.Stretch
        smooth: false
        cache: false
        z: 20
    }

    SettingsPanel {
        anchors.fill: parent
        visible: appController.settingsVisible && !appController.player.visible
        z: 30
        focus: visible
    }

    Rectangle {
        anchors.fill: parent
        visible: appController.busy
        color: "#88030a10"

        Column {
            anchors.centerIn: parent
            spacing: 20

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: true
                width: 80
                height: 80
            }

            Label {
                text: appController.busyText
                font.pixelSize: 30
                color: "#e8f3ff"
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 36
        width: Math.min(parent.width * 0.72, 1100)
        height: appController.errorText.length > 0 ? errorLabel.implicitHeight + 36 : 0
        visible: appController.errorText.length > 0
        radius: 22
        color: "#d0223040"
        border.color: "#f58f93"
        border.width: 1

        Label {
            id: errorLabel
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: 18
            text: appController.errorText
            wrapMode: Text.Wrap
            color: "#fff4f5"
            font.pixelSize: 24
            horizontalAlignment: Text.AlignHCenter
        }

        MouseArea {
            anchors.fill: parent
            onClicked: appController.clearError()
        }
    }
}
