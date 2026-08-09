.pragma library

function rowAvailable(row, isTV, hdrPlayback, valueForKey) {
    if (!row)
        return false
    if (row.platform === "desktop" && isTV)
        return false
    if (row.platform === "webos" && !isTV)
        return false
    if (row.requiresHdrPlayback && !hdrPlayback)
        return false
    if (row.dependsOnKey && String(valueForKey(row.dependsOnKey)) !== String(row.dependsOnValue))
        return false
    return true
}

// Flatten hand-authored sections into a MenuListView model. resolve(key)
// returns the spec to show for a key, or a falsy value to leave it out; a
// section whose rows all drop out takes its header with it. Header entries are
// tagged so MenuListView's default rowEnabled skips over them.
function sectionedRows(sections, resolve) {
    const rows = []
    for (let s = 0; s < sections.length; ++s) {
        const section = sections[s]
        const visible = []
        for (let k = 0; k < section.keys.length; ++k) {
            const spec = resolve(section.keys[k])
            if (spec) {
                visible.push({
                                 "section": false,
                                 "spec": spec
                             })
            }
        }
        if (visible.length === 0)
            continue
        rows.push({
                      "section": true,
                      "spec": {
                          "title": section.title
                      }
                  })
        for (let v = 0; v < visible.length; ++v)
            rows.push(visible[v])
    }
    return rows
}

function firstActionableRow(rows, start) {
    for (let index = Math.max(0, start); index < rows.length; ++index) {
        if (!rows[index].section)
            return index
    }
    return -1
}

function detailLevel(row) {
    return row && row.level !== undefined ? Number(row.level) : 0
}

function clampIndex(index, count) {
    if (count <= 0)
        return -1
    return Math.max(0, Math.min(count - 1, index))
}

function rowCount(rows) {
    if (!rows)
        return 0
    return rows.count !== undefined ? Number(rows.count) : Number(rows.length || 0)
}

function rowAt(rows, index) {
    if (!rows || index < 0 || index >= rowCount(rows))
        return null
    return rows.get ? rows.get(index) : rows[index]
}

function indexForRowKey(rows, key) {
    for (let index = 0; index < rowCount(rows); ++index) {
        const row = rowAt(rows, index)
        if (row && row.rowKey === key)
            return index
    }
    return -1
}

function nearestRowKey(rows, sourceIndex) {
    let best = null
    let bestDistance = Number.POSITIVE_INFINITY
    for (let index = 0; index < rowCount(rows); ++index) {
        const row = rowAt(rows, index)
        if (!row)
            continue
        const distance = Math.abs(Number(row.sourceIndex) - Number(sourceIndex))
        const precedes = Number(row.sourceIndex) <= Number(sourceIndex)
        const bestPrecedes = best && Number(best.sourceIndex) <= Number(sourceIndex)
        if (distance < bestDistance || (distance === bestDistance && precedes && !bestPrecedes)) {
            best = row
            bestDistance = distance
        }
    }
    return best ? String(best.rowKey || "") : ""
}

function reconcileRows(model, nextRows) {
    const oldCount = rowCount(model)
    const nextCount = rowCount(nextRows)
    let prefix = 0
    while (prefix < oldCount && prefix < nextCount
           && rowAt(model, prefix).rowKey === rowAt(nextRows, prefix).rowKey)
        ++prefix

    let suffix = 0
    while (suffix < oldCount - prefix && suffix < nextCount - prefix
           && rowAt(model, oldCount - suffix - 1).rowKey === rowAt(nextRows, nextCount - suffix - 1).rowKey)
        ++suffix

    const removeCount = oldCount - prefix - suffix
    if (removeCount > 0)
        model.remove(prefix, removeCount)
    const insertCount = nextCount - prefix - suffix
    for (let index = 0; index < insertCount; ++index)
        model.insert(prefix + index, rowAt(nextRows, prefix + index))
    if (model.set) {
        for (let index = 0; index < prefix; ++index)
            model.set(index, rowAt(nextRows, index))
        for (let offset = 0; offset < suffix; ++offset) {
            const index = nextCount - suffix + offset
            model.set(index, rowAt(nextRows, index))
        }
    }
}
