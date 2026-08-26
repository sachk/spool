import QtQuick
import QtTest
import "../../qml/primitives/ModelAccess.js" as ModelAccess

// Three model shapes reach the same rows; this is the only place that is
// allowed to care which one arrived.
TestCase {
    name: "ModelAccess"

    ListModel {
        id: listModel
        ListElement {
            title: "one"
        }
        ListElement {
            title: "two"
        }
    }

    function test_countsEveryModelShape() {
        compare(ModelAccess.count(null), 0)
        compare(ModelAccess.count(undefined), 0)
        compare(ModelAccess.count(["a", "b", "c"]), 3)
        compare(ModelAccess.count(listModel), 2)
        compare(ModelAccess.count({
                                      "rowCount": function () {
                                          return 5
                                      }
                                  }), 5)
    }

    function test_readsEveryModelShape() {
        compare(ModelAccess.at(["a", "b"], 1), "b")
        compare(ModelAccess.at(listModel, 0).title, "one")
    }

    function test_outOfRangeReadsAreEmptyRatherThanUndefined() {
        compare(JSON.stringify(ModelAccess.at(["a"], 5)), "{}")
        compare(JSON.stringify(ModelAccess.at(["a"], -1)), "{}")
        compare(JSON.stringify(ModelAccess.at(null, 0)), "{}")
    }
}
