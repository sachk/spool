#include "MpvVideoItem.h"

#include <QMutexLocker>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QPointer>
#include <QQuickWindow>
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

class MpvFboRenderer final : public QQuickFramebufferObject::Renderer
{
public:
    explicit MpvFboRenderer(MpvVideoItem *item)
        : m_item(item)
    {
    }

    ~MpvFboRenderer() override
    {
        if (m_item) {
            if (auto *ctx = m_item->m_renderCtxAtomic.exchange(nullptr))
                mpv_render_context_free(ctx);
        }
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override
    {
        QOpenGLFramebufferObjectFormat fmt;
        fmt.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        return new QOpenGLFramebufferObject(size, fmt);
    }

    void synchronize(QQuickFramebufferObject *fbo) override
    {
        m_window = fbo->window();
        auto *item = static_cast<MpvVideoItem *>(fbo);
        m_item = item;
        const auto snap = item->takePendingHandle();
        if (!snap.dirty)
            return;
        mpv_handle *next = snap.handle;

        // Free the old context (if still ours — setMpvHandle(nullptr) may
        // have already claimed and freed it from the GUI thread).
        if (auto *old = item->m_renderCtxAtomic.exchange(nullptr))
            mpv_render_context_free(old);

        if (!next)
            return;

        mpv_opengl_init_params glInit{};
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
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
#ifndef JELLYFIN_NATIVE_WEBOS
            {MPV_RENDER_PARAM_BACKEND, const_cast<char *>("gpu-next")},
#endif
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInit},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };

        mpv_render_context *newCtx = nullptr;
        const int err = mpv_render_context_create(&newCtx, next, params);
        if (err < 0) {
            const QString message =
                QStringLiteral("Failed to initialize video rendering: %1")
                    .arg(QString::fromUtf8(mpv_error_string(err)));
            qCritical() << "MpvVideoItem:" << message;
            const QPointer<MpvVideoItem> guardedItem(item);
            QMetaObject::invokeMethod(
                item,
                [guardedItem, message]() {
                    if (guardedItem)
                        emit guardedItem->renderError(message);
                },
                Qt::QueuedConnection);
            return;
        }
        mpv_render_context_set_update_callback(newCtx, &MpvFboRenderer::onMpvUpdate,
                                               static_cast<void *>(item));
        item->m_renderCtxAtomic.store(newCtx);
    }

    void render() override
    {
        if (!m_item)
            return;
        // Atomic load so a concurrent setMpvHandle(nullptr) that frees the
        // context doesn't leave us with a dangling pointer. If the GUI thread
        // claims the context between the load and the call below,
        // mpv_render_context_free blocks until our render finishes — safe.
        mpv_render_context *ctx = m_item->m_renderCtxAtomic.load();
        if (!ctx)
            return;

        QOpenGLFramebufferObject *fbo = framebufferObject();
        if (!fbo)
            return;

        mpv_opengl_fbo mpfbo{};
        mpfbo.fbo = static_cast<int>(fbo->handle());
        mpfbo.w = fbo->width();
        mpfbo.h = fbo->height();
        mpfbo.internal_format = 0;

        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };

        if (m_window)
            m_window->beginExternalCommands();
        mpv_render_context_render(ctx, params);
        if (m_window)
            m_window->endExternalCommands();
    }

private:
    static void onMpvUpdate(void *ctx)
    {
        auto *item = static_cast<MpvVideoItem *>(ctx);
        QMetaObject::invokeMethod(item, "update", Qt::QueuedConnection);
    }

    MpvVideoItem *m_item = nullptr;
    QQuickWindow *m_window = nullptr;
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
    {
        QMutexLocker locker(&m_handleMutex);
        m_pendingHandle = handle;
        m_handleDirty = true;
    }

    if (handle) {
        update();
        return;
    }

    // Tearing the handle down. Atomically claim the render context published
    // by the renderer and free it ourselves; mpv_render_context_free is
    // documented as thread-safe and waits for any in-progress render to
    // finish, so the renderer's render() (which loads the same atomic) is
    // either skipped entirely or completes before free returns. After this,
    // PlayerController can mpv_destroy() the handle without risk.
    if (auto *ctx = m_renderCtxAtomic.exchange(nullptr))
        mpv_render_context_free(ctx);
}

QQuickFramebufferObject::Renderer *MpvVideoItem::createRenderer() const
{
    return new MpvFboRenderer(const_cast<MpvVideoItem *>(this));
}

MpvVideoItem::HandleSnapshot MpvVideoItem::takePendingHandle()
{
    QMutexLocker locker(&m_handleMutex);
    HandleSnapshot snap{m_pendingHandle, m_handleDirty};
    m_handleDirty = false;
    return snap;
}

} // namespace JellyfinNative
