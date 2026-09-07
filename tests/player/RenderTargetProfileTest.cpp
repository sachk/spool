#include "player/RenderTargetProfile.h"

#include "player/MpvOptionProfile.h"

#include "TestMain.h"

#include <QCoreApplication>

#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace JellyfinNative;

namespace {

QByteArray valueFor(const std::vector<MpvOption>& options, const QByteArray& name)
{
    for (const MpvOption& option : options) {
        if (option.name == name)
            return option.value;
    }
    return {};
}

void require(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << message << '\n';
    std::exit(1);
}

DisplayOutputCapabilities hdrDisplay(
    RenderTargetProfile::Format format = RenderTargetProfile::Format::ExtendedSrgbLinear)
{
    DisplayOutputCapabilities display;
    display.hdrAvailable = true;
    display.preferredFormat = format;
    display.sdrWhiteNits = 240.0f;
    display.minLuminanceNits = 0.005f;
    display.maxLuminanceNits = 1000.0f;
    display.luminanceMeasured = true;
    return display;
}

} // namespace

JELLYFIN_TEST_MAIN("render-target-profile")
{
    QCoreApplication app(argc, argv);

    const DisplayOutputCapabilities sdrOnly;
    require(!RenderTargetPolicy::resolve(sdrOnly, HdrOutputPreference::Always).isHdr(),
        "a backend with no HDR swapchain stays SDR however loudly it is asked");
    require(RenderTargetPolicy::targetOptions(RenderTargetPolicy::resolve(sdrOnly, HdrOutputPreference::Auto)).empty(),
        "an SDR target should name no mpv target options at all");

    require(!RenderTargetPolicy::resolve(hdrDisplay(), HdrOutputPreference::Never).isHdr(),
        "Never should hold an HDR display in SDR");
    require(RenderTargetPolicy::resolve(hdrDisplay(), HdrOutputPreference::Auto).isHdr(),
        "Auto should follow a display that says it is in HDR mode");

    const RenderTargetProfile scrgb = RenderTargetPolicy::resolve(hdrDisplay(), HdrOutputPreference::Auto);
    require(scrgb.format == RenderTargetProfile::Format::ExtendedSrgbLinear && scrgb.isHdr(),
        "an HDR source on an scRGB-capable display should present scRGB");
    const auto scrgbOptions = RenderTargetPolicy::targetOptions(scrgb);
    require(valueFor(scrgbOptions, "target-trc") == "scrgb", "scRGB is an encoding libplacebo names itself");
    require(valueFor(scrgbOptions, "target-prim") == "bt.709", "scRGB carries BT.709 primaries");
    require(valueFor(scrgbOptions, "target-peak") == "1000", "the display's peak should reach mpv in nits");
    require(valueFor(scrgbOptions, "hdr-reference-white") == "240",
        "the operating system's SDR white should reach mpv in nits");

    const RenderTargetProfile pq
        = RenderTargetPolicy::resolve(hdrDisplay(RenderTargetProfile::Format::Pq), HdrOutputPreference::Auto);
    const auto pqOptions = RenderTargetPolicy::targetOptions(pq);
    require(valueFor(pqOptions, "target-trc") == "pq" && valueFor(pqOptions, "target-prim") == "bt.2020",
        "an HDR10 target is PQ over BT.2020");

    DisplayOutputCapabilities silent = hdrDisplay();
    silent.maxLuminanceNits = 0.0f;
    silent.sdrWhiteNits = 0.0f;
    const RenderTargetProfile guessed = RenderTargetPolicy::resolve(silent, HdrOutputPreference::Always);
    const auto guessedOptions = RenderTargetPolicy::targetOptions(guessed);
    require(valueFor(guessedOptions, "target-peak").isEmpty(),
        "a display that reports no peak should leave mpv to its own detection");
    require(std::fabs(guessed.sdrWhiteNits - RenderTargetProfile::kDefaultSdrWhiteNits) < 0.01f,
        "an unreported SDR white should fall back to the reference value");

    DisplayOutputCapabilities absurd = hdrDisplay();
    absurd.maxLuminanceNits = 250000.0f;
    require(
        valueFor(RenderTargetPolicy::targetOptions(RenderTargetPolicy::resolve(absurd, HdrOutputPreference::Always)),
            "target-peak")
            == "10000",
        "a peak beyond what mpv accepts should be clamped, not passed through and rejected");

    require(std::fabs(RenderTargetPolicy::osdBrightnessScale(scrgb) - 3.0f) < 0.01f,
        "scRGB overlays need scaling from its 80-nit unity to the display's SDR white");
    require(std::fabs(RenderTargetPolicy::osdBrightnessScale(pq) - 1.0f) < 0.01f,
        "PQ carries absolute luminance, so its overlays are already placed");
    require(std::fabs(RenderTargetPolicy::osdBrightnessScale(RenderTargetProfile()) - 1.0f) < 0.01f,
        "an SDR target needs no overlay scaling");

    require(RenderTargetPolicy::preferenceFromName(QStringLiteral("Always")) == HdrOutputPreference::Always
            && RenderTargetPolicy::preferenceFromName(QStringLiteral("never")) == HdrOutputPreference::Never
            && RenderTargetPolicy::preferenceFromName(QStringLiteral("nonsense")) == HdrOutputPreference::Auto,
        "the stored preference should round-trip and fall back to Auto");
    require(RenderTargetPolicy::preferenceName(HdrOutputPreference::Always) == "always",
        "the preference should write back the name it reads");

    // Outside Windows nothing measures the panel, so a reported peak is Qt's
    // own fixed guess. Saying nothing leaves mpv to detect, which beats
    // passing 1000 nits off as a description of the display.
    DisplayOutputCapabilities guessing = hdrDisplay();
    guessing.luminanceMeasured = false;
    require(
        valueFor(RenderTargetPolicy::targetOptions(RenderTargetPolicy::resolve(guessing, HdrOutputPreference::Auto)),
            "target-peak")
            .isEmpty(),
        "an unmeasured peak should not reach mpv as though it were measured");

    RenderTargetOverrides manual;
    manual.maxLuminanceNits = 600.0f;
    require(valueFor(RenderTargetPolicy::targetOptions(
                         RenderTargetPolicy::resolve(guessing, HdrOutputPreference::Auto, manual)),
                "target-peak")
            == "600",
        "a peak the viewer set should be used where nothing measured one");
    require(valueFor(RenderTargetPolicy::targetOptions(
                         RenderTargetPolicy::resolve(hdrDisplay(), HdrOutputPreference::Auto, manual)),
                "target-peak")
            == "600",
        "a peak the viewer set should beat even a measured one");

    manual.sdrWhiteNits = 120.0f;
    require(valueFor(RenderTargetPolicy::targetOptions(
                         RenderTargetPolicy::resolve(hdrDisplay(), HdrOutputPreference::Auto, manual)),
                "hdr-reference-white")
            == "120",
        "a reference white the viewer set should win too");
    require(!RenderTargetPolicy::resolve(sdrOnly, HdrOutputPreference::Auto, manual).isHdr(),
        "an override is not a reason to claim an HDR target that does not exist");
    return 0;
}
