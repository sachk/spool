#pragma once

#include <QString>

namespace JellyfinNative {

struct MpvConfigPolicy {
    enum class Mode {
        Disabled,
        Standard,
        Custom,
    };

    Mode mode = Mode::Disabled;
    QString directory;
    bool valid = true;
    QString error;
};

MpvConfigPolicy validatedPlatformMpvConfigPolicy(const QString& mode, const QString& directory);

} // namespace JellyfinNative
