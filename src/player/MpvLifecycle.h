#pragma once

#include <atomic>
#include <functional>
#include <thread>

struct mpv_event;
struct mpv_handle;

namespace JellyfinNative {

class MpvLifecycle final {
public:
    using EventHandler = std::function<void(mpv_event *)>;
    using BeforeDestroy = std::function<void(mpv_handle *)>;

    ~MpvLifecycle();

    mpv_handle *handle() const;
    bool adopt(mpv_handle *handle, EventHandler eventHandler);
    void destroy(BeforeDestroy beforeDestroy = {});
    void requestEventLoopStop();

    void beginFileLoad();
    void cancelFileLoad();
    void completeFileLoad();
    bool hasPendingFileLoads() const;

private:
    void runEventLoop(mpv_handle *handle, EventHandler eventHandler);

    std::thread m_eventThread;
    std::atomic_bool m_terminating { false };
    std::atomic<int> m_pendingFileLoads { 0 };
    std::atomic<mpv_handle *> m_handle { nullptr };
};

} // namespace JellyfinNative
