#include "SettingsSchema.h"

#include "../common/JellyfinTypes.h"
#include "../platform/PlatformSettingsPolicy.h"

#include <QVariantMap>

#include <algorithm>

namespace JellyfinNative {
namespace {

    constexpr SettingChoice kUiDetailLevelChoices[]
        = { { "Essential", "Essential" }, { "More", "Advanced" }, { "All", "Expert" } };
    constexpr SettingChoice kMpvConfigModeChoices[]
        = { { "disabled", "Disabled" }, { "standard", "Standard mpv directory" }, { "custom", "Custom directory" } };
    constexpr SettingChoice kAccentChoices[]
        = { { "0", "Jellyfin Blue" }, { "1", "Jellyfin Purple" }, { "2", "Blue-Purple" } };
    constexpr SettingChoice kRailLabelChoices[]
        = { { "Never", "Never" }, { "On focus", "On focus" }, { "Always", "Always" } };
    constexpr SettingChoice kTextRenderModeChoices[] = { { "0", "Qt" }, { "1", "Curve" } };
    constexpr SettingChoice kTechnicalMetadataChoices[]
        = { { "Always", "Always" }, { "On details only", "On details only" }, { "Hidden", "Hidden" } };
    constexpr SettingChoice kSubtitleModeChoices[]
        = { { "Default", "Default - use embedded Default and Forced flags; preferred language breaks ties" },
              { "Smart", "Smart - show the preferred language when selected audio uses another language" },
              { "OnlyForced", "Only forced - show only tracks marked Forced" },
              { "Always", "Always - show the preferred subtitle language regardless of audio language" },
              { "None", "None - do not select subtitles automatically" } };
    constexpr SettingChoice kSubtitleStylingChoices[]
        = { { "Auto", "Automatic - respect ASS/SSA styles and style plain text" },
              { "Native", "Respect embedded styles" }, { "Custom", "Override with my style" } };
    constexpr SettingChoice kSubtitleTextSizeChoices[] = { { "smaller", "Smaller" }, { "small", "Small" },
        { "", "Normal" }, { "large", "Large" }, { "larger", "Larger" }, { "extralarge", "Extra large" } };
    constexpr SettingChoice kSubtitleTextWeightChoices[] = { { "normal", "Normal" }, { "bold", "Bold" } };
    constexpr SettingChoice kSubtitleFontChoices[] = { { "", "Default" }, { "serif", "Source Serif 4" },
        { "typewriter", "Typewriter" }, { "print", "Print" }, { "console", "Console" }, { "cursive", "Cursive" },
        { "casual", "Casual" }, { "smallcaps", "Small caps" } };
    constexpr SettingChoice kSubtitleTextColorChoices[] = { { "#ffffff", "White" }, { "#d3d3d3", "Light gray" },
        { "#808080", "Gray" }, { "#ffff00", "Yellow" }, { "#008000", "Green" }, { "#00ffff", "Cyan" },
        { "#0000ff", "Blue" }, { "#ff00ff", "Magenta" }, { "#ff0000", "Red" }, { "#000000", "Black" } };
    constexpr SettingChoice kSubtitleDropShadowChoices[] = { { "none", "None" }, { "raised", "Raised" },
        { "depressed", "Depressed" }, { "uniform", "Uniform" }, { "", "Drop shadow" } };
    constexpr SettingChoice kSubtitleBackgroundChoices[]
        = { { "transparent", "None" }, { "translucent", "Translucent black" }, { "opaque", "Solid black" } };
    constexpr SettingChoice kSubtitleBitmapSmoothingChoices[]
        = { { "sharp", "Sharp" }, { "soft", "Smooth" }, { "softer", "Extra smooth" } };
    constexpr SettingChoice kButtonActionChoices[] = { { "none", "No action" }, { "togglePause", "Play / Pause" },
        { "toggleSubs", "Toggle subtitles" }, { "cycleSubs", "Cycle subtitles" }, { "cycleAudio", "Cycle audio track" },
        { "queuePrevious", "Previous queue item" }, { "queueNext", "Next queue item" },
        { "skipBack10", "Skip back 10 s" }, { "skipForward10", "Skip forward 10 s" },
        { "skipBack30", "Skip back 30 s" }, { "skipForward30", "Skip forward 30 s" },
        { "skipBack90", "Skip back 90 s" }, { "skipForward90", "Skip forward 90 s" },
        { "skipBackAndEnableSubs", "Skip back 10 s + enable subs" }, { "skipSegment", "Skip intro / outro" },
        { "showInfo", "Show info" }, { "stop", "Stop playback" } };

    template <size_t N> constexpr qsizetype countOf(const SettingChoice (&)[N])
    {
        return static_cast<qsizetype>(N);
    }

    QString typeName(SettingType type)
    {
        switch (type) {
        case SettingType::Action:
            return QStringLiteral("action");
        case SettingType::ReadOnly:
            return QStringLiteral("readonly");
        case SettingType::Text:
            return QStringLiteral("text");
        case SettingType::Toggle:
            return QStringLiteral("toggle");
        case SettingType::Select:
            return QStringLiteral("select");
        case SettingType::Slider:
            return QStringLiteral("slider");
        }
        return QStringLiteral("readonly");
    }

    int levelValue(SettingLevel level)
    {
        return static_cast<int>(level);
    }

    QString platformName(SettingPlatform platform)
    {
        switch (platform) {
        case SettingPlatform::All:
            return QStringLiteral("all");
        case SettingPlatform::Desktop:
            return QStringLiteral("desktop");
        case SettingPlatform::WebOS:
            return QStringLiteral("webos");
        }
        return QStringLiteral("all");
    }

    bool boolValue(const QVariant& value)
    {
        if (value.typeId() == QMetaType::Bool)
            return value.toBool();
        const QString text = value.toString().trimmed().toLower();
        return text == QStringLiteral("true") || text == QStringLiteral("1") || text == QStringLiteral("yes");
    }

    bool hasChoice(const SettingSpec& spec, const QString& value)
    {
        for (qsizetype i = 0; i < spec.choiceCount; ++i) {
            if (value == QLatin1String(spec.choices[i].value))
                return true;
        }
        return false;
    }

    QString choiceOrDefault(const SettingSpec& spec, const QString& value)
    {
        return hasChoice(spec, value) ? value : QLatin1String(spec.defaultValue);
    }

    QString normalizedSubtitleColor(QString color)
    {
        color = color.trimmed().toLower();
        if (!color.startsWith(QLatin1Char('#')) || color.size() != 7)
            return QStringLiteral("#ffffff");
        for (int i = 1; i < color.size(); ++i) {
            const QChar ch = color.at(i);
            if (!ch.isDigit() && (ch < QLatin1Char('a') || ch > QLatin1Char('f')))
                return QStringLiteral("#ffffff");
        }
        return color;
    }

    SettingSpec externalSpec(const char *key, const char *group, const char *title, const char *description,
        SettingType type, SettingLevel level = SettingLevel::Essential, const SettingChoice *choices = nullptr,
        qsizetype choiceCount = 0)
    {
        SettingSpec spec { key, group, title, description, type, "", choices, choiceCount, 0, 0, 1, 0, "", 0, 0,
            SettingTarget::External,
            type == SettingType::Select ? SettingNormalizer::Choice : SettingNormalizer::String, true,
            type == SettingType::Select };
        spec.level = level;
        spec.persisted = false;
        return spec;
    }

} // namespace

const QVector<SettingSpec>& settingSpecs()
{
    const PlatformAudioOutputPolicy& audioOutput = platformAudioOutputPolicy();
    static const QVector<SettingSpec> specs {
        { "settings/detailLevel", "General", "Settings shown",
            "Essential keeps everyday choices short; Advanced adds tuning; Expert reveals diagnostics and platform "
            "controls",
            SettingType::Select, "Essential", kUiDetailLevelChoices, countOf(kUiDetailLevelChoices), 0, 0, 1, 0, "", 0,
            0, SettingTarget::UiDetailLevel, SettingNormalizer::Choice, true, true },
        externalSpec("session/account", "Account", "Active Account", "Current account and Jellyfin server",
            SettingType::ReadOnly),
        externalSpec("action/switchUser", "Account", "Switch User", "Return to profile selection", SettingType::Action),
        externalSpec("action/logout", "Account", "Sign out of this account",
            "Keep this profile on the device and require authentication next time", SettingType::Action),
        externalSpec("i18n/locale", "Appearance", "Language", "Restart the app to update cached server text",
            SettingType::Select),
        externalSpec("theme/accent", "Appearance", "Accent", "Interface highlight colour", SettingType::Select,
            SettingLevel::Essential, kAccentChoices, countOf(kAccentChoices)),
        externalSpec("action/uiScaleSetup", "Appearance", "Scale Setup",
            "Compare Compact, Balanced, and Relaxed layouts", SettingType::Action),
        externalSpec("theme/reducedMotion", "Appearance", "Reduced Motion", "Reduce focus and page animation",
            SettingType::Toggle),
        externalSpec("theme/railLabels", "Appearance", "Side Rail Labels", "Choose when navigation labels appear",
            SettingType::Select, SettingLevel::Advanced, kRailLabelChoices, countOf(kRailLabelChoices)),
        externalSpec("theme/renderMode", "Appearance", "Text Render Mode", "Choose Qt or curve-based text rendering",
            SettingType::Select, SettingLevel::Advanced, kTextRenderModeChoices, countOf(kTextRenderModeChoices)),
        externalSpec("theme/antialiasedText", "Appearance", "Antialiased Text", "Smooth text glyph edges",
            SettingType::Toggle, SettingLevel::Advanced),
        externalSpec("theme/technicalMetadata", "Appearance", "Show Technical Metadata",
            "Control codec and stream details in browse and detail views", SettingType::Select, SettingLevel::Advanced,
            kTechnicalMetadataChoices, countOf(kTechnicalMetadataChoices)),
        externalSpec("about/version", "About", "Jellyfin Native", "Qt 6.11 client with native mpv playback",
            SettingType::ReadOnly),
        externalSpec("action/openSourceNotices", "About", "Open-source notices",
            "Acknowledgements, licenses, and corresponding source", SettingType::Action),
        externalSpec("about/locale", "About", "UI Locale", "Current interface locale", SettingType::ReadOnly),
        externalSpec("shell/diagnostics", "Diagnostics", "Diagnostics Overlay",
            "Show live playback and performance diagnostics", SettingType::Toggle, SettingLevel::Expert),
        externalSpec("shell/latencyGuard", "Diagnostics", "Latency Logging",
            "Record input latency samples in diagnostics", SettingType::Toggle, SettingLevel::Expert),
        externalSpec("shell/latencyOverlay", "Diagnostics", "Latency Warnings",
            "Show warnings when input processing exceeds the threshold", SettingType::Toggle, SettingLevel::Expert),
        externalSpec("action/clearLatencyStatistics", "Diagnostics", "Clear Latency Statistics",
            "Erase the current in-memory latency samples", SettingType::Action, SettingLevel::Expert),
        { "appearance/uiScalePercent", "Appearance", "UI Scale",
            "Scale text, controls, spacing, cards, rows, and grids", SettingType::Slider, "115", nullptr, 0, 80, 180, 5,
            0, "%", 86, 300, SettingTarget::UiScale, SettingNormalizer::IntRange, true, false },
        { "playback/manualStreamingBitrate", "Playback", "Manual Streaming Limit",
            "Override automatic server-route bandwidth measurement", SettingType::Toggle, "false", nullptr, 0, 0, 0, 1,
            0, "", 0, 0, SettingTarget::ManualStreamingBitrate, SettingNormalizer::Bool, true, false },
        { "playback/maxStreamingBitrateMbps", "Playback", "Maximum Streaming Bitrate",
            "Maximum bitrate before Jellyfin transcodes", SettingType::Slider, "120", nullptr, 0, 5, 1000, 5, 0, "Mbps",
            112, 340, SettingTarget::MaxStreamingBitrate, SettingNormalizer::IntRange, true, false,
            SettingLevel::Essential, SettingPlatform::All, "speed", "", "playback/manualStreamingBitrate", "true" },
        { "settings/nightMode", "Playback", "Night Mode", "Dialogue lift and late-night dynamic range",
            SettingType::Toggle, "false", nullptr, 0, 0, 0, 1, 0, "", 0, 0, SettingTarget::NightMode,
            SettingNormalizer::Bool, true, false },
        { "settings/audioDelayMs", "Playback", "A/V Sync", "Adjust audio timing in milliseconds; applied immediately",
            SettingType::Slider, "0", nullptr, 0, -2000, 2000, 10, 0, "ms", 92, 340, SettingTarget::AudioDelay,
            SettingNormalizer::IntRange, true, false },
        { "playback/showVolumeSlider", "Playback", "Player Volume Slider", "Show in desktop player controls",
            SettingType::Toggle, "true", nullptr, 0, 0, 0, 1, 0, "", 0, 0, SettingTarget::PlayerVolumeSlider,
            SettingNormalizer::Bool, true, false, SettingLevel::Essential, SettingPlatform::Desktop },
        { "playback/unlimitedLocalBitrate", "Playback", "Unlimited on Local Network",
            "Ignore bitrate limits when Jellyfin identifies this connection as local", SettingType::Toggle, "false",
            nullptr, 0, 0, 0, 1, 0, "", 0, 0, SettingTarget::UnlimitedLocalBitrate, SettingNormalizer::Bool, true,
            false, SettingLevel::Advanced },
        { "playback/preferRemux", "Playback", "Prefer Remux", "Copy compatible streams before transcoding",
            SettingType::Toggle, "true", nullptr, 0, 0, 0, 1, 0, "", 0, 0, SettingTarget::PreferRemux,
            SettingNormalizer::Bool, true, false, SettingLevel::Advanced },
        { "playback/forwardCacheSizeMiB", "Playback", "Forward Cache",
            "Memory reserved for upcoming media; takes effect on the next playback", SettingType::Slider, "32", nullptr,
            0, 16, 4096, 1, 0, "MB", 100, 340, SettingTarget::ForwardCacheSize, SettingNormalizer::IntRange, true,
            false, SettingLevel::Advanced },
        { "settings/audioOutputMode", "Playback", "Audio Output",
            "Automatic lets mpv choose the first available platform output; takes effect on the next playback",
            SettingType::Select, audioOutput.defaultValue, audioOutput.choices, audioOutput.choiceCount, 0, 0, 1, 0, "",
            0, 0, SettingTarget::AudioOutput, SettingNormalizer::AudioOutput, true, true, SettingLevel::Advanced },
        { "playback/mpvConfigMode", "Playback", "mpv Configuration",
            "Expert desktop option. Custom configuration and scripts can change or break playback; application input "
            "remains owned by Tern. Takes effect on the next playback.",
            SettingType::Select, "disabled", kMpvConfigModeChoices, countOf(kMpvConfigModeChoices), 0, 0, 1, 0, "", 0,
            0, SettingTarget::MpvConfigMode, SettingNormalizer::Choice, true, true, SettingLevel::Expert,
            SettingPlatform::Desktop, "tune" },
        { "playback/mpvConfigDirectory", "Playback", "Custom mpv Directory",
            "Absolute readable directory containing mpv.conf and optional scripts; takes effect on the next playback",
            SettingType::Text, "", nullptr, 0, 0, 0, 1, 0, "", 0, 0, SettingTarget::MpvConfigDirectory,
            SettingNormalizer::String, true, false, SettingLevel::Expert, SettingPlatform::Desktop, "folder", "",
            "playback/mpvConfigMode", "custom" },
        { "subtitles/language", "Subtitles", "Preferred Language", "Used when choosing a subtitle track automatically",
            SettingType::Select, "", nullptr, 0, 0, 0, 1, 0, "", 0, 0, SettingTarget::SubtitleLanguage,
            SettingNormalizer::String, true, false },
        { "subtitles/mode", "Subtitles", "Playback Mode", "Choose when Jellyfin automatically selects a subtitle track",
            SettingType::Select, "Default", kSubtitleModeChoices, countOf(kSubtitleModeChoices), 0, 0, 1, 0, "", 0, 0,
            SettingTarget::SubtitleMode, SettingNormalizer::SubtitleMode, true, true },
        externalSpec("action/subtitleSettings", "Subtitles", "Subtitle Appearance",
            "Preview and adjust subtitle language and appearance", SettingType::Action),
        { "subtitles/styling", "Subtitle Appearance", "Style Policy",
            "Automatic respects ASS/SSA styles and applies your choices to plain text. Bitmap subtitles can scale and "
            "smooth, but their font and colour cannot change.",
            SettingType::Select, "Auto", kSubtitleStylingChoices, countOf(kSubtitleStylingChoices), 0, 0, 1, 0, "", 0,
            0, SettingTarget::SubtitleStyling, SettingNormalizer::SubtitleStyling, true, true, SettingLevel::Advanced },
        { "subtitles/textSize", "Subtitle Appearance", "Text Size", "Base size for text subtitles", SettingType::Select,
            "", kSubtitleTextSizeChoices, countOf(kSubtitleTextSizeChoices), 0, 0, 1, 0, "", 0, 0,
            SettingTarget::SubtitleTextSize, SettingNormalizer::SubtitleTextSize, true, true, SettingLevel::Advanced,
            SettingPlatform::All, "subtitles" },
        { "subtitles/scalePercent", "Subtitle Appearance", "Overall Scale",
            "Scales text, styled, and bitmap subtitles together", SettingType::Slider, "100", nullptr, 0, 50, 200, 5, 0,
            "%", 86, 300, SettingTarget::SubtitleScale, SettingNormalizer::IntRange, true, false,
            SettingLevel::Advanced },
        { "subtitles/bitmapSmoothing", "Subtitle Appearance", "Bitmap Smoothing",
            "Controls scaling of PGS, VobSub, and other image subtitles", SettingType::Select, "soft",
            kSubtitleBitmapSmoothingChoices, countOf(kSubtitleBitmapSmoothingChoices), 0, 0, 1, 0, "", 0, 0,
            SettingTarget::SubtitleBitmapSmoothing, SettingNormalizer::String, true, true, SettingLevel::Advanced },
        { "subtitles/textWeight", "Subtitle Appearance", "Text Weight", "Improve readability on busy scenes",
            SettingType::Select, "normal", kSubtitleTextWeightChoices, countOf(kSubtitleTextWeightChoices), 0, 0, 1, 0,
            "", 0, 0, SettingTarget::SubtitleTextWeight, SettingNormalizer::SubtitleTextWeight, true, true,
            SettingLevel::Advanced, SettingPlatform::All, "subtitles" },
        { "subtitles/font", "Subtitle Appearance", "Font",
            "Bundled fonts are available everywhere; desktop also lists installed system fonts", SettingType::Select,
            "", kSubtitleFontChoices, countOf(kSubtitleFontChoices), 0, 0, 1, 0, "", 0, 0, SettingTarget::SubtitleFont,
            SettingNormalizer::SubtitleFont, true, true, SettingLevel::Advanced, SettingPlatform::All, "font" },
        { "subtitles/textColor", "Subtitle Appearance", "Text Colour", "Named colour swatches preview immediately",
            SettingType::Select, "#ffffff", kSubtitleTextColorChoices, countOf(kSubtitleTextColorChoices), 0, 0, 1, 0,
            "", 0, 0, SettingTarget::SubtitleTextColor, SettingNormalizer::SubtitleTextColor, true, true,
            SettingLevel::Advanced, SettingPlatform::All, "palette" },
        { "subtitles/dropShadow", "Subtitle Appearance", "Outline / Shadow",
            "Choose a readable edge treatment for text subtitles", SettingType::Select, "", kSubtitleDropShadowChoices,
            countOf(kSubtitleDropShadowChoices), 0, 0, 1, 0, "", 0, 0, SettingTarget::SubtitleDropShadow,
            SettingNormalizer::SubtitleDropShadow, true, true, SettingLevel::Advanced, SettingPlatform::All,
            "contrast" },
        { "subtitles/textBackground", "Subtitle Appearance", "Text Background", "Backdrop behind subtitle text",
            SettingType::Select, "transparent", kSubtitleBackgroundChoices, countOf(kSubtitleBackgroundChoices), 0, 0,
            1, 0, "", 0, 0, SettingTarget::SubtitleTextBackground, SettingNormalizer::String, true, false,
            SettingLevel::Advanced, SettingPlatform::All, "contrast" },
        { "subtitles/verticalPositionPercent", "Subtitle Appearance", "Vertical Position - Bottom ↔ Top",
            "Move subtitles from the original bottom position toward the top of the safe area", SettingType::Slider,
            "100", nullptr, 0, 0, 100, 1, 0, "%", 72, 300, SettingTarget::SubtitleVerticalPosition,
            SettingNormalizer::IntRange, true, false, SettingLevel::Advanced },
        { "subtitles/dimInHdr", "Subtitle Appearance", "Comfortable HDR White",
            "Dim subtitle white to an SDR-like paper-white level inside HDR video", SettingType::Toggle, "true",
            nullptr, 0, 0, 0, 1, 0, "", 0, 0, SettingTarget::SubtitleDimInHdr, SettingNormalizer::Bool, true, false,
            SettingLevel::Advanced, SettingPlatform::All, "hdr", "", "", "", true, true },
        { "subtitles/hdrBrightnessPercent", "Subtitle Appearance", "HDR Paper White",
            "Comfortable SDR-like subtitle white level inside HDR video", SettingType::Slider, "75", nullptr, 0, 40,
            100, 5, 0, "%", 86, 300, SettingTarget::SubtitleHdrBrightness, SettingNormalizer::IntRange, true, false,
            SettingLevel::Advanced, SettingPlatform::All, "hdr", "", "subtitles/dimInHdr", "true", true, true },
        externalSpec("action/resetSubtitleAppearance", "Subtitle Appearance", "Reset Appearance",
            "Restore local subtitle rendering defaults", SettingType::Action, SettingLevel::Advanced),
        { "settings/toneMappingVisualization", "Diagnostics", "GPU-next Tone Mapping View",
            "False-colour libplacebo tone-mapping diagnostic", SettingType::Toggle, "false", nullptr, 0, 0, 0, 1, 0, "",
            0, 0, SettingTarget::ToneMappingVisualization, SettingNormalizer::Bool, true, false, SettingLevel::Expert,
            SettingPlatform::Desktop },
        { "input/redButton", "Button Remap", "Red Button", "TV remote color button", SettingType::Select, "none",
            kButtonActionChoices, countOf(kButtonActionChoices), 0, 0, 1, 0, "", 0, 0, SettingTarget::RedButton,
            SettingNormalizer::String, true, false, SettingLevel::Expert, SettingPlatform::WebOS },
        { "input/greenButton", "Button Remap", "Green Button", "Defaults to skip back 10 s + enable subs",
            SettingType::Select, "skipBackAndEnableSubs", kButtonActionChoices, countOf(kButtonActionChoices), 0, 0, 1,
            0, "", 0, 0, SettingTarget::GreenButton, SettingNormalizer::String, true, false, SettingLevel::Expert,
            SettingPlatform::WebOS },
        { "input/yellowButton", "Button Remap", "Yellow Button", "TV remote color button action", SettingType::Select,
            "none", kButtonActionChoices, countOf(kButtonActionChoices), 0, 0, 1, 0, "", 0, 0,
            SettingTarget::YellowButton, SettingNormalizer::String, true, false, SettingLevel::Expert,
            SettingPlatform::WebOS },
        { "input/blueButton", "Button Remap", "Blue Button", "TV remote color button action", SettingType::Select,
            "none", kButtonActionChoices, countOf(kButtonActionChoices), 0, 0, 1, 0, "", 0, 0,
            SettingTarget::BlueButton, SettingNormalizer::String, true, false, SettingLevel::Expert,
            SettingPlatform::WebOS },
    };
    return specs;
}

const SettingSpec *findSettingSpec(const QString& key)
{
    const auto& specs = settingSpecs();
    const auto it = std::find_if(
        specs.cbegin(), specs.cend(), [&key](const SettingSpec& spec) { return key == QLatin1String(spec.key); });
    return it == specs.cend() ? nullptr : &(*it);
}

QVariant settingDefaultValue(const SettingSpec& spec)
{
    switch (spec.type) {
    case SettingType::Action:
    case SettingType::ReadOnly:
    case SettingType::Text:
        return QString::fromLatin1(spec.defaultValue);
    case SettingType::Toggle:
        return boolValue(QLatin1String(spec.defaultValue));
    case SettingType::Slider:
        return std::clamp(QString::fromLatin1(spec.defaultValue).toInt(), spec.minimum, spec.maximum);
    case SettingType::Select:
        return QString::fromLatin1(spec.defaultValue);
    }
    return QString::fromLatin1(spec.defaultValue);
}

QVariant normalizedSettingValue(const SettingSpec& spec, const QVariant& value)
{
    switch (spec.normalizer) {
    case SettingNormalizer::Bool:
        return boolValue(value);
    case SettingNormalizer::IntRange:
        return std::clamp(value.toString().toInt(), spec.minimum, spec.maximum);
    case SettingNormalizer::AudioOutput:
        return normalizedAudioOutputMode(value.toString());
    case SettingNormalizer::Choice:
        return choiceOrDefault(spec, value.toString());
    case SettingNormalizer::SubtitleMode:
    case SettingNormalizer::SubtitleStyling:
    case SettingNormalizer::SubtitleTextSize:
    case SettingNormalizer::SubtitleTextWeight:
    case SettingNormalizer::SubtitleFont: {
        const QString font = value.toString();
        return font.startsWith(QStringLiteral("system:")) && font.size() > 7 ? font : choiceOrDefault(spec, font);
    }
    case SettingNormalizer::SubtitleDropShadow:
        return choiceOrDefault(spec, value.toString());
    case SettingNormalizer::SubtitleTextColor:
        return normalizedSubtitleColor(value.toString());
    case SettingNormalizer::String:
        return spec.normalizeChoices ? choiceOrDefault(spec, value.toString()) : value.toString();
    }
    return value.toString();
}

QString serializedSettingValue(const SettingSpec& spec, const QVariant& value)
{
    const QVariant normalized = normalizedSettingValue(spec, value);
    if (spec.type == SettingType::Toggle)
        return normalized.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (spec.type == SettingType::Slider)
        return QString::number(normalized.toInt());
    return normalized.toString();
}

QVariantList settingSchemaModel()
{
    QVariantList model;
    model.reserve(settingSpecs().size());
    for (const SettingSpec& spec : settingSpecs()) {
        QVariantMap row { { QStringLiteral("key"), QLatin1String(spec.key) },
            { QStringLiteral("source"), spec.persisted ? QStringLiteral("settings") : QStringLiteral("page") },
            { QStringLiteral("group"), QLatin1String(spec.group) },
            { QStringLiteral("title"), QLatin1String(spec.title) },
            { QStringLiteral("description"), QLatin1String(spec.description) },
            { QStringLiteral("type"), typeName(spec.type) },
            { QStringLiteral("defaultValue"), settingDefaultValue(spec) }, { QStringLiteral("visible"), spec.visible },
            { QStringLiteral("level"), levelValue(spec.level) },
            { QStringLiteral("platform"), platformName(spec.platform) },
            { QStringLiteral("icon"), QLatin1String(spec.icon) },
            { QStringLiteral("valueSummary"), QLatin1String(spec.valueSummary) },
            { QStringLiteral("dependsOnKey"), QLatin1String(spec.dependsOnKey) },
            { QStringLiteral("dependsOnValue"), QLatin1String(spec.dependsOnValue) },
            { QStringLiteral("requiresHdrPlayback"), spec.requiresHdrPlayback },
            { QStringLiteral("from"), spec.minimum }, { QStringLiteral("to"), spec.maximum },
            { QStringLiteral("step"), spec.step }, { QStringLiteral("decimals"), spec.decimals },
            { QStringLiteral("unitText"), QLatin1String(spec.unit) },
            { QStringLiteral("valueBoxWidth"), spec.valueBoxWidth },
            { QStringLiteral("sliderPreferredWidth"), spec.sliderPreferredWidth } };
        QVariantList values;
        QVariantList labels;
        values.reserve(spec.choiceCount);
        labels.reserve(spec.choiceCount);
        for (qsizetype i = 0; i < spec.choiceCount; ++i) {
            values.push_back(QLatin1String(spec.choices[i].value));
            labels.push_back(QLatin1String(spec.choices[i].label));
        }
        row.insert(QStringLiteral("choiceValues"), values);
        row.insert(QStringLiteral("choiceLabels"), labels);
        model.push_back(row);
    }
    return model;
}

} // namespace JellyfinNative
