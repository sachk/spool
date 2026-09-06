#include "MpvVideoItem.h"

#include <QEventLoop>
#include <QMetaObject>
#include <QMutexLocker>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLFunctions>
#include <QPointer>
#include <QQuickWindow>
#include <QTimer>
#include <QtDebug>

#if defined(JELLYFIN_MPV_ITEM_RHI)
#include <QSGRendererInterface>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>
// Whether this Qt was built with Vulkan, asked in the only way that cannot
// disagree with itself: QT_CONFIG reads a feature macro that a mixed set of
// include paths can answer for a different Qt than the one supplying headers.
#if __has_include(<QVulkanInstance>)
#define JELLYFIN_MPV_ITEM_VULKAN 1
#include <QVulkanInstance>
#include <vulkan/vulkan.h>
#endif
#endif

#include <vector>

extern "C" {
#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>
#if defined(JELLYFIN_MPV_ITEM_VULKAN)
#include <mpv/render_vk.h>
#endif
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

#if !defined(JELLYFIN_MPV_ITEM_RHI)

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
            if (auto *gl = QOpenGLContext::currentContext()) {
                auto *functions = gl->functions();
                qInfo() << "player: OpenGL context" << gl->format()
                        << "vendor=" << reinterpret_cast<const char *>(functions->glGetString(GL_VENDOR))
                        << "renderer=" << reinterpret_cast<const char *>(functions->glGetString(GL_RENDERER))
                        << "version=" << reinterpret_cast<const char *>(functions->glGetString(GL_VERSION));
            }
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

#else // JELLYFIN_MPV_ITEM_RHI

    // The same lifecycle as the framebuffer renderer above, written out rather
    // than shared with it. Only one of the two is ever compiled, so a common
    // base would put the platforms that keep the framebuffer -- Android and
    // webOS, neither testable from here -- behind a class that nothing builds
    // on the one platform whose tests actually run this code.
    class MpvRhiRenderer final : public QQuickRhiItemRenderer {
    public:
        explicit MpvRhiRenderer(MpvVideoItem *item)
            : m_item(item)
        {
        }

        ~MpvRhiRenderer() override
        {
            QObject::disconnect(m_frameSwappedConnection);
            destroyGlFramebuffer();
            releaseRenderContext();
        }

    protected:
        void initialize(QRhiCommandBuffer *) override
        {
            // A new target, so anything remembered about the old one is stale
            // and the first frame has to be drawn again.
            m_hasRenderedFrame = false;
            m_hasRenderedVideoFrame = false;
            m_swapPending = false;
            m_firstVideoFrameSwapPending = false;
            destroyGlFramebuffer();
        }

        void synchronize(QQuickRhiItem *rhiItem) override
        {
            auto *item = static_cast<MpvVideoItem *>(rhiItem);
            setWindow(item->window());
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

        void render(QRhiCommandBuffer *cb) override
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

            QRhiTexture *target = colorTexture();
            if (!target || !cb)
                return;

            // Everything mpv does here is its own submission, not Qt's. This is
            // what makes that safe, and on Vulkan it is also what puts mpv's
            // work in the queue ahead of the scene graph sampling the result.
            cb->beginExternal();
            const bool drew = renderInto(ctx, target);
            cb->endExternal();
            if (!drew)
                return;

            m_hasRenderedFrame = true;
            m_swapPending = true;
            if (firstVideoFrame) {
                m_hasRenderedVideoFrame = true;
                m_firstVideoFrameSwapPending = true;
                qInfo() << "player: first video frame rendered";
            }
        }

    private:
        bool renderInto(mpv_render_context *ctx, QRhiTexture *target)
        {
            const QSize size = target->pixelSize();
            if (size.isEmpty())
                return false;

#if defined(JELLYFIN_MPV_ITEM_VULKAN)
            if (m_vulkan) {
                const QRhiTexture::NativeTexture native = target->nativeTexture();
                if (!native.object)
                    return false;
                mpv_vulkan_image image {};
                image.image = native.object;
                image.format = m_vkFormat;
                // Only what mpv is actually asked to do with it. Claiming usage
                // the image was not created with is how libplacebo ends up
                // advertising a capability the driver will refuse.
                image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
                image.w = size.width();
                image.h = size.height();
                image.layout = native.layout;

                mpv_render_param params[] = {
                    { MPV_RENDER_PARAM_VULKAN_IMAGE, &image },
                    { MPV_RENDER_PARAM_INVALID, nullptr },
                };
                mpv_render_context_render(ctx, params);
                // mpv leaves it ready to be sampled; Qt tracks layouts itself
                // and would otherwise transition from the one it last knew.
                target->setNativeLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                return true;
            }
#endif
            const GLuint texture = static_cast<GLuint>(target->nativeTexture().object);
            if (!texture || !ensureGlFramebuffer(texture, size))
                return false;

            mpv_opengl_fbo mpfbo {};
            mpfbo.fbo = static_cast<int>(m_glFbo);
            mpfbo.w = size.width();
            mpfbo.h = size.height();
            mpfbo.internal_format = 0;
            // A framebuffer object has OpenGL's bottom-left origin, and the
            // scene graph samples this texture with the RHI's top-left one.
            // The framebuffer item used to reconcile that itself; here mpv is
            // asked to write the image the way it will be read.
            int flipY = 1;

            mpv_render_param params[] = {
                { MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo },
                { MPV_RENDER_PARAM_FLIP_Y, &flipY },
                { MPV_RENDER_PARAM_INVALID, nullptr },
            };
            mpv_render_context_render(ctx, params);
            return true;
        }

        bool ensureGlFramebuffer(GLuint texture, const QSize& size)
        {
            if (m_glFbo && m_glFboTexture == texture && m_glFboSize == size)
                return true;

            QOpenGLContext *gl = QOpenGLContext::currentContext();
            if (!gl)
                return false;
            destroyGlFramebuffer();

            QOpenGLFunctions *functions = gl->functions();
            functions->glGenFramebuffers(1, &m_glFbo);
            if (!m_glFbo)
                return false;
            GLint previous = 0;
            functions->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
            functions->glBindFramebuffer(GL_FRAMEBUFFER, m_glFbo);
            functions->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
            const GLenum status = functions->glCheckFramebufferStatus(GL_FRAMEBUFFER);
            functions->glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
            if (status != GL_FRAMEBUFFER_COMPLETE) {
                qWarning() << "player: incomplete framebuffer over the scene graph texture" << status;
                destroyGlFramebuffer();
                return false;
            }
            m_glFboTexture = texture;
            m_glFboSize = size;
            return true;
        }

        void destroyGlFramebuffer()
        {
            if (!m_glFbo)
                return;
            if (QOpenGLContext *gl = QOpenGLContext::currentContext())
                gl->functions()->glDeleteFramebuffers(1, &m_glFbo);
            m_glFbo = 0;
            m_glFboTexture = 0;
            m_glFboSize = QSize();
        }

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
            std::vector<mpv_render_param> params;
            mpv_opengl_init_params glInit {};
            glInit.get_proc_address = &getProcAddressGl;
#if defined(JELLYFIN_MPV_ITEM_VULKAN)
            mpv_vulkan_init_params vkInit {};
            m_vulkan = false;
#endif
            QRhi *rhi = this->rhi();
            if (!rhi) {
                qCritical() << "MpvVideoItem: no RHI to render through";
                return;
            }

#if defined(JELLYFIN_MPV_ITEM_VULKAN)
            if (rhi->backend() == QRhi::Vulkan) {
                const auto *native = static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles());
                if (!native || !native->inst || !native->physDev || !native->dev) {
                    qCritical() << "MpvVideoItem: the Vulkan device is not available";
                    return;
                }
                if (native->gfxQueueIdx != 0) {
                    // mpv takes the family's first queue and orders its frame
                    // against the caller's submissions on it. A different index
                    // would be a different queue and no ordering at all.
                    qCritical() << "MpvVideoItem: Qt is on queue index" << native->gfxQueueIdx
                                << "which mpv cannot share";
                    return;
                }
                vkInit.instance = native->inst->vkInstance();
                vkInit.get_instance_proc_addr
                    = reinterpret_cast<void *>(native->inst->getInstanceProcAddr("vkGetInstanceProcAddr"));
                vkInit.physical_device = native->physDev;
                vkInit.device = native->dev;
                vkInit.queue_family_index = native->gfxQueueFamilyIdx;
                m_vulkan = true;
                m_vkFormat = vulkanFormat(colorTexture());
                params.push_back({ MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_VULKAN) });
                params.push_back({ MPV_RENDER_PARAM_VULKAN_INIT_PARAMS, &vkInit });
            }
#endif
            if (params.empty()) {
                if (rhi->backend() != QRhi::OpenGLES2) {
                    qCritical() << "MpvVideoItem: no mpv render backend for this graphics API";
                    return;
                }
                if (auto *gl = QOpenGLContext::currentContext()) {
                    auto *functions = gl->functions();
                    qInfo() << "player: OpenGL context" << gl->format()
                            << "vendor=" << reinterpret_cast<const char *>(functions->glGetString(GL_VENDOR))
                            << "renderer=" << reinterpret_cast<const char *>(functions->glGetString(GL_RENDERER))
                            << "version=" << reinterpret_cast<const char *>(functions->glGetString(GL_VERSION));
                }
                params.push_back({ MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL) });
                params.push_back({ MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInit });
            }

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
            // control_cb (null here) and returns VO_NOTIMPL immediately -- stats
            // simply don't show GPU pass timings, and direct rendering / mpv
            // screenshots are disabled, neither of which we use.
            static constexpr const char *backends[] = { "gpu-next", "gpu" };

            mpv_render_context *newCtx = nullptr;
            int err = MPV_ERROR_UNSUPPORTED;
            for (const char *backend : backends) {
                std::vector<mpv_render_param> attempt = params;
                attempt.push_back({ MPV_RENDER_PARAM_BACKEND, const_cast<char *>(backend) });
                attempt.push_back({ MPV_RENDER_PARAM_INVALID, nullptr });

                newCtx = nullptr;
                err = mpv_render_context_create(&newCtx, next, attempt.data());
                if (err >= 0) {
                    qInfo() << "player: render backend" << backend << "on" << (m_vulkan ? "Vulkan" : "OpenGL");
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
            mpv_render_context_set_update_callback(newCtx, &MpvRhiRenderer::onMpvUpdate, static_cast<void *>(m_item));
            m_item->m_renderCtxAtomic.store(newCtx);
        }

#if defined(JELLYFIN_MPV_ITEM_VULKAN)
        // Qt does not say which VkFormat it gave the texture, so this mirrors
        // the mapping its Vulkan backend uses for the formats the item offers.
        static int vulkanFormat(QRhiTexture *texture)
        {
            if (!texture)
                return VK_FORMAT_R8G8B8A8_UNORM;
            switch (texture->format()) {
            case QRhiTexture::RGBA16F:
                return VK_FORMAT_R16G16B16A16_SFLOAT;
            case QRhiTexture::RGBA32F:
                return VK_FORMAT_R32G32B32A32_SFLOAT;
            case QRhiTexture::RGB10A2:
                return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
            default:
                break;
            }
            return texture->flags().testFlag(QRhiTexture::sRGB) ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
        }
#endif

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
        bool m_vulkan = false;
        int m_vkFormat = 0;
        GLuint m_glFbo = 0;
        GLuint m_glFboTexture = 0;
        QSize m_glFboSize;
        QPointer<QObject> m_releaseWaiter;
        std::shared_ptr<std::atomic_bool> m_releaseCompleted;
        std::shared_ptr<std::atomic_bool> m_attachCompleted;
    };

#endif // JELLYFIN_MPV_ITEM_RHI

} // namespace

MpvVideoItem *MpvVideoItem::s_instance = nullptr;

MpvVideoItem::MpvVideoItem(QQuickItem *parent)
    : JELLYFIN_MPV_ITEM_BASE(parent)
{
#if defined(JELLYFIN_MPV_ITEM_RHI)
    // A floating-point target is what an HDR swapchain can be handed, and it
    // costs little when the swapchain is SDR: the extra precision is discarded
    // once at the end rather than at every step before it.
    setColorBufferFormat(TextureFormat::RGBA16F);
    setAlphaBlending(false);
#endif
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

#if defined(JELLYFIN_MPV_ITEM_RHI)
QQuickRhiItemRenderer *MpvVideoItem::createRenderer()
{
    return new MpvRhiRenderer(this);
}
#else
QQuickFramebufferObject::Renderer *MpvVideoItem::createRenderer() const
{
    return new MpvFboRenderer(const_cast<MpvVideoItem *>(this));
}
#endif

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
