.pragma library

function move(profileIndex, profileCount, addFocused, delta) {
    const count = Math.max(0, profileCount)
    if (count === 0)
        return { profileIndex: -1, addFocused: true }
    if (delta > 0) {
        if (addFocused)
            return { profileIndex: count - 1, addFocused: true }
        if (profileIndex + 1 < count)
            return { profileIndex: profileIndex + 1, addFocused: false }
        return { profileIndex: count - 1, addFocused: true }
    }
    if (addFocused)
        return { profileIndex: count - 1, addFocused: false }
    return { profileIndex: Math.max(0, profileIndex - 1), addFocused: false }
}
