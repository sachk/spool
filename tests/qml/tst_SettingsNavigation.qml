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
                                               }, false, false, lookup))
        verify(!SettingsNavigation.rowAvailable({
                                                    "platform": "desktop"
                                                }, true, false, lookup))
        verify(SettingsNavigation.rowAvailable({
                                                   "platform": "webos"
                                               }, true, false, lookup))
        verify(!SettingsNavigation.rowAvailable({
                                                    "platform": "webos"
                                                }, false, false, lookup))
        verify(SettingsNavigation.rowAvailable({
                                                   "dependsOnKey": "playback/mpvConfigMode",
                                                   "dependsOnValue": "custom"
                                               }, false, false, lookup))
        verify(!SettingsNavigation.rowAvailable({
                                                    "dependsOnKey": "playback/mpvConfigMode",
                                                    "dependsOnValue": "standard"
                                                }, false, false, lookup))
        verify(!SettingsNavigation.rowAvailable(null, false, false, lookup))
        verify(!SettingsNavigation.rowAvailable({
                                                    "requiresHdrPlayback": true
                                                }, false, false, lookup))
        verify(SettingsNavigation.rowAvailable({
                                                   "requiresHdrPlayback": true
                                               }, false, true, lookup))
    }

    function test_focusClampsAfterFiltering() {
        compare(SettingsNavigation.clampIndex(-1, 5), 0)
        compare(SettingsNavigation.clampIndex(3, 5), 3)
        compare(SettingsNavigation.clampIndex(9, 5), 4)
        compare(SettingsNavigation.clampIndex(4, 2), 1)
        compare(SettingsNavigation.clampIndex(0, 0), -1)
    }

    function test_stableKeyLookupSupportsArraysAndListModels() {
        const rows = [
                  {
                      "rowKey": "a"
                  },
                  {
                      "rowKey": "b"
                  }
              ]
        compare(SettingsNavigation.indexForRowKey(rows, "b"), 1)
        compare(SettingsNavigation.indexForRowKey(rows, "missing"), -1)

        const model = Qt.createQmlObject("import QtQuick; ListModel {}", this)
        model.append(rows[0])
        model.append(rows[1])
        compare(SettingsNavigation.indexForRowKey(model, "a"), 0)
        model.destroy()
    }

    function test_nearestSourceOrderPrefersPrecedingRowOnTie() {
        const rows = [
                  {
                      "rowKey": "before",
                      "sourceIndex": 8
                  },
                  {
                      "rowKey": "after",
                      "sourceIndex": 12
                  }
              ]
        compare(SettingsNavigation.nearestRowKey(rows, 10), "before")
        compare(SettingsNavigation.nearestRowKey(rows, 11), "after")
        compare(SettingsNavigation.nearestRowKey([], 10), "")
    }

    function test_reconcileRowsPreservesPrefixAndSuffix() {
        const model = Qt.createQmlObject("import QtQuick; ListModel {}", this)
        model.append({
                         "rowKey": "prefix",
                         "showHeader": true,
                         "sourceIndex": 0
                     })
        model.append({
                         "rowKey": "remove",
                         "showHeader": false,
                         "sourceIndex": 2
                     })
        model.append({
                         "rowKey": "suffix",
                         "showHeader": false,
                         "sourceIndex": 6
                     })
        SettingsNavigation.reconcileRows(model, [
                                             {
                                                 "rowKey": "prefix",
                                                 "showHeader": true,
                                                 "sourceIndex": 0
                                             },
                                             {
                                                 "rowKey": "insert-one",
                                                 "showHeader": false,
                                                 "sourceIndex": 2
                                             },
                                             {
                                                 "rowKey": "insert-two",
                                                 "showHeader": false,
                                                 "sourceIndex": 4
                                             },
                                             {
                                                 "rowKey": "suffix",
                                                 "showHeader": true,
                                                 "sourceIndex": 6
                                             }
                                         ])
        compare(model.count, 4)
        compare(model.get(0).rowKey, "prefix")
        compare(model.get(1).rowKey, "insert-one")
        compare(model.get(2).rowKey, "insert-two")
        compare(model.get(3).rowKey, "suffix")
        compare(model.get(3).showHeader, true)
        model.destroy()
    }
}
