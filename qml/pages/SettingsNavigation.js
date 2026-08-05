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
