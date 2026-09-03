import QtQuick
import QtQuick.Window

// Every text rendering, hinting and antialiasing mode Qt gives us, at the
// exact size, weight and colour of the year/director line under an item name
// on webOS: 22px PT Root UI VF DemiBold in #C6C6C6, which is what dp(22) comes
// to in the 1080p scene at the television's default 130% interface scale.
Window {
    id: win
    visible: true
    width: grid.width
    height: grid.height
    color: "#0B0B0B"
    flags: Qt.FramelessWindowHint

    // render.sh passes these after a bare --, so the sheet can be pointed at
    // any frame without editing the scene.
    function argument(name, fallback) {
        const prefix = "--" + name + "="
        for (const value of Qt.application.arguments)
            if (value.startsWith(prefix))
                return value.substring(prefix.length)
        return fallback
    }

    readonly property string fontPath: argument("font", "")
    readonly property string framePath: argument("frame", "")
    readonly property string outPath: argument("out", "modes.png")
    readonly property string sample: argument("sample", "1999 · Directed by David Fincher")

    FontLoader {
        id: sansFont
        source: fontPath
    }

    readonly property var hintings: [
        {
            "name": "no hint",
            "value": Font.PreferNoHinting
        },
        {
            "name": "vertical",
            "value": Font.PreferVerticalHinting
        },
        {
            "name": "full",
            "value": Font.PreferFullHinting
        },
        {
            "name": "default",
            "value": Font.PreferDefaultHinting
        }
    ]
    readonly property var aaModes: [
        {
            "name": "AA",
            "aa": true
        },
        {
            "name": "no AA",
            "aa": false
        }
    ]
    readonly property var qualities: [
        {
            "name": "q:Default",
            "value": Text.DefaultRenderTypeQuality
        },
        {
            "name": "q:Low",
            "value": Text.LowRenderTypeQuality
        },
        {
            "name": "q:Normal",
            "value": Text.NormalRenderTypeQuality
        },
        {
            "name": "q:High",
            "value": Text.HighRenderTypeQuality
        },
        {
            "name": "q:VeryHigh",
            "value": Text.VeryHighRenderTypeQuality
        }
    ]

    readonly property int columnWidth: 356
    readonly property int rowHeight: 32
    // One patch of picture behind every cell, so the only thing that differs
    // between two columns is how the glyphs were drawn.
    readonly property int frameX: -700
    readonly property int frameY: -560

    component Label: Text {
        color: "#7F8C93"
        font.family: "sans-serif"
        font.pixelSize: 11
        renderType: Text.NativeRendering
    }

    // The line exactly as the app draws it; only the axes under test vary.
    component Sample: Text {
        text: win.sample
        color: "#C6C6C6"
        font.family: sansFont.name
        font.pixelSize: 22
        font.weight: Font.DemiBold
        font.preferTypoLineMetrics: true
        textFormat: Text.PlainText
        maximumLineCount: 1
    }

    component Cell: Column {
        id: cellRoot
        required property string label
        required property int render
        required property int hinting
        required property bool aa
        property int quality: Text.NormalRenderTypeQuality
        spacing: 2

        Label {
            text: cellRoot.label
            height: 16
        }

        Rectangle {
            width: win.columnWidth
            height: win.rowHeight
            color: "#000000"

            Sample {
                x: 8
                anchors.verticalCenter: parent.verticalCenter
                renderType: cellRoot.render
                font.hintingPreference: cellRoot.hinting
                antialiasing: cellRoot.aa
                renderTypeQuality: cellRoot.quality
            }
        }

        Item {
            width: win.columnWidth
            height: win.rowHeight
            clip: true

            Image {
                source: framePath
                x: win.frameX
                y: win.frameY
            }

            Sample {
                x: 8
                anchors.verticalCenter: parent.verticalCenter
                renderType: cellRoot.render
                font.hintingPreference: cellRoot.hinting
                antialiasing: cellRoot.aa
                renderTypeQuality: cellRoot.quality
            }
        }
    }

    Column {
        id: grid
        spacing: 12
        padding: 12

        Label {
            text: "PT Root UI VF · 22px · DemiBold · #C6C6C6 — the year/director line at the webOS 1080p scene scale. "
                  + "Top row of every pair on black, bottom row on a frame."
            font.pixelSize: 12
            color: "#B0BCC4"
        }

        // Hinting only reaches the native rasteriser, and antialiasing only
        // reaches the two shader paths, so every column carries both anyway:
        // the pairs that come out identical are part of the answer.
        Repeater {
            model: [
                {
                    "name": "Native (platform rasteriser, honours hinting)",
                    "value": Text.NativeRendering
                },
                {
                    "name": "Qt / distance field",
                    "value": Text.QtRendering
                },
                {
                    "name": "Curve",
                    "value": Text.CurveRendering
                }
            ]

            Column {
                id: section
                required property var modelData
                spacing: 4

                Label {
                    text: "renderType: " + section.modelData.name
                    color: "#9FB4C0"
                    font.pixelSize: 12
                }

                Row {
                    spacing: 10

                    Repeater {
                        model: win.aaModes

                        Row {
                            id: aaGroup
                            required property var modelData
                            spacing: 0

                            Repeater {
                                model: win.hintings

                                Cell {
                                    required property var modelData
                                    label: aaGroup.modelData.name + " · " + modelData.name
                                    render: section.modelData.value
                                    hinting: modelData.value
                                    aa: aaGroup.modelData.aa
                                }
                            }
                        }
                    }
                }
            }
        }

        // The one knob that is only distance field's: how finely the glyph
        // atlas is sampled. It is the lever for the softness that made native
        // rendering the default here in the first place.
        Column {
            spacing: 4

            Label {
                text: "renderType: Qt / distance field · renderTypeQuality sweep (hinting: default)"
                color: "#9FB4C0"
                font.pixelSize: 12
            }

            Row {
                spacing: 10

                Repeater {
                    model: win.aaModes

                    Row {
                        id: qualityGroup
                        required property var modelData
                        spacing: 0

                        Repeater {
                            model: win.qualities

                            Cell {
                                required property var modelData
                                label: qualityGroup.modelData.name + " · " + modelData.name
                                render: Text.QtRendering
                                hinting: Font.PreferDefaultHinting
                                aa: qualityGroup.modelData.aa
                                quality: modelData.value
                            }
                        }
                    }
                }
            }
        }
    }

    Timer {
        interval: 1200
        running: true
        onTriggered: grid.grabToImage(function (result) {
            result.saveToFile(outPath)
            Qt.exit(0)
        }, Qt.size(grid.width, grid.height))
    }
}
