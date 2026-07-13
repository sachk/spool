#pragma once

#include <QMutex>
#include <QPointer>
#include <QQuickFramebufferObject>
#include <QtQmlIntegration/qqmlintegration.h>

#include <atomic>
#include <memory>

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
    struct RenderDiagnostics {
        quint64 requests = 0;
        quint64 frames = 0;
        quint64 totalNanoseconds = 0;
        quint64 maxNanoseconds = 0;
    };

    explicit MpvVideoItem(QQuickItem *parent = nullptr);
    ~MpvVideoItem() override;

    // Called from the GUI thread. A non-null handle is adopted by the renderer
    // on its next frame, with Qt's OpenGL context current.
    void setMpvHandle(mpv_handle *handle);

    // Release the render context on Qt's render thread and wait for that short
    // handoff before PlayerController destroys the underlying mpv core.
    bool releaseMpvHandle(int timeoutMs = 5000);

    static MpvVideoItem *instance();
    void setDiagnosticsEnabled(bool enabled);
    RenderDiagnostics takeRenderDiagnostics();

    Renderer *createRenderer() const override;

    // Renderer-side: atomically read the latest handle update and clear the
    // dirty flag. `dirty` is true only on the first sync after setMpvHandle().
    struct HandleSnapshot {
        mpv_handle *handle;
        bool dirty;
        QPointer<QObject> releaseWaiter;
        std::shared_ptr<std::atomic_bool> releaseCompleted;
    };
    HandleSnapshot takePendingHandle();

    // Published for lifecycle diagnostics. Creation and destruction happen
    // exclusively in Renderer::render(), with the original GL context current.
    std::atomic<mpv_render_context *> m_renderCtxAtomic { nullptr };
    std::atomic_bool m_diagnosticsEnabled { false };
    std::atomic<quint64> m_renderRequests { 0 };
    std::atomic<quint64> m_renderFrames { 0 };
    std::atomic<quint64> m_renderNanoseconds { 0 };
    std::atomic<quint64> m_maxRenderNanoseconds { 0 };

signals:
    void renderError(const QString& message);

private:
    static MpvVideoItem *s_instance;

    QMutex m_handleMutex;
    mpv_handle *m_pendingHandle = nullptr;
    bool m_handleDirty = false;
    QPointer<QObject> m_releaseWaiter;
    std::shared_ptr<std::atomic_bool> m_releaseCompleted;
};

} // namespace JellyfinNative
