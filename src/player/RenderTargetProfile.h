#pragma once

#include <QByteArray>
#include <QString>

#include <vector>

namespace JellyfinNative {

struct MpvOption;

// What the window is presenting into, and what mpv has to be told about it.
//
// The embedded player renders into a target Spool owns, not into a display
// mpv can interrogate. So the transfer function, the primaries and the
// luminance range are decided here and pushed down; reading them back out of
// mpv is how the current OpenGL path ends up reporting SDR for everything.
//
// Four things decide the target and they are kept apart on purpose: what the
// source carries, what the graphics backend can present, what the operating
// system reports about the display, and what the user asked for. Any one of
// them alone is a wrong answer -- HDR metadata on a file says nothing about
// the monitor, and an HDR-capable monitor is not a reason to put an SDR film
// through a PQ pipeline.
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
};

// The user's say, which overrides the automatic answer in both directions.
enum class HdrOutputPreference {
    // HDR when the display is in HDR mode and the source has something to put
    // there. This is the only setting that looks at the source.
    Auto,
    // Never leave SDR, whatever the display says.
    Never,
    // Always present HDR where the display can, including for SDR sources,
    // which is what someone with a display left permanently in HDR wants.
    Always,
};

class RenderTargetPolicy final {
public:
    static HdrOutputPreference preferenceFromName(const QString& name);
    static QByteArray preferenceName(HdrOutputPreference preference);

    // The target to present into. Returns an SDR profile whenever HDR was not
    // asked for, is not available, or would gain nothing.
    static RenderTargetProfile resolve(
        const DisplayOutputCapabilities& display, HdrOutputPreference preference, bool sourceIsHdr);

    // What mpv has to be told so it renders for that target. Empty for SDR:
    // mpv's own defaults are already an sRGB target, and naming them would
    // only take the decision away from a user's own configuration.
    static std::vector<MpvOption> targetOptions(const RenderTargetProfile& profile);

    // How bright diffuse white ends up, relative to SDR. Subtitles and the
    // OSD are drawn in the target's units, so on an HDR target they need
    // scaling or they arrive at whatever the shell's white happens to be.
    static float osdBrightnessScale(const RenderTargetProfile& profile);
};

} // namespace JellyfinNative
