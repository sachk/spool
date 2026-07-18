import QtQuick
import QtTest
import "../../qml/pages/SettingsNavigation.js" as SettingsNavigation

TestCase {
    name: "SettingsNavigation"

    function valueLookup(values) {
        return function (key) {
            return values[key]
        }
    }

    function test_progressiveDisclosureLevels() {
        compare(SettingsNavigation.detailLevel({}), 0)
        compare(SettingsNavigation.detailLevel({
                                                   "level": 1
                                               }), 1)
        compare(SettingsNavigation.detailLevel({
                                                   "level": 2
                                               }), 2)
    }

    function test_platformAndDependencyFiltering() {
        const values = {
            "playback/mpvConfigMode": "custom"
        }
        const lookup = valueLookup(values)
        verify(SettingsNavigation.rowAvailable({
                                                   "platform": "desktop"
                                               }, false, lookup))
        verify(!SettingsNavigation.rowAvailable({
                                                    "platform": "desktop"
                                                }, true, lookup))
        verify(SettingsNavigation.rowAvailable({
                                                   "platform": "webos"
                                               }, true, lookup))
        verify(!SettingsNavigation.rowAvailable({
                                                    "platform": "webos"
                                                }, false, lookup))
        verify(SettingsNavigation.rowAvailable({
                                                   "dependsOnKey": "playback/mpvConfigMode",
                                                   "dependsOnValue": "custom"
                                               }, false, lookup))
        verify(!SettingsNavigation.rowAvailable({
                                                    "dependsOnKey": "playback/mpvConfigMode",
                                                    "dependsOnValue": "standard"
                                                }, false, lookup))
        verify(!SettingsNavigation.rowAvailable({
                                                    "visible": false
                                                }, false, lookup))
    }

    function test_focusClampsAfterFiltering() {
        compare(SettingsNavigation.clampIndex(-1, 5), 0)
        compare(SettingsNavigation.clampIndex(3, 5), 3)
        compare(SettingsNavigation.clampIndex(9, 5), 4)
        compare(SettingsNavigation.clampIndex(4, 2), 1)
        compare(SettingsNavigation.clampIndex(0, 0), -1)
    }
}
