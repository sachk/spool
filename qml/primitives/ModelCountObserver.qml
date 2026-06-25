import QtQuick

Item {
    id: root

    property var sourceModel
    property int count: 0

    visible: false
    width: 0
    height: 0

    function refresh() {
        if (!sourceModel) {
            count = 0
            return
        }
        count = sourceModel.count !== undefined ? sourceModel.count
                                                : sourceModel.rowCount ? sourceModel.rowCount() : 0
    }

    onSourceModelChanged: refresh()
    Component.onCompleted: refresh()

    Connections {
        target: root.sourceModel
        function onCountChanged() { root.refresh() }
        function onModelReset() { root.refresh() }
        function onRowsInserted() { root.refresh() }
        function onRowsRemoved() { root.refresh() }
    }
}
