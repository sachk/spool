#include "player/RenderTargetProfile.h"

#include "player/MpvOptionProfile.h"

#include <QtGlobal>

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
    const DisplayOutputCapabilities& display, HdrOutputPreference preference, bool sourceIsHdr)
{
    RenderTargetProfile profile;
    profile.sdrWhiteNits
        = display.sdrWhiteNits >= kMinNits ? display.sdrWhiteNits : RenderTargetProfile::kDefaultSdrWhiteNits;
    if (preference == HdrOutputPreference::Never)
        return profile;
    // No HDR swapchain means no HDR, whatever the display or the user think.
    // Every OpenGL context lands here, which is why this is asked of the
    // backend rather than of the monitor.
    if (!display.hdrAvailable || display.preferredFormat == RenderTargetProfile::Format::Sdr)
        return profile;
    // An SDR film on an HDR target gains nothing and costs a tone-mapping
    // round trip, so Auto leaves it alone. Someone whose display sits in HDR
    // permanently, and who would rather everything went through one pipeline,
    // says Always.
    if (preference == HdrOutputPreference::Auto && !sourceIsHdr)
        return profile;

    profile.format = display.preferredFormat;
    profile.minLuminanceNits = std::max(0.0f, display.minLuminanceNits);
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
