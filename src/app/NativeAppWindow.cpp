#include "NativeAppWindow.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QExposeEvent>
#include <QGuiApplication>
#include <QResizeEvent>
#include <QThread>

#include <qpa/qplatformnativeinterface.h>

#include <cstring>

namespace JellyfinNative {

NativeAppWindow::NativeAppWindow(const QString &appId, QWindow *parent)
    : QQuickView(parent)
    , m_appId(appId)
{
    setColor(Qt::transparent);
    setResizeMode(QQuickView::SizeRootObjectToView);
    setFlags(Qt::FramelessWindowHint | Qt::Window);
    setTitle(QStringLiteral("Jellyfin Native"));
    resize(1920, 1080);
}

NativeAppWindow::~NativeAppWindow()
{
    if (m_exported)
        wl_webos_exported_destroy(m_exported);
    if (m_webosShellSurface)
        wl_webos_shell_surface_destroy(m_webosShellSurface);
    if (m_webosShell)
        wl_webos_shell_destroy(m_webosShell);
    if (m_webosForeign)
        wl_webos_foreign_destroy(m_webosForeign);
    if (m_registry)
        wl_registry_destroy(m_registry);
}

bool NativeAppWindow::prepareForUiSurface()
{
    showFullScreen();
    requestActivate();

    const int timeoutMs = 5000;
    const int stepMs = 25;
    for (int waited = 0; waited < timeoutMs; waited += stepMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, stepMs);
        if (ensureShellSurface())
            break;
        QThread::msleep(stepMs);
    }

    return m_webosShellSurface && m_surface;
}

bool NativeAppWindow::prepareForPlaybackSurface()
{
    if (!prepareForUiSurface())
        return false;

    const int timeoutMs = 5000;
    const int stepMs = 25;
    for (int waited = 0; waited < timeoutMs; waited += stepMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, stepMs);
        if (ensureVideoSurface() && !m_windowId.empty())
            break;
        QThread::msleep(stepMs);
    }

    if (m_windowId.empty())
        return false;

    setenv("STARFISH_WINDOW_ID", m_windowId.c_str(), 1);
    setenv("STARFISH_WINDOW_WIDTH", QByteArray::number(width()).constData(), 1);
    setenv("STARFISH_WINDOW_HEIGHT", QByteArray::number(height()).constData(), 1);
    return true;
}

QString NativeAppWindow::windowId() const
{
    return QString::fromStdString(m_windowId);
}

void NativeAppWindow::exposeEvent(QExposeEvent *event)
{
    QQuickView::exposeEvent(event);
    if (isExposed())
        ensureShellSurface();
}

void NativeAppWindow::resizeEvent(QResizeEvent *event)
{
    QQuickView::resizeEvent(event);
    if (!m_windowId.empty()) {
        setenv("STARFISH_WINDOW_WIDTH", QByteArray::number(width()).constData(), 1);
        setenv("STARFISH_WINDOW_HEIGHT", QByteArray::number(height()).constData(), 1);
        updateCropRegion();
    }
}

bool NativeAppWindow::ensureVideoSurface()
{
    if (m_exported || !handle())
        return true;

    if (!ensureShellSurface())
        return false;

    if (!m_webosForeign || !m_surface)
        return false;

    m_exported =
        wl_webos_foreign_export_element(m_webosForeign, m_surface, WL_WEBOS_FOREIGN_WEBOS_EXPORTED_TYPE_VIDEO_OBJECT);
    if (!m_exported)
        return false;

    wl_webos_exported_add_listener(m_exported, &s_exportedListener, this);
    wl_surface_commit(m_surface);
    wl_display_roundtrip(m_display);
    updateCropRegion();
    return true;
}

bool NativeAppWindow::ensureShellSurface()
{
    if (m_webosShellSurface && m_surface)
        return true;

    if (!handle())
        return false;

    auto *native = QGuiApplication::platformNativeInterface();
    if (!native)
        return false;

    m_display = static_cast<wl_display *>(native->nativeResourceForIntegration(QByteArrayLiteral("display")));
    m_surface = static_cast<wl_surface *>(native->nativeResourceForWindow(QByteArrayLiteral("surface"), this));
    if (!m_display || !m_surface)
        return false;

    if (!bindGlobals())
        return false;

    if (!m_webosShell || !m_surface)
        return false;

    m_webosShellSurface = wl_webos_shell_get_shell_surface(m_webosShell, m_surface);
    if (!m_webosShellSurface)
        return false;

    wl_webos_shell_surface_set_property(m_webosShellSurface, "_WEBOS_ACCESS_POLICY_KEYS_GUIDE", "true");
    wl_webos_shell_surface_set_property(m_webosShellSurface, "_WEBOS_ACCESS_POLICY_KEYS_BACK", "true");
    wl_webos_shell_surface_set_property(m_webosShellSurface, "appId", m_appId.toUtf8().constData());
    wl_webos_shell_surface_set_property(
        m_webosShellSurface, "displayAffinity", getenv("DISPLAY_ID") ? getenv("DISPLAY_ID") : "0");
    wl_webos_shell_surface_set_state(m_webosShellSurface, WL_WEBOS_SHELL_SURFACE_STATE_FULLSCREEN);
    wl_surface_commit(m_surface);
    wl_display_roundtrip(m_display);
    return true;
}

bool NativeAppWindow::bindGlobals()
{
    if (m_webosForeign && m_compositor)
        return true;

    m_registry = wl_display_get_registry(m_display);
    if (!m_registry)
        return false;

    wl_registry_add_listener(m_registry, &s_registryListener, this);
    wl_display_roundtrip(m_display);
    wl_display_roundtrip(m_display);

    return m_compositor && m_subcompositor && m_webosForeign && m_webosShell;
}

void NativeAppWindow::updateCropRegion()
{
    if (!m_exported || !m_compositor)
        return;

    wl_region *orig = wl_compositor_create_region(m_compositor);
    wl_region *src = wl_compositor_create_region(m_compositor);
    wl_region *dst = wl_compositor_create_region(m_compositor);
    if (!orig || !src || !dst)
        return;

    wl_region_add(orig, 0, 0, width(), height());
    wl_region_add(src, 0, 0, width(), height());
    wl_region_add(dst, 0, 0, width(), height());
    wl_webos_exported_set_crop_region(m_exported, orig, src, dst);
    wl_region_destroy(dst);
    wl_region_destroy(src);
    wl_region_destroy(orig);
    wl_display_flush(m_display);
}

void NativeAppWindow::registryGlobal(void *data, wl_registry *registry, uint32_t name,
                                     const char *interface, uint32_t version)
{
    auto *self = static_cast<NativeAppWindow *>(data);
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        const uint32_t bindVersion = version < 3 ? version : 3;
        self->m_compositor =
            static_cast<wl_compositor *>(wl_registry_bind(registry, name, &wl_compositor_interface, bindVersion));
    } else if (strcmp(interface, wl_subcompositor_interface.name) == 0) {
        self->m_subcompositor =
            static_cast<wl_subcompositor *>(wl_registry_bind(registry, name, &wl_subcompositor_interface, 1));
    } else if (strcmp(interface, wl_webos_shell_interface.name) == 0) {
        const uint32_t bindVersion = version < 2 ? version : 2;
        self->m_webosShell =
            static_cast<wl_webos_shell *>(wl_registry_bind(registry, name, &wl_webos_shell_interface, bindVersion));
    } else if (strcmp(interface, wl_webos_foreign_interface.name) == 0) {
        const uint32_t bindVersion = version < 2 ? version : 2;
        self->m_webosForeign =
            static_cast<wl_webos_foreign *>(wl_registry_bind(registry, name, &wl_webos_foreign_interface, bindVersion));
    }
}

void NativeAppWindow::registryRemove(void *, wl_registry *, uint32_t)
{
}

void NativeAppWindow::exportedWindowIdAssigned(void *data, wl_webos_exported *, const char *window_id, uint32_t)
{
    auto *self = static_cast<NativeAppWindow *>(data);
    self->m_windowId = window_id ? window_id : "";
}

const wl_registry_listener NativeAppWindow::s_registryListener = {
    &NativeAppWindow::registryGlobal,
    &NativeAppWindow::registryRemove,
};

const wl_webos_exported_listener NativeAppWindow::s_exportedListener = {
    &NativeAppWindow::exportedWindowIdAssigned,
};

} // namespace JellyfinNative
