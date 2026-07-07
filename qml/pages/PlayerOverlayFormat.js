.pragma library

function formatClock(seconds) {
    const total = Math.max(0, Math.floor(seconds || 0))
    const hours = Math.floor(total / 3600)
    const minutes = Math.floor((total % 3600) / 60)
    const secs = total % 60
    return hours > 0 ? hours + ":" + String(minutes).padStart(2, "0") + ":" + String(secs).padStart(2, "0") : minutes + ":" + String(secs).padStart(2, "0")
}

function formatAudioDelay(ms) {
    if (ms > 0)
        return "+" + ms + " ms"
    return ms + " ms"
}

function actionIcon(value, paused, fullScreen) {
    if (value === "back") return "fast_rewind"
    if (value === "pause") return paused ? "play_arrow" : "pause"
    if (value === "forward") return "fast_forward"
    if (value === "prevChapter") return "skip_previous"
    if (value === "nextChapter") return "skip_next"
    if (value === "subtitles") return "closed_caption"
    if (value === "audio") return "audiotrack"
    if (value === "queue") return "playlist_play"
    if (value === "fullscreen") return fullScreen ? "fullscreen_exit" : "fullscreen"
    return "settings"
}
