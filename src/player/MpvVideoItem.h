#pragma once

#include <QMutex>
#include <QPointer>
#include <QQuickFramebufferObject>
#include <QtQmlIntegration/qqmlintegration.h>

#include <atomic>

struct mpv_handle;
struct mpv_render_context;

namespace JellyfinNative {

// QQuickFramebufferObject that hosts libmpv's render API. PlayerController
// hands us an mpv_handle via setMpvHandle(); the scene-graph render thread
// then creates an mpv_render_context bound to Qt's OpenGL context and renders
// each frame into our FBO. Used on non-Starfish (desktop) builds where mpv
// would otherwise pop its own toplevel window via vo=gpu.
class MpvVideoItem : public QQuickFramebufferObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(MpvVideoItem)

public:
    explicit MpvVideoItem(QQuickItem *parent = nullptr);
    ~MpvVideoItem() override;

    // Called from the GUI thread. Pass nullptr to release the current handle
    // (must happen before PlayerController destroys the mpv_handle). This call
    // synchronously waits for the render thread to free the render context so
    // it is safe to mpv_terminate_destroy() the handle on return.
    void setMpvHandle(mpv_handle *handle);

    static MpvVideoItem *instance();

    Renderer *createRenderer() const override;

    // Renderer-side: atomically read the latest handle update and clear the
    // dirty flag. `dirty` is true only on the first sync after setMpvHandle().
    struct HandleSnapshot {
        mpv_handle *handle;
        bool dirty;
    };
    HandleSnapshot takePendingHandle();

private:
    static MpvVideoItem *s_instance;

    QMutex m_handleMutex;
    mpv_handle *m_pendingHandle = nullptr;
    bool m_handleDirty = false;
};

} // namespace JellyfinNative
