#include "player/RenderTargetProfile.h"

#include "player/MpvOptionProfile.h"

#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSettings>
#include <QtGlobal>

#if defined(JELLYFIN_MPV_ITEM_RHI)
#include <rhi/qrhi.h>
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

    QSettings startupStore()
    {
        return QSettings(QStringLiteral("Spool"), QStringLiteral("Spool"));
    }

} // namespace

QByteArray RenderTargetPolicy::startupSwapChainRequest()
{
    // Only an explicit yes turns the swapchain over. Qt Quick applies no colour
    // management of its own -- its materials write sRGB-encoded values straight
    // into whatever buffer they are given -- so an HDR swapchain is also a
    // change to how the whole interface looks, and that is not something to do
    // to somebody who never asked for it. Auto stays SDR until the interface is
    // corrected for the buffer it lands in.
    const QString stored = startupStore().value(QLatin1String(kStartupPreferenceKey)).toString();
    if (stored.isEmpty() || preferenceFromName(stored) != HdrOutputPreference::Always)
        return {};
    // scRGB rather than HDR10: it keeps SDR content at the same values it has
    // now, needs no PQ encode of the interface, and is the format Qt reports
    // support for most widely. Where the display cannot present it Qt falls
    // back to SDR by itself.
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
    std::vector<MpvOption> options;
    if (!profile.isHdr())
        return options;

    if (profile.format == RenderTargetProfile::Format::Pq) {
        options.push_back({ QByteArrayLiteral("target-prim"), QByteArrayLiteral("bt.2020") });
        options.push_back({ QByteArrayLiteral("target-trc"), QByteArrayLiteral("pq") });
    } else {
        // libplacebo names this encoding, so mpv can be told exactly what the
        // swapchain is rather than approximated with linear and a peak.
        options.push_back({ QByteArrayLiteral("target-prim"), QByteArrayLiteral("bt.709") });
        options.push_back({ QByteArrayLiteral("target-trc"), QByteArrayLiteral("scrgb") });
    }

    // Both of these stay unset when the display did not say. A number nobody
    // measured is worse than mpv's own detection.
    if (const QByteArray peak = nitsOption(profile.maxLuminanceNits); !peak.isEmpty())
        options.push_back({ QByteArrayLiteral("target-peak"), peak });
    if (const QByteArray white = nitsOption(profile.sdrWhiteNits); !white.isEmpty())
        options.push_back({ QByteArrayLiteral("hdr-reference-white"), white });
    return options;
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
        // Both are linear with 1.0 at SDR white; they differ in primaries, and
        // the P3 one is only reachable on Apple platforms.
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
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
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
#endif
#else
    Q_UNUSED(window);
#endif
    return display;
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
