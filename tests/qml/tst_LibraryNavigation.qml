import QtQuick
import QtTest
import "../../qml/pages/LibraryNavigation.js" as LibraryNavigation

TestCase {
    name: "LibraryNavigation"

    function test_nameSectionsUseLocaleCasingWithoutArticleStripping() {
        compare(LibraryNavigation.sectionLabel("SortName", {
                                                   "sortName": "éclair"
                                               }), "É")
        compare(LibraryNavigation.sectionLabel("SortName", {
                                                   "sortName": "The Abyss"
                                               }), "T")
        compare(LibraryNavigation.sectionLabel("SortName", {
                                                   "sortName": "  2001: A Space Odyssey"
                                               }), "#")
        compare(LibraryNavigation.sectionLabel("SortName", {
                                                   "sortName": ""
                                               }), "#")
    }

    function test_dateSortsShowRelevantYear() {
        compare(LibraryNavigation.sectionLabel("PremiereDate", {
                                                   "premiereDate": "1982-06-25T00:00:00Z",
                                                   "year": 1981
                                               }), "1982")
        compare(LibraryNavigation.sectionLabel("DateCreated", {
                                                   "dateCreated": "2026-07-18T10:30:00Z"
                                               }), "2026")
        compare(LibraryNavigation.sectionLabel("SeriesDatePlayed", {
                                                   "datePlayed": "2025-11-01T00:00:00Z"
                                               }), "2025")
        compare(LibraryNavigation.sectionLabel("DateLastContentAdded", {
                                                   "dateLastContentAdded": "2024-02-01T00:00:00Z"
                                               }), "2024")
    }

    function test_numericSortsShowCompactValues() {
        compare(LibraryNavigation.sectionLabel("CommunityRating", {
                                                   "communityRating": 8.25
                                               }), "8.3")
        compare(LibraryNavigation.sectionLabel("CriticRating", {
                                                   "criticRating": 7
                                               }), "7.0")
        compare(LibraryNavigation.sectionLabel("OfficialRating", {
                                                   "officialRating": "pg-13"
                                               }), "PG-13")
        compare(LibraryNavigation.sectionLabel("PlayCount", {
                                                   "playCount": 42
                                               }), "42")
        compare(LibraryNavigation.sectionLabel("Runtime", {
                                                   "runtimeTicks": 7500000000
                                               }), "13 min")
        compare(LibraryNavigation.sectionLabel("Random", {}), "#")
    }
}
