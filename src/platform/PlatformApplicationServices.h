#pragma once

#include <memory>

class QGuiApplication;

namespace JellyfinNative {

class AppController;
class NativeAppWindow;
class RouterController;

class PlatformApplicationServices final {
public:
    PlatformApplicationServices(
        QGuiApplication& application, NativeAppWindow& window, AppController& controller, RouterController& router);
    ~PlatformApplicationServices();

    PlatformApplicationServices(const PlatformApplicationServices&) = delete;
    PlatformApplicationServices& operator=(const PlatformApplicationServices&) = delete;

    void start();

private:
    struct PlatformData;
    std::unique_ptr<PlatformData> m_platform;
};

} // namespace JellyfinNative
