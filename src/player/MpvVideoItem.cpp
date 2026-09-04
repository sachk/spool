#include "MpvVideoItem.h"

#include <QEventLoop>
#include <QMetaObject>
#include <QMutexLocker>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QPointer>
#include <QQuickWindow>
#include <QTimer>
#include <QtDebug>

extern "C" {
#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>
}

namespace JellyfinNative {

namespace {

    void *getProcAddressGl(void *, const char *name)
    {
        QOpenGLContext *gl = QOpenGLContext::currentContext();
        if (!gl)
            return nullptr;
        return reinterpret_cast<void *>(gl->getProcAddress(QByteArray(name)));
    }

    class MpvFboRenderer final : public QQuickFramebufferObject::Renderer {
    public:
        explicit MpvFboRenderer(MpvVideoItem *item)
            : m_item(item)
        {
        }

        ~MpvFboRenderer() override
        {
            QObject::disconnect(m_frameSwappedConnection);
            releaseRenderContext();
        }

        QOpenGLFramebufferObject *createFramebufferObject(const QSize& size) override
        {
            m_hasRenderedFrame = false;
            m_hasRenderedVideoFrame = false;
            m_swapPending = false;
            m_firstVideoFrameSwapPending = false;
            QOpenGLFramebufferObjectFormat fmt;
            fmt.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
            return new QOpenGLFramebufferObject(size, fmt);
        }

        void synchronize(QQuickFramebufferObject *fbo) override
        {
            setWindow(fbo->window());
            auto *item = static_cast<MpvVideoItem *>(fbo);
            m_item = item;
            const auto snap = item->takePendingHandle();
            if (!snap.dirty)
                return;

            m_nextHandle = snap.handle;
            m_handleDirty = true;
            m_releaseWaiter = snap.releaseWaiter;
            m_releaseCompleted = snap.releaseCompleted;
            m_attachCompleted = snap.attachCompleted;
        }

        void render() override
        {
            if (!m_item)
                return;

            if (m_handleDirty) {
                releaseRenderContext();
                if (m_nextHandle)
                    createRenderContext(m_nextHandle);
                m_nextHandle = nullptr;
                m_handleDirty = false;
                completeReleaseWaiter();
                completeAttachHandoff();
            }

            mpv_render_context *ctx = m_item->m_renderCtxAtomic.load();
            if (!ctx)
                return;

            const uint64_t updateFlags = mpv_render_context_update(ctx);
            if (m_hasRenderedFrame && !(updateFlags & MPV_RENDER_UPDATE_FRAME))
                return;

            const bool firstVideoFrame = !m_hasRenderedVideoFrame && (updateFlags & MPV_RENDER_UPDATE_FRAME);

            QOpenGLFramebufferObject *fbo = framebufferObject();
            if (!fbo)
                return;

            mpv_opengl_fbo mpfbo {};
            mpfbo.fbo = static_cast<int>(fbo->handle());
            mpfbo.w = fbo->width();
            mpfbo.h = fbo->height();
            mpfbo.internal_format = 0;

            mpv_render_param params[] = {
                { MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo },
                { MPV_RENDER_PARAM_INVALID, nullptr },
            };

            if (m_window)
                m_window->beginExternalCommands();
            mpv_render_context_render(ctx, params);
            if (m_window)
                m_window->endExternalCommands();
            m_hasRenderedFrame = true;
            m_swapPending = true;
            if (firstVideoFrame) {
                m_hasRenderedVideoFrame = true;
                m_firstVideoFrameSwapPending = true;
                qInfo() << "player: first video frame rendered";
            }
        }

    private:
        void setWindow(QQuickWindow *window)
        {
            if (m_window == window)
                return;

            QObject::disconnect(m_frameSwappedConnection);
            m_window = window;
            if (!m_window)
                return;

            m_frameSwappedConnection = QObject::connect(
                m_window, &QQuickWindow::frameSwapped, m_window,
                [this] {
                    if (!m_item || !m_swapPending)
                        return;

                    m_swapPending = false;
                    if (auto *ctx = m_item->m_renderCtxAtomic.load()) {
                        mpv_render_context_report_swap(ctx);
                        if (m_firstVideoFrameSwapPending) {
                            m_firstVideoFrameSwapPending = false;
                            qInfo() << "player: first video frame swapped";
                        }
                    }
                },
                Qt::DirectConnection);
        }

        void createRenderContext(mpv_handle *next)
        {
            mpv_opengl_init_params glInit {};
            glInit.get_proc_address = &getProcAddressGl;
            // Deliberately do NOT set MPV_RENDER_PARAM_ADVANCED_CONTROL.
            //
            // advanced_control routes VOCTRL_PERFORMANCE_DATA (and SCREENSHOT)
            // through mp_dispatch_run on the render context's dispatch queue,
            // which is a *synchronous* call that blocks the caller until the
            // render thread next enters mpv_render_context_render. mpv's stats
            // overlay queries vo_passes (= VOCTRL_PERFORMANCE_DATA) on a
            // periodic lua timer while holding the core dispatch lock, so any
            // delay on the render thread (Wayland frame-callback throttling
            // under nixGL on this user's setup) stalls the core, freezing
            // playback. Without advanced_control the control falls through to
            // control_cb (null here) and returns VO_NOTIMPL immediately — stats
            // simply don't show GPU pass timings, and direct rendering / mpv
            // screenshots are disabled, neither of which we use.
#ifdef JELLYFIN_NATIVE_WEBOS
            static constexpr const char *backends[] = { "gpu" };
#else
            static constexpr const char *backends[] = { "gpu-next", "gpu" };
#endif

            mpv_render_context *newCtx = nullptr;
            int err = MPV_ERROR_UNSUPPORTED;
            for (const char *backend : backends) {
                mpv_render_param params[] = {
                    { MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL) },
                    { MPV_RENDER_PARAM_BACKEND, const_cast<char *>(backend) },
                    { MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInit },
                    { MPV_RENDER_PARAM_INVALID, nullptr },
                };

                newCtx = nullptr;
                err = mpv_render_context_create(&newCtx, next, params);
                if (err >= 0) {
                    qInfo() << "player: render backend" << backend;
                    break;
                }
                qWarning() << "player: render backend" << backend << "unavailable:" << mpv_error_string(err);
            }
            if (err < 0) {
                const QString message = QStringLiteral("Failed to initialize video rendering: %1")
                                            .arg(QString::fromUtf8(mpv_error_string(err)));
                qCritical() << "MpvVideoItem:" << message;
                const QPointer<MpvVideoItem> guardedItem(m_item);
                QMetaObject::invokeMethod(
                    m_item,
                    [guardedItem, message]() {
                        if (guardedItem)
                            emit guardedItem->renderError(message);
                    },
                    Qt::QueuedConnection);
                return;
            }
            mpv_render_context_set_update_callback(newCtx, &MpvFboRenderer::onMpvUpdate, static_cast<void *>(m_item));
            m_item->m_renderCtxAtomic.store(newCtx);
        }

        void releaseRenderContext()
        {
            m_hasRenderedFrame = false;
            m_hasRenderedVideoFrame = false;
            m_swapPending = false;
            m_firstVideoFrameSwapPending = false;
            if (!m_item)
                return;
            if (auto *ctx = m_item->m_renderCtxAtomic.exchange(nullptr)) {
                mpv_render_context_set_update_callback(ctx, nullptr, nullptr);
                mpv_render_context_free(ctx);
            }
        }

        void completeReleaseWaiter()
        {
            if (!m_releaseCompleted)
                return;
            m_releaseCompleted->store(true);
            if (m_releaseWaiter)
                QMetaObject::invokeMethod(m_releaseWaiter, "quit", Qt::QueuedConnection);
            m_releaseWaiter = nullptr;
            m_releaseCompleted.reset();
        }

        void completeAttachHandoff()
        {
            if (!m_attachCompleted)
                return;
            m_attachCompleted->store(true);
            m_attachCompleted.reset();
            if (m_item)
                QMetaObject::invokeMethod(m_item, "renderContextHandoffCompleted", Qt::QueuedConnection);
        }

        static void onMpvUpdate(void *ctx)
        {
            auto *item = static_cast<MpvVideoItem *>(ctx);
            QMetaObject::invokeMethod(item, "update", Qt::QueuedConnection);
        }

        QMetaObject::Connection m_frameSwappedConnection;
        MpvVideoItem *m_item = nullptr;
        QQuickWindow *m_window = nullptr;
        mpv_handle *m_nextHandle = nullptr;
        bool m_handleDirty = false;
        bool m_hasRenderedFrame = false;
        bool m_hasRenderedVideoFrame = false;
        bool m_swapPending = false;
        bool m_firstVideoFrameSwapPending = false;
        QPointer<QObject> m_releaseWaiter;
        std::shared_ptr<std::atomic_bool> m_releaseCompleted;
        std::shared_ptr<std::atomic_bool> m_attachCompleted;
    };

} // namespace

MpvVideoItem *MpvVideoItem::s_instance = nullptr;

MpvVideoItem::MpvVideoItem(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
{
    if (s_instance)
        qWarning() << "MpvVideoItem: replacing existing singleton instance";
    s_instance = this;
}

MpvVideoItem::~MpvVideoItem()
{
    if (s_instance == this)
        s_instance = nullptr;
}

MpvVideoItem *MpvVideoItem::instance()
{
    return s_instance;
}

void MpvVideoItem::setMpvHandle(mpv_handle *handle)
{
    Q_ASSERT(handle);
    {
        QMutexLocker locker(&m_handleMutex);
        m_pendingHandle = handle;
        m_handleDirty = true;
        m_releaseWaiter = nullptr;
        m_releaseCompleted.reset();
        m_attachCompleted = std::make_shared<std::atomic_bool>(false);
    }
    update();
}

bool MpvVideoItem::waitForRenderContext(int timeoutMs)
{
    std::shared_ptr<std::atomic_bool> completed;
    {
        QMutexLocker locker(&m_handleMutex);
        completed = m_attachCompleted;
    }
    if (!completed)
        return m_renderCtxAtomic.load() != nullptr;

    QEventLoop waitLoop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &waitLoop, &QEventLoop::quit);
    QObject::connect(this, &MpvVideoItem::renderContextHandoffCompleted, &waitLoop, &QEventLoop::quit);
    timeout.start(timeoutMs);
    if (!completed->load())
        waitLoop.exec(QEventLoop::ExcludeUserInputEvents);
    const bool ready = completed->load() && m_renderCtxAtomic.load() != nullptr;
    if (!ready) {
        // The handoff runs on the render thread, so it only happens once the
        // scene graph draws this item. Report what kept it from being drawn.
        qWarning() << "player: render context handoff did not complete within" << timeoutMs
                   << "ms handedOff=" << completed->load() << "visible=" << isVisible() << "size=" << width() << "x"
                   << height() << "window=" << (window() != nullptr)
                   << "windowExposed=" << (window() && window()->isExposed());
    }
    return ready;
}

bool MpvVideoItem::releaseMpvHandle(int timeoutMs)
{
    QEventLoop releaseLoop;
    const auto completed = std::make_shared<std::atomic_bool>(false);
    {
        QMutexLocker locker(&m_handleMutex);
        const bool needsRenderHandoff = m_renderCtxAtomic.load() || m_pendingHandle;
        m_pendingHandle = nullptr;
        m_handleDirty = true;
        if (!needsRenderHandoff)
            return true;
        m_releaseWaiter = &releaseLoop;
        m_releaseCompleted = completed;
    }

    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &releaseLoop, &QEventLoop::quit);
    timeout.start(timeoutMs);
    update();
    releaseLoop.exec(QEventLoop::ExcludeUserInputEvents);
    return completed->load();
}

QQuickFramebufferObject::Renderer *MpvVideoItem::createRenderer() const
{
    return new MpvFboRenderer(const_cast<MpvVideoItem *>(this));
}

MpvVideoItem::HandleSnapshot MpvVideoItem::takePendingHandle()
{
    QMutexLocker locker(&m_handleMutex);
    HandleSnapshot snap { m_pendingHandle, m_handleDirty, m_releaseWaiter, m_releaseCompleted, m_attachCompleted };
    m_handleDirty = false;
    m_releaseWaiter = nullptr;
    m_releaseCompleted.reset();
    return snap;
}

} // namespace JellyfinNative
