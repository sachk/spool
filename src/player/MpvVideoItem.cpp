#include "MpvVideoItem.h"

#include <QMutexLocker>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QQuickWindow>
#include <QRunnable>
#include <QSemaphore>
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
        if (m_renderCtx) {
            mpv_render_context_free(m_renderCtx);
            m_renderCtx = nullptr;
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
        const auto snap = item->takePendingHandle();
        if (!snap.dirty)
            return;
        mpv_handle *next = snap.handle;

        if (m_renderCtx) {
            mpv_render_context_free(m_renderCtx);
            m_renderCtx = nullptr;
        }

        if (!next)
            return;

        mpv_opengl_init_params glInit{};
        glInit.get_proc_address = &getProcAddressGl;
        int advanced = 1;
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInit},
            {MPV_RENDER_PARAM_ADVANCED_CONTROL, &advanced},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };

        const int err = mpv_render_context_create(&m_renderCtx, next, params);
        if (err < 0) {
            qFatal("MpvVideoItem: mpv_render_context_create failed: %s",
                   mpv_error_string(err));
            return;
        }
        mpv_render_context_set_update_callback(m_renderCtx, &MpvFboRenderer::onMpvUpdate,
                                               static_cast<void *>(item));
    }

    void render() override
    {
        if (!m_renderCtx)
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
        mpv_render_context_render(m_renderCtx, params);
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
    mpv_render_context *m_renderCtx = nullptr;
};

class TeardownJob final : public QRunnable
{
public:
    TeardownJob(MpvVideoItem *item, QSemaphore *done)
        : m_item(item)
        , m_done(done)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        // Force the renderer to pick up the cleared handle and free its
        // mpv_render_context before we let setMpvHandle() return.
        m_item->update();
        m_done->release();
    }

private:
    MpvVideoItem *m_item;
    QSemaphore *m_done;
};

} // namespace

MpvVideoItem *MpvVideoItem::s_instance = nullptr;

MpvVideoItem::MpvVideoItem(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
{
    if (s_instance)
        qWarning() << "MpvVideoItem: replacing existing singleton instance";
    s_instance = this;
    // mpv writes the FBO with GL's bottom-left origin; Qt scene graph samples
    // it top-down. Mirror vertically to compensate.
    setMirrorVertically(true);
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

    // Tearing the handle down: synchronously wait until the render thread has
    // freed the mpv_render_context. Otherwise PlayerController would
    // mpv_terminate_destroy() the handle while the renderer still uses it.
    auto *win = window();
    if (!win) {
        // No window yet — nothing to free.
        return;
    }

    QSemaphore done;
    win->scheduleRenderJob(new TeardownJob(this, &done),
                           QQuickWindow::BeforeSynchronizingStage);
    win->update();

    // Wait, but pump the GUI event loop so a queued window->update() can run
    // if we happen to be on the same thread that owns the window.
    while (!done.tryAcquire(1, 16)) {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 16);
    }
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
