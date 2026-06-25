import QtQuick

Item {
    id: root

    property var sourceModel
    property int count: 0

    visible: false
    width: 0
    height: 0

    function modelCount() {
        if (!sourceModel)
            return 0
        if (typeof sourceModel.rowCount === "function")
            return sourceModel.rowCount()
        return sourceModel.count !== undefined ? sourceModel.count : 0
    }

    function refresh() {
        count = modelCount()
    }

    function refreshLater() {
        refreshTimer.restart()
    }

    onSourceModelChanged: refreshLater()
    Component.onCompleted: refreshLater()

    Timer {
        id: refreshTimer
        interval: 0
        repeat: false
        onTriggered: root.refresh()
    }

    Connections {
        target: root.sourceModel
        ignoreUnknownSignals: true
        function onCountChanged() { root.refreshLater() }
        function onModelReset() { root.refreshLater() }
        function onRowsInserted() { root.refreshLater() }
        function onRowsMoved() { root.refreshLater() }
        function onRowsRemoved() { root.refreshLater() }
        function onLayoutChanged() { root.refreshLater() }
    }
}
