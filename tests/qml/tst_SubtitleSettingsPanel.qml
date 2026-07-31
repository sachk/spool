import QtQuick
import QtTest
import "../../qml/pages/SettingsNavigation.js" as SettingsNavigation
import "../../qml/primitives" as Primitives

TestCase {
    id: testCase
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

    Component {
        id: menuListComponent

        Primitives.MenuListView {
            width: 320
            height: 240
            delegate: Item {
                required property int index
                width: 320
                height: 20
            }
        }
    }

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

    function test_menuListReadsSectionedArrayEntries() {
        const rows = SettingsNavigation.sectionedRows(sections, resolveAll)
        const list = createTemporaryObject(menuListComponent, testCase)
        verify(list)
        list.model = rows.length
        list.entryProvider = function (index) {
            return rows[index]
        }
        tryCompare(list, "count", rows.length)
        compare(list.entryAt(0).spec.title, "Track")
        verify(!list.isRowEnabled(0))
        verify(list.isRowEnabled(1))
        list.currentIndex = 1
        list.moveSelection(1)
        compare(list.currentIndex, 2)
        list.moveSelection(1)
        compare(list.currentIndex, 4)
    }
    function test_noSectionsYieldsNoRows() {
        compare(SettingsNavigation.sectionedRows([], resolveAll).length, 0)
    }
}
