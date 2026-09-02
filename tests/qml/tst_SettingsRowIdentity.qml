import QtQuick
import QtTest
import "../../qml/primitives" as Primitives
import "../../qml/pages/SettingsNavigation.js" as SettingsNavigation

// Settings rows are drawn by a Loader inside the delegate, so the row that
// paints the highlight is not the item the view marks as current. Copying the
// delegate's index into the loaded row froze it: the model inserts and removes
// rows whenever a disclosure opens or a dependent setting appears, every
// delegate below the change shifts, and a stale copy then matched a
// currentIndex belonging to a different row -- two highlights moving together,
// one of them a ghost. These tests pin the delegate to the view's own
// single-current-item guarantee instead.
TestCase {
    id: testCase
    name: "SettingsRowIdentity"
    width: 400
    height: 600
    when: windowShown

    ListModel {
        id: rowsModel
    }

    Component {
        id: rowComponent

        Rectangle {
            readonly property int rowIndex: parent ? parent.rowIndex : -1
            readonly property bool rowFocus: parent ? parent.rowCurrent : false
            readonly property string rowTitle: parent ? parent.row.title : ""
            width: 100
            height: 40
        }
    }

    Primitives.MenuListView {
        id: settingsList

        anchors.fill: parent
        model: rowsModel

        delegate: Column {
            id: settingsDelegate

            required property int index
            required property string rowKey
            readonly property var rowData: ({
                                                "title": settingsDelegate.rowKey.toUpperCase()
                                            })
            readonly property bool rowCurrent: ListView.isCurrentItem && settingsList.activeFocus
            readonly property var controlItem: rowLoader.item
            width: settingsList.width

            Loader {
                id: rowLoader

                readonly property var row: settingsDelegate.rowData
                readonly property int rowIndex: settingsDelegate.index
                readonly property bool rowCurrent: settingsDelegate.rowCurrent
                width: parent.width
                sourceComponent: rowComponent
            }
        }
    }

    function rowsFor(keys) {
        const rows = []
        for (let index = 0; index < keys.length; ++index)
            rows.push({
                          "rowKey": keys[index]
                      })
        return rows
    }

    function seed(keys) {
        rowsModel.clear()
        const rows = rowsFor(keys)
        for (let index = 0; index < rows.length; ++index)
            rowsModel.append(rows[index])
        settingsList.forceLayout()
    }

    function reconcile(keys) {
        SettingsNavigation.reconcileRows(rowsModel, rowsFor(keys))
        settingsList.forceLayout()
    }

    function focusedRowKeys() {
        const focused = []
        for (let index = 0; index < rowsModel.count; ++index) {
            const delegate = settingsList.itemAtIndex(index)
            if (delegate && delegate.controlItem && delegate.controlItem.rowFocus)
                focused.push(delegate.rowKey)
        }
        return focused
    }

    function init() {
        settingsList.forceActiveFocus()
    }

    // Expanding a group inserts rows above everything that follows it. The
    // rows below keep their delegates and only their index moves.
    function test_single_highlight_after_insert() {
        seed(["appearance/scale", "playback/mpvConfigMode", "streaming/manual", "action/toggleAdvanced/Streaming"])
        settingsList.currentIndex = 1
        compare(focusedRowKeys(), ["playback/mpvConfigMode"])

        reconcile(["appearance/scale", "playback/audioOutput", "playback/mpvConfigMode", "playback/mpvConfigDirectory",
                   "streaming/manual", "action/toggleAdvanced/Streaming"])
        settingsList.currentIndex = SettingsNavigation.indexForRowKey(rowsModel, "playback/mpvConfigMode")
        compare(focusedRowKeys(), ["playback/mpvConfigMode"])
    }

    // Switching mpv configuration away from a custom directory drops the path
    // row, shifting every row after it the other way.
    function test_single_highlight_after_remove() {
        seed(["appearance/scale", "playback/mpvConfigMode", "playback/mpvConfigDirectory", "streaming/manual",
              "action/toggleAdvanced/Streaming"])
        settingsList.currentIndex = 2
        compare(focusedRowKeys(), ["playback/mpvConfigDirectory"])

        reconcile(["appearance/scale", "playback/mpvConfigMode", "streaming/manual", "action/toggleAdvanced/Streaming"])
        settingsList.currentIndex = SettingsNavigation.indexForRowKey(rowsModel, "playback/mpvConfigMode")
        compare(focusedRowKeys(), ["playback/mpvConfigMode"])
    }

    // The same copy drives which row a click activates, so a stale one sent
    // pointer activation to a neighbour.
    function test_row_index_tracks_the_model() {
        seed(["appearance/scale", "playback/mpvConfigMode", "streaming/manual"])
        reconcile(["appearance/scale", "playback/audioOutput", "playback/mpvConfigMode", "streaming/manual"])
        for (let index = 0; index < rowsModel.count; ++index) {
            const delegate = settingsList.itemAtIndex(index)
            verify(delegate && delegate.controlItem)
            compare(delegate.controlItem.rowIndex, index)
            compare(delegate.controlItem.rowTitle, rowsModel.get(index).rowKey.toUpperCase())
        }
    }
}
