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
class MpvVideoItem : public QQuickFramebufferObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(MpvVideoItem)

public:
    explicit MpvVideoItem(QQuickItem *parent = nullptr);
    ~MpvVideoItem() override;

    // Called from the GUI thread. Pass nullptr to release the current handle
    // before PlayerController destroys the mpv_handle. The render context is
    // freed synchronously here via mpv_render_context_free (thread-safe, and
    // it waits for any in-progress render), so we do not depend on the
    // scene-graph render thread still being alive — this is safe to call from
    // QCoreApplication::aboutToQuit on shutdown.
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

    // Published by the renderer in synchronize() once the render context is
    // created. setMpvHandle(nullptr) atomically claims and frees it. The
    // renderer's render() also loads this and skips when null, so a free
    // racing with a render is safe (mpv_render_context_free waits for the
    // in-progress mpv_render_context_render call to finish).
    std::atomic<mpv_render_context *> m_renderCtxAtomic { nullptr };

signals:
    void renderError(const QString& message);

private:
    static MpvVideoItem *s_instance;

    QMutex m_handleMutex;
    mpv_handle *m_pendingHandle = nullptr;
    bool m_handleDirty = false;
};

} // namespace JellyfinNative
