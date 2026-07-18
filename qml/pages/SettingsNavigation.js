.pragma library

function rowAvailable(row, isTV, hdrPlayback, valueForKey) {
    if (!row || row.visible === false)
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

function detailLevel(row) {
    return row && row.level !== undefined ? Number(row.level) : 0
}

function clampIndex(index, count) {
    if (count <= 0)
        return -1
    return Math.max(0, Math.min(count - 1, index))
}
