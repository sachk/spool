#include "player/RenderTargetProfile.h"

#include "player/MpvOptionProfile.h"

#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSettings>
#include <QtGlobal>

#if defined(JELLYFIN_MPV_ITEM_RHI)
#include <rhi/qrhi.h>
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID) && QT_CONFIG(vulkan)
#include <QGuiApplication>
#include <rhi/qrhi_platform.h>
#endif
#endif

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include "platform/linux/WaylandColorInfo.h"
#endif

#include <algorithm>
#include <cmath>

namespace JellyfinNative {

namespace {

    // mpv takes both of these in nits and refuses anything outside this range,
    // so a display that reports something absurd is ignored rather than
    // rejected at option-apply time, where the failure is a dead player.
    constexpr float kMinNits = 10.0f;
    constexpr float kMaxNits = 10000.0f;

    // scRGB puts 1.0 at 80 nits by definition, whatever the operating system
    // has chosen for its own SDR white.
    constexpr float kScrgbUnityNits = 80.0f;

    bool sameNits(float a, float b)
    {
        return std::fabs(a - b) < 0.01f;
    }

    QByteArray nitsOption(float nits)
    {
        if (!(nits >= kMinNits))
            return {};
        return QByteArray::number(qRound(std::min(nits, kMaxNits)));
    }

} // namespace

bool RenderTargetProfile::operator==(const RenderTargetProfile& other) const
{
    return format == other.format && sameNits(sdrWhiteNits, other.sdrWhiteNits)
        && sameNits(minLuminanceNits, other.minLuminanceNits) && sameNits(maxLuminanceNits, other.maxLuminanceNits);
}

HdrOutputPreference RenderTargetPolicy::preferenceFromName(const QString& name)
{
    const QString normalized = name.trimmed().toLower();
    if (normalized == QStringLiteral("never") || normalized == QStringLiteral("off"))
        return HdrOutputPreference::Never;
    if (normalized == QStringLiteral("always") || normalized == QStringLiteral("on"))
        return HdrOutputPreference::Always;
    return HdrOutputPreference::Auto;
}

namespace {

    // Not the application database: that opens well after the window, and the
    // swapchain format is read once, at the window's first expose. QSettings
    // is synchronous and available before anything else has started.
    constexpr auto kStartupPreferenceKey = "render/hdrOutput";

    // The application's own store, by the organisation and application names
    // main() sets before this is ever reached -- not a second file of its own.
    QSettings startupStore()
    {
        return QSettings();
    }

} // namespace

QByteArray RenderTargetPolicy::startupSwapChainRequest(bool automaticHdr)
{
    const auto preference = preferenceFromName(startupStore().value(QLatin1String(kStartupPreferenceKey)).toString());
    if (preference == HdrOutputPreference::Never || (preference == HdrOutputPreference::Auto && !automaticHdr))
        return {};
    // The shell converts SDR UI to linear BT.709 at the reported reference
    // white. Do not request PQ/P3 until their composition paths exist.
    return QByteArrayLiteral("scrgb");
}

void RenderTargetPolicy::rememberPreference(HdrOutputPreference preference)
{
    QSettings store = startupStore();
    store.setValue(QLatin1String(kStartupPreferenceKey), QString::fromLatin1(preferenceName(preference)));
}

QByteArray RenderTargetPolicy::preferenceName(HdrOutputPreference preference)
{
    switch (preference) {
    case HdrOutputPreference::Never:
        return QByteArrayLiteral("never");
    case HdrOutputPreference::Always:
        return QByteArrayLiteral("always");
    case HdrOutputPreference::Auto:
        break;
    }
    return QByteArrayLiteral("auto");
}

RenderTargetProfile RenderTargetPolicy::resolve(
    const DisplayOutputCapabilities& display, HdrOutputPreference preference, const RenderTargetOverrides& overrides)
{
    const float white = overrides.sdrWhiteNits >= kMinNits ? overrides.sdrWhiteNits : display.sdrWhiteNits;

    RenderTargetProfile profile;
    profile.sdrWhiteNits = white >= kMinNits ? white : RenderTargetProfile::kDefaultSdrWhiteNits;
    if (preference == HdrOutputPreference::Never)
        return profile;
    // No HDR swapchain means no HDR, whatever the display or the user think.
    // Every OpenGL context lands here, which is why this is asked of the
    // backend rather than of the monitor.
    if (!display.hdrAvailable || display.preferredFormat == RenderTargetProfile::Format::Sdr)
        return profile;

    profile.format = display.preferredFormat;
    profile.minLuminanceNits = std::max(0.0f, display.minLuminanceNits);
    // A number the viewer gave wins. Otherwise only a measured one is passed
    // on: where Qt fabricates the luminance, saying nothing leaves mpv to its
    // own detection, which is a better answer than a fixed 1000 nits pretending
    // to describe the panel.
    if (overrides.maxLuminanceNits >= kMinNits)
        profile.maxLuminanceNits = overrides.maxLuminanceNits;
    else if (display.luminanceMeasured)
        profile.maxLuminanceNits = std::max(0.0f, display.maxLuminanceNits);
    return profile;
}

std::vector<MpvOption> RenderTargetPolicy::targetOptions(const RenderTargetProfile& profile)
{
    const bool hdr = profile.isHdr();
    const bool pq = profile.format == RenderTargetProfile::Format::Pq;
    const QByteArray peak = hdr ? nitsOption(profile.maxLuminanceNits) : QByteArray();
    // Describe the embedding target, not a tone-mapping recipe. In particular,
    // reset luminance overrides when returning to SDR or an unknown display.
    return {
        { QByteArrayLiteral("target-prim"), pq ? QByteArrayLiteral("bt.2020") : QByteArrayLiteral("bt.709") },
        { QByteArrayLiteral("target-trc"),
            !hdr     ? QByteArrayLiteral("bt.1886")
                : pq ? QByteArrayLiteral("pq")
                     : QByteArrayLiteral("scrgb") },
        { QByteArrayLiteral("target-peak"), peak.isEmpty() ? QByteArrayLiteral("auto") : peak },
        { QByteArrayLiteral("target-contrast"), hdr ? QByteArrayLiteral("inf") : QByteArrayLiteral("auto") },
        { QByteArrayLiteral("hdr-reference-white"),
            hdr ? nitsOption(profile.sdrWhiteNits) : QByteArrayLiteral("auto") },
    };
}

DisplayOutputCapabilities RenderTargetPolicy::probe(QQuickWindow *window)
{
    DisplayOutputCapabilities display;
#if defined(JELLYFIN_MPV_ITEM_RHI)
    if (!window)
        return display;
    QSGRendererInterface *renderer = window->rendererInterface();
    if (!renderer)
        return display;
    auto *swapchain
        = static_cast<QRhiSwapChain *>(renderer->getResource(window, QSGRendererInterface::RhiSwapchainResource));
    if (!swapchain)
        return display;
    display.surfaceReady = true;

    // Ask the backend, not the monitor. OpenGL answers no to both of these
    // however bright the display is, which is the whole reason the question is
    // put this way round.
    if (swapchain->isFormatSupported(QRhiSwapChain::HDRExtendedSrgbLinear)) {
        display.supportedFormat = RenderTargetProfile::Format::ExtendedSrgbLinear;
    } else if (swapchain->isFormatSupported(QRhiSwapChain::HDR10)) {
        display.supportedFormat = RenderTargetProfile::Format::Pq;
    }

    // And then ask what it settled on. Qt chooses a window's swapchain format
    // once, when the window is created, so this is a fact about the window in
    // front of us rather than a preference -- support without a window that
    // asked for it still presents SDR.
    switch (swapchain->format()) {
    case QRhiSwapChain::HDRExtendedSrgbLinear:
    case QRhiSwapChain::HDRExtendedDisplayP3Linear:
        display.preferredFormat = RenderTargetProfile::Format::ExtendedSrgbLinear;
        break;
    case QRhiSwapChain::HDR10:
        display.preferredFormat = RenderTargetProfile::Format::Pq;
        break;
    case QRhiSwapChain::SDR:
        break;
    }
    if (display.preferredFormat == RenderTargetProfile::Format::Sdr)
        return display;
    display.hdrAvailable = true;
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID) && QT_CONFIG(vulkan)
    if (renderer->graphicsApi() == QSGRendererInterface::Vulkan
        && QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        auto *rhi = static_cast<QRhi *>(renderer->getResource(window, QSGRendererInterface::RhiResource));
        const auto *handles = static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles());
        const auto getFormats = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
            handles->inst->getInstanceProcAddr("vkGetPhysicalDeviceSurfaceFormatsKHR"));
        const VkSurfaceKHR surface = QVulkanInstance::surfaceForWindow(window);
        uint32_t count = 0;
        if (!getFormats || getFormats(handles->physDev, surface, &count, nullptr) != VK_SUCCESS || !count) {
            display.hdrAvailable = false;
            return display;
        }
        std::vector<VkSurfaceFormatKHR> formats(count);
        if (getFormats(handles->physDev, surface, &count, formats.data()) != VK_SUCCESS) {
            display.hdrAvailable = false;
            return display;
        }
        const VkFormat format = display.preferredFormat == RenderTargetProfile::Format::Pq
            ? VK_FORMAT_A2B10G10R10_UNORM_PACK32
            : VK_FORMAT_R16G16B16A16_SFLOAT;
        // Match QVkSwapChain::chooseFormats: PASS_THROUGH wins for this format.
        display.needsWaylandDescription
            = std::any_of(formats.begin(), formats.begin() + count, [format](const VkSurfaceFormatKHR& candidate) {
                  return candidate.format == format && candidate.colorSpace == VK_COLOR_SPACE_PASS_THROUGH_EXT;
              });
    }
#endif

    // Qt only asks the system on Windows -- QD3D11SwapChain, QD3D12SwapChain
    // and QVkSwapChain under Q_OS_WIN, the last through DXGI. Everywhere else,
    // including Metal, this returns a fixed 1000 nits and 200 nits SDR white
    // whatever is plugged in, so it is a starting point and not an answer.
    const QRhiSwapChainHdrInfo info = swapchain->hdrInfo();
    display.sdrWhiteNits = info.sdrWhiteLevel > 0.0f ? info.sdrWhiteLevel : RenderTargetProfile::kDefaultSdrWhiteNits;
    if (info.limitsType == QRhiSwapChainHdrInfo::LuminanceInNits) {
        display.minLuminanceNits = info.limits.luminanceInNits.minLuminance;
        display.maxLuminanceNits = info.limits.luminanceInNits.maxLuminance;
    } else {
        // The other report is relative, with 1.0 at the display's SDR white,
        // so it only becomes a luminance once that white is known.
        display.maxLuminanceNits = info.limits.colorComponentValue.maxColorComponentValue * display.sdrWhiteNits;
    }
#if defined(Q_OS_WIN)
    display.luminanceMeasured = true;
#endif
#else
    Q_UNUSED(window);
#endif
    return display;
}

void RenderTargetPolicy::updateDisplayLuminance(DisplayOutputCapabilities& display, QQuickWindow *window)
{
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID) && !defined(JELLYFIN_NATIVE_WEBOS)
    if (!display.hdrAvailable)
        return;
    // The compositor does know, and on Wayland it will say. This is the number
    // somebody set in their display settings, which nothing reads out of EDID
    // and Qt never asks for, so it replaces the guess above rather than
    // supplementing it.
    if (const WaylandColorInfo wayland = waylandColorInfo(window); wayland.valid) {
        display.maxLuminanceNits = wayland.maxLuminanceNits;
        display.minLuminanceNits = wayland.minLuminanceNits;
        if (wayland.referenceLuminanceNits > 0.0f)
            display.sdrWhiteNits = wayland.referenceLuminanceNits;
        display.luminanceMeasured = true;
    }
#else
    Q_UNUSED(display);
    Q_UNUSED(window);
#endif
}

float RenderTargetPolicy::osdBrightnessScale(const RenderTargetProfile& profile)
{
    // PQ carries absolute luminance, and libplacebo already draws the OSD at
    // the reference white it was given. scRGB does not: its 1.0 is 80 nits, so
    // an unscaled overlay arrives dimmer than the shell around it by exactly
    // the ratio the operating system chose for SDR white.
    if (profile.format != RenderTargetProfile::Format::ExtendedSrgbLinear)
        return 1.0f;
    const float white
        = profile.sdrWhiteNits >= kMinNits ? profile.sdrWhiteNits : RenderTargetProfile::kDefaultSdrWhiteNits;
    return white / kScrgbUnityNits;
}

} // namespace JellyfinNative
