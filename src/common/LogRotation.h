#pragma once

#include <cstdio>
#include <limits.h>

namespace JellyfinNative {

inline void rotateLogFile(const char *path)
{
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
