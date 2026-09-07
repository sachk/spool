#pragma once

#include <QByteArray>
#include <QString>

#include <vector>

class QQuickWindow;

namespace JellyfinNative {

struct MpvOption;

// What the window is presenting into, and what mpv has to be told about it.
//
// The embedded player renders into a target Spool owns, not into a display
// mpv can interrogate. So the transfer function, the primaries and the
// luminance range are decided here and pushed down; reading them back out of
// mpv is how the current OpenGL path ends up reporting SDR for everything.
//
// Three things decide it and they are kept apart on purpose: what the graphics
// backend can present, what the operating system reports about the display,
// and what the user asked for. None of them alone is an answer -- a monitor in
// HDR mode says nothing about whether the backend has an HDR swapchain to
// reach it with.
//
// What the source carries is deliberately not among them. The swapchain is
// chosen when the window is created and Qt gives no way to change it after,
// so a target that followed the file would be a target that cannot exist.
// Source characteristics decide tone mapping and how bright the overlay is
// drawn, which is a different question asked later.
struct RenderTargetProfile {
    enum class Format {
        // 8-bit sRGB. Every backend has this, and it is what the OpenGL
        // framebuffer path can offer.
        Sdr,
        // scRGB: linear, FP16, BT.709 primaries, 1.0 at the display's SDR
        // white. Values run past 1.0, which is where the highlights live.
        ExtendedSrgbLinear,
        // HDR10: PQ-encoded, BT.2020 primaries, absolute luminance.
        Pq,
    };

    // BT.2408's reference white. The value a diffuse white in SDR content
    // should reach on an HDR display, and what OSD and subtitles are drawn
    // against when the OS reports nothing better.
    static constexpr float kDefaultSdrWhiteNits = 203.0f;

    Format format = Format::Sdr;
    float sdrWhiteNits = kDefaultSdrWhiteNits;
    float minLuminanceNits = 0.0f;
    // Zero means the display did not say. mpv is then left to its own
    // detection rather than told a number nobody measured.
    float maxLuminanceNits = 0.0f;

    bool isHdr() const
    {
        return format != Format::Sdr;
    }
    bool operator==(const RenderTargetProfile& other) const;
    bool operator!=(const RenderTargetProfile& other) const
    {
        return !(*this == other);
    }
};

// What the graphics backend and the operating system between them say the
// window can present. Filled from the live swapchain, never assumed.
struct DisplayOutputCapabilities {
    // False whenever the query failed or the backend has no HDR swapchain at
    // all, which is the case for every OpenGL context.
    bool hdrAvailable = false;
    RenderTargetProfile::Format preferredFormat = RenderTargetProfile::Format::Sdr;
    float sdrWhiteNits = RenderTargetProfile::kDefaultSdrWhiteNits;
    float minLuminanceNits = 0.0f;
    float maxLuminanceNits = 0.0f;
    // Whether those numbers came from the display or from Qt's own fixed
    // fallback. Only D3D11, D3D12 and Vulkan-on-Windows ask the system; every
    // other backend returns 1000 nits and 200 nits SDR white regardless of
    // what is plugged in. Handing a guess to mpv as though it were measured is
    // how a display ends up tone mapped to numbers nobody checked.
    bool luminanceMeasured = false;
};

// What the viewer said, which beats both the display and the guess. On Linux
// and macOS this is the only real number available: nothing here reports panel
// luminance, and the desktops do not read it out of EDID either, so somebody
// who wants their own peak respected has to say what it is.
struct RenderTargetOverrides {
    // Nits. Zero leaves whatever the display reported in place.
    float maxLuminanceNits = 0.0f;
    float sdrWhiteNits = 0.0f;
};

// The user's say, which overrides the automatic answer in both directions.
enum class HdrOutputPreference {
    // HDR when the display is in HDR mode and the backend can present it.
    Auto,
    // Never leave SDR, whatever the display says. What someone whose desktop
    // handles HDR badly, or who simply prefers the SDR pipeline, asks for.
    Never,
    // Currently the same as Auto, and kept apart from it so that a display
    // whose report cannot be trusted still has a way to be used.
    Always,
};

class RenderTargetPolicy final {
public:
    static HdrOutputPreference preferenceFromName(const QString& name);
    static QByteArray preferenceName(HdrOutputPreference preference);

    // The target to present into. Returns an SDR profile whenever HDR was not
    // asked for or is not available.
    static RenderTargetProfile resolve(const DisplayOutputCapabilities& display, HdrOutputPreference preference,
        const RenderTargetOverrides& overrides = {});

    // What mpv has to be told so it renders for that target. Empty for SDR:
    // mpv's own defaults are already an sRGB target, and naming them would
    // only take the decision away from a user's own configuration.
    static std::vector<MpvOption> targetOptions(const RenderTargetProfile& profile);

    // How bright diffuse white ends up, relative to SDR. Subtitles and the
    // OSD are drawn in the target's units, so on an HDR target they need
    // scaling or they arrive at whatever the shell's white happens to be.
    static float osdBrightnessScale(const RenderTargetProfile& profile);

    // What the window's live swapchain says it can present, asked of the
    // graphics backend rather than of the monitor. Returns nothing available
    // when there is no swapchain yet, which is every moment before the scene
    // graph has drawn once.
    static DisplayOutputCapabilities probe(QQuickWindow *window);
};

} // namespace JellyfinNative
