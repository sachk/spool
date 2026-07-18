#pragma once

#include <memory>

namespace JellyfinNative {

class ScreenSaverInhibitor final {
public:
    ScreenSaverInhibitor();
    ~ScreenSaverInhibitor();

    ScreenSaverInhibitor(const ScreenSaverInhibitor&) = delete;
    ScreenSaverInhibitor& operator=(const ScreenSaverInhibitor&) = delete;

    void setInhibited(bool inhibited);
    bool inhibited() const;

private:
    struct PlatformData;
    std::unique_ptr<PlatformData> m_platform;
    bool m_inhibited = false;
};

} // namespace JellyfinNative
