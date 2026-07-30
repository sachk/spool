import QtQuick
import QtTest
import "../../qml/pages/SettingsNavigation.js" as SettingsNavigation

TestCase {
    name: "SubtitleSettingsPanel"

    readonly property var sections: [
        {
            "title": "Track",
            "keys": ["subtitles/language", "subtitles/mode"]
        },
        {
            "title": "Image subtitles",
            "keys": ["subtitles/imageColorMode", "subtitles/bitmapSmoothing"]
        },
        {
            "title": "HDR",
            "keys": ["subtitles/dimInHdr"]
        }
    ]

    function resolveAll(key) {
        return {
            "key": key
        }
    }

    function test_sectionsFlattenInDeclaredOrder() {
        const rows = SettingsNavigation.sectionedRows(sections, resolveAll)
        compare(rows.length, 8)
        verify(rows[0].section)
        compare(rows[0].spec.title, "Track")
        compare(rows[1].spec.key, "subtitles/language")
        compare(rows[2].spec.key, "subtitles/mode")
        verify(rows[3].section)
        compare(rows[3].spec.title, "Image subtitles")
        compare(rows[4].spec.key, "subtitles/imageColorMode")
    }

    // A header with nothing under it is worse than no header, so an empty
    // section has to disappear entirely.
    function test_emptySectionDropsItsHeader() {
        const rows = SettingsNavigation.sectionedRows(sections, function (key) {
            return key === "subtitles/dimInHdr" ? null : resolveAll(key)
        })
        for (let index = 0; index < rows.length; ++index)
            verify(!rows[index].section || rows[index].spec.title !== "HDR")
        compare(rows.length, 6)
    }

    function test_missingRowsShrinkTheirSection() {
        const rows = SettingsNavigation.sectionedRows(sections, function (key) {
            return key === "subtitles/mode" ? null : resolveAll(key)
        })
        compare(rows[0].spec.title, "Track")
        compare(rows[1].spec.key, "subtitles/language")
        verify(rows[2].section)
    }

    // Section entries must stay unselectable, which MenuListView decides from
    // exactly this flag.
    function test_headersAreNotSelectable() {
        const rows = SettingsNavigation.sectionedRows(sections, resolveAll)
        const rowEnabled = function (entry) {
            return !(entry && entry.section === true)
        }
        verify(!rowEnabled(rows[0]))
        verify(rowEnabled(rows[1]))
    }

    function test_noSectionsYieldsNoRows() {
        compare(SettingsNavigation.sectionedRows([], resolveAll).length, 0)
    }
}
