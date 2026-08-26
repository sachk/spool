.pragma library

// Rows, grids and pages are handed models of three shapes: a
// QAbstractListModel from C++, a QVariantList, and a plain JS array. These
// two functions are the only place that difference is allowed to matter.

function count(model) {
    if (!model)
        return 0
    if (model.count !== undefined)
        return Number(model.count)
    if (model.length !== undefined)
        return Number(model.length)
    return model.rowCount ? Number(model.rowCount()) : 0
}

function at(model, index) {
    if (!model || index < 0 || index >= count(model))
        return ({})
    if (model.get)
        return model.get(index) || ({})
    return model[index] || ({})
}
