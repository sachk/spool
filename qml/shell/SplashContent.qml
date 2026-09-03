import QtQuick

// The launch screen, drawn the same way everywhere it appears: the frame Qt
// puts up before the shell exists, the overlay the shell holds until the first
// page has painted, and the slow-start page that grows out of it.
//
// Everything outside the mark is pure black, and the mark is the same picture
// the system already has on screen -- rendered from the same PNG, at the same
// size -- so the handover from the system's launch frame to ours shows
// nothing at all.
//
// This is loaded from a plain resource rather than the QML module because the
// first of those three uses happens before the module is loaded.
Rectangle {
    id: root

    // Every default here is a literal, and the two callers hand in real values
    // by whichever route works where they run. An earlier version read the
    // context directly and one of the two callers silently got undefined,
    // which made the whole expression below NaN -- and a NaN width does not
    // fail, it quietly leaves the image at its natural size, so the mark came
    // out at the size of the source PNG and nothing said a word.
    objectName: "startupSplashCore"

    // Pixels per Android dp. Zero on platforms that size the mark against the
    // viewport instead, which is every platform without a system launch frame
    // to match.
    property real pixelsPerDp: 0
    // The size the system's launch frame drew the mark at, in dp.
    property real coreWidthDp: 0
    // Fraction of the shorter edge the mark takes when nothing else dictates.
    property real coreWidthFraction: 0.5926
    property real coreAspect: 1
    property url coreSource: ""

    // Matching the frame the system already has on screen beats sizing against
    // the viewport, so the dp path wins wherever there is one to match.
    readonly property bool matchesSystemFrame: pixelsPerDp > 0 && coreWidthDp > 0
    readonly property real coreWidth: matchesSystemFrame ? Math.round(coreWidthDp * pixelsPerDp) : Math.round(Math.min(
                                                                                                                  width, height)
                                                                                                              * coreWidthFraction)

    color: "black"

    // A launch screen that quietly picked the wrong rule drew the mark at
    // twice the size the system had just drawn it, and nothing said so.
    Component.onCompleted: console.info("splash:", matchesSystemFrame ? "system-matched" : "viewport-sized",
                                        "pixelsPerDp=" + pixelsPerDp, "coreWidthDp=" + coreWidthDp, "coreWidth="
                                        + coreWidth, "viewport=" + width + "x" + height)

    Image {
        id: core
        width: root.coreWidth
        height: Math.round(root.coreWidth / root.coreAspect)
        anchors.centerIn: parent
        source: root.coreSource
        sourceSize.width: width
        sourceSize.height: height
        fillMode: Image.PreserveAspectFit
        // The whole point of this frame is to be the first one, so it is not
        // allowed to arrive a frame late.
        asynchronous: false
        cache: true
        smooth: true
    }

    // Where a slow start hangs its spinner and its way out, so the two agree
    // on the gap below the mark without either of them measuring the mark.
    readonly property real markBottomY: core.y + core.height
}
