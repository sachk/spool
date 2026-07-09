#pragma once

#include <cstdio>
#include <cstring>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace JellyfinNative {

// mkdir -p for the directory portion of a log path. The log directory lives
// under /tmp on webOS and is wiped every boot, so every writer must be able
// to recreate it — not just whichever code path happened to run first.
inline void ensureParentDirectoryExists(const char *path)
{
    char dir[PATH_MAX];
    const char *lastSlash = std::strrchr(path, '/');
    if (!lastSlash || lastSlash == path || static_cast<size_t>(lastSlash - path) >= sizeof(dir))
        return;
    std::memcpy(dir, path, lastSlash - path);
    dir[lastSlash - path] = '\0';
    for (char *p = dir + 1; *p; ++p) {
        if (*p != '/')
            continue;
        *p = '\0';
        ::mkdir(dir, 0755);
        *p = '/';
    }
    ::mkdir(dir, 0755);
}

inline void rotateLogFile(const char *path)
{
    ensureParentDirectoryExists(path);
    char older[PATH_MAX];
    char newer[PATH_MAX];
    if (std::snprintf(older, sizeof(older), "%s.2", path) < static_cast<int>(sizeof(older)))
        std::remove(older);
    if (std::snprintf(older, sizeof(older), "%s.1", path) < static_cast<int>(sizeof(older))
        && std::snprintf(newer, sizeof(newer), "%s.2", path) < static_cast<int>(sizeof(newer)))
        std::rename(older, newer);
    if (std::snprintf(newer, sizeof(newer), "%s.1", path) < static_cast<int>(sizeof(newer)))
        std::rename(path, newer);
}

} // namespace JellyfinNative
