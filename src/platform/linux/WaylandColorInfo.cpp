#include "platform/linux/WaylandColorInfo.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <QtDebug>

#include <qpa/qplatformnativeinterface.h>

#include <unistd.h>

#include <wayland-client-core.h>

#include "protocol/color-management-v1-client-protocol.h"

namespace JellyfinNative {

namespace {

    struct Reader {
        wp_color_manager_v1 *manager = nullptr;
        WaylandColorInfo info;
        bool ready = false;
        bool failed = false;
    };

    void registryGlobal(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
    {
        auto *reader = static_cast<Reader *>(data);
        if (qstrcmp(interface, wp_color_manager_v1_interface.name) != 0)
            return;
        reader->manager = static_cast<wp_color_manager_v1 *>(
            wl_registry_bind(registry, name, &wp_color_manager_v1_interface, qMin(version, 1u)));
    }

    void registryGlobalRemove(void *, wl_registry *, uint32_t) { }

    const wl_registry_listener kRegistryListener = { registryGlobal, registryGlobalRemove };

    void descriptionReady(void *data, wp_image_description_v1 *, uint32_t)
    {
        static_cast<Reader *>(data)->ready = true;
    }

    void descriptionFailed(void *data, wp_image_description_v1 *, uint32_t, const char *message)
    {
        auto *reader = static_cast<Reader *>(data);
        reader->failed = true;
        qInfo() << "display: the compositor could not describe the output:" << message;
    }

    const wp_image_description_v1_listener kDescriptionListener = { descriptionFailed, descriptionReady };

    void infoLuminances(
        void *data, wp_image_description_info_v1 *, uint32_t minLum, uint32_t maxLum, uint32_t referenceLum)
    {
        auto *reader = static_cast<Reader *>(data);
        // The minimum arrives scaled so that a hundredth of a nit survives an
        // integer; the other two are plain nits.
        reader->info.minLuminanceNits = float(minLum) / 10000.0f;
        reader->info.maxLuminanceNits = float(maxLum);
        reader->info.referenceLuminanceNits = float(referenceLum);
        reader->info.valid = maxLum > 0;
    }

    // Everything else the compositor volunteers about the image description.
    // Ignored deliberately: primaries and the transfer function are Spool's to
    // choose through the swapchain format, and only the luminances are a fact
    // about the display that cannot be found any other way.
    void infoDone(void *, wp_image_description_info_v1 *) { }
    void infoIccFile(void *, wp_image_description_info_v1 *, int32_t fd, uint32_t)
    {
        if (fd >= 0)
            close(fd);
    }
    void infoPrimaries(
        void *, wp_image_description_info_v1 *, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t)
    {
    }
    void infoPrimariesNamed(void *, wp_image_description_info_v1 *, uint32_t) { }
    void infoTfPower(void *, wp_image_description_info_v1 *, uint32_t) { }
    void infoTfNamed(void *, wp_image_description_info_v1 *, uint32_t) { }
    void infoTargetPrimaries(
        void *, wp_image_description_info_v1 *, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t)
    {
    }
    void infoTargetLuminance(void *, wp_image_description_info_v1 *, uint32_t, uint32_t) { }
    void infoTargetMaxCll(void *, wp_image_description_info_v1 *, uint32_t) { }
    void infoTargetMaxFall(void *, wp_image_description_info_v1 *, uint32_t) { }

    const wp_image_description_info_v1_listener kInfoListener = {
        infoDone,
        infoIccFile,
        infoPrimaries,
        infoPrimariesNamed,
        infoTfPower,
        infoTfNamed,
        infoLuminances,
        infoTargetPrimaries,
        infoTargetLuminance,
        infoTargetMaxCll,
        infoTargetMaxFall,
    };

    template <typename T> T *onQueue(T *proxy, wl_event_queue *queue)
    {
        auto *wrapper = static_cast<T *>(wl_proxy_create_wrapper(proxy));
        if (wrapper)
            wl_proxy_set_queue(reinterpret_cast<wl_proxy *>(wrapper), queue);
        return wrapper;
    }

} // namespace

WaylandColorInfo waylandColorInfo(QWindow *window)
{
    WaylandColorInfo result;
    if (!window || !window->screen())
        return result;
    QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();
    if (!native)
        return result;

    auto *display = static_cast<wl_display *>(native->nativeResourceForIntegration("wl_display"));
    auto *output = static_cast<wl_output *>(native->nativeResourceForScreen("output", window->screen()));
    if (!display || !output)
        return result;

    // A queue of our own, so dispatching to get an answer cannot swallow the
    // events Qt is waiting for on the same connection.
    wl_event_queue *queue = wl_display_create_queue(display);
    if (!queue)
        return result;

    Reader reader;
    wl_registry *registry = nullptr;
    wp_color_management_output_v1 *colorOutput = nullptr;
    wp_image_description_v1 *description = nullptr;
    wp_image_description_info_v1 *information = nullptr;
    wl_display *displayWrapper = onQueue(display, queue);
    wl_output *outputWrapper = onQueue(output, queue);

    if (displayWrapper)
        registry = wl_display_get_registry(displayWrapper);
    if (registry) {
        wl_registry_add_listener(registry, &kRegistryListener, &reader);
        wl_display_roundtrip_queue(display, queue);
    }

    if (reader.manager && outputWrapper) {
        colorOutput = wp_color_manager_v1_get_output(reader.manager, outputWrapper);
        if (colorOutput) {
            description = wp_color_management_output_v1_get_image_description(colorOutput);
            if (description) {
                wp_image_description_v1_add_listener(description, &kDescriptionListener, &reader);
                wl_display_roundtrip_queue(display, queue);
            }
        }
    }

    if (reader.ready && description) {
        information = wp_image_description_v1_get_information(description);
        if (information) {
            wp_image_description_info_v1_add_listener(information, &kInfoListener, &reader);
            wl_display_roundtrip_queue(display, queue);
        }
    }

    if (information)
        wp_image_description_info_v1_destroy(information);
    if (description)
        wp_image_description_v1_destroy(description);
    if (colorOutput)
        wp_color_management_output_v1_destroy(colorOutput);
    if (reader.manager)
        wp_color_manager_v1_destroy(reader.manager);
    if (registry)
        wl_registry_destroy(registry);
    if (outputWrapper)
        wl_proxy_wrapper_destroy(outputWrapper);
    if (displayWrapper)
        wl_proxy_wrapper_destroy(displayWrapper);
    wl_event_queue_destroy(queue);

    return reader.info;
}

} // namespace JellyfinNative
