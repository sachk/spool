#include "MpvLifecycle.h"
extern "C" {
#include <mpv/client.h>
}

#include <QDebug>

#include <utility>

namespace JellyfinNative {

MpvLifecycle::~MpvLifecycle()
{
    destroy();
}

mpv_handle *MpvLifecycle::handle() const
{
    return m_handle.load();
}

bool MpvLifecycle::adopt(mpv_handle *handle, EventHandler eventHandler)
{
    if (!handle || m_handle.load() || m_eventThread.joinable())
        return false;

    m_terminating = false;
    m_pendingFileLoads = 0;
    m_handle = handle;
    m_eventThread = std::thread([this, handle, eventHandler = std::move(eventHandler)]() mutable {
        runEventLoop(handle, std::move(eventHandler));
    });
    return true;
}

void MpvLifecycle::destroy(BeforeDestroy beforeDestroy)
{
    mpv_handle *handle = m_handle.exchange(nullptr);
    if (!handle) {
        if (m_eventThread.joinable())
            m_eventThread.join();
        return;
    }

    if (beforeDestroy)
        beforeDestroy(handle);

    m_terminating = true;
    if (m_eventThread.joinable())
        m_eventThread.join();

    qInfo() << "player: calling mpv_terminate_destroy";
    mpv_terminate_destroy(handle);
    qInfo() << "player: mpv_terminate_destroy returned";

    m_terminating = false;
    m_pendingFileLoads = 0;
}

void MpvLifecycle::requestEventLoopStop()
{
    m_terminating = true;
}

void MpvLifecycle::beginFileLoad()
{
    m_pendingFileLoads.fetch_add(1, std::memory_order_acq_rel);
}

void MpvLifecycle::cancelFileLoad()
{
    int pending = m_pendingFileLoads.load(std::memory_order_acquire);
    while (pending > 0
        && !m_pendingFileLoads.compare_exchange_weak(
            pending, pending - 1, std::memory_order_acq_rel, std::memory_order_acquire)) {
    }
}

void MpvLifecycle::completeFileLoad()
{
    cancelFileLoad();
}

bool MpvLifecycle::hasPendingFileLoads() const
{
    return m_pendingFileLoads.load(std::memory_order_acquire) > 0;
}

void MpvLifecycle::runEventLoop(mpv_handle *handle, EventHandler eventHandler)
{
    while (!m_terminating.load()) {
        mpv_event *event = mpv_wait_event(handle, 0.1);
        if (event && eventHandler)
            eventHandler(event);
    }
}

} // namespace JellyfinNative
