#include "SettingsSchema.h"

#include "../common/JellyfinTypes.h"

#include <QVariantMap>

#include <algorithm>

namespace JellyfinNative {
namespace {

    constexpr SettingChoice kAudioOutputChoices[] = { { "alsa", "ALSA" }, { "starfish-pcm", "Starfish" } };
    constexpr SettingChoice kSubtitleModeChoices[] = { { "Default", "Default" }, { "Smart", "Smart" },
        { "OnlyForced", "Only forced" }, { "Always", "Always play" }, { "None", "None" } };
    constexpr SettingChoice kSubtitleBurnInChoices[] = { { "", "Auto" }, { "onlyimageformats", "Only image formats" },
        { "allcomplexformats", "All complex formats" }, { "all", "All" } };
    constexpr SettingChoice kSubtitleStylingChoices[]
        = { { "Auto", "Auto" }, { "Custom", "Custom" }, { "Native", "Native" } };
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
        case SettingType::Toggle:
            return QStringLiteral("toggle");
        case SettingType::Select:
            return QStringLiteral("select");
        case SettingType::Slider:
            return QStringLiteral("slider");
        }
        return QStringLiteral("select");
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

} // namespace

const QVector<SettingSpec>& settingSpecs()
{
    static const QVector<SettingSpec> specs {
        { "appearance/uiScalePercent", "Appearance", "UI Scale",
            "Scale text, controls, spacing, cards, rows, and grids", SettingType::Slider, "115", nullptr, 0, 80, 180, 5,
            0, "%", 86, 300, SettingTarget::UiScale, SettingNormalizer::IntRange, true, false },
        { "settings/nightMode", "Playback", "Night Mode", "Dialogue lift and late-night dynamic range",
            SettingType::Toggle, "false", nullptr, 0, 0, 0, 1, 0, "", 0, 0, SettingTarget::NightMode,
            SettingNormalizer::Bool, true, false },
        { "playback/manualStreamingBitrate", "Playback", "Manual Streaming Limit",
            "Override automatic server-route bandwidth measurement", SettingType::Toggle, "false", nullptr, 0, 0, 0, 1,
            0, "", 0, 0, SettingTarget::ManualStreamingBitrate, SettingNormalizer::Bool, true, false },
        { "playback/maxStreamingBitrateMbps", "Playback", "Maximum Streaming Bitrate",
            "Maximum bitrate before Jellyfin transcodes", SettingType::Slider, "120", nullptr, 0, 5, 1000, 5, 0, "Mbps",
            112, 340, SettingTarget::MaxStreamingBitrate, SettingNormalizer::IntRange, true, false },
        { "playback/unlimitedLocalBitrate", "Playback", "Unlimited on Local Network",
            "Ignore bitrate limits when Jellyfin identifies this connection as local", SettingType::Toggle, "false",
            nullptr, 0, 0, 0, 1, 0, "", 0, 0, SettingTarget::UnlimitedLocalBitrate, SettingNormalizer::Bool, true,
            false },
        { "playback/preferRemux", "Playback", "Prefer Remux", "Copy compatible streams before transcoding",
            SettingType::Toggle, "true", nullptr, 0, 0, 0, 1, 0, "", 0, 0, SettingTarget::PreferRemux,
            SettingNormalizer::Bool, true, false },
        { "playback/showVolumeSlider", "Playback", "Player Volume Slider", "Show in desktop player controls",
            SettingType::Toggle, "true", nullptr, 0, 0, 0, 1, 0, "", 0, 0, SettingTarget::PlayerVolumeSlider,
            SettingNormalizer::Bool, true, false },
        { "settings/audioDelayMs", "Playback", "A/V Sync", "Audio delay in milliseconds", SettingType::Slider, "0",
            nullptr, 0, -2000, 2000, 10, 0, "ms", 92, 340, SettingTarget::AudioDelay, SettingNormalizer::IntRange, true,
            false },
        { "settings/audioOutputMode", "Playback", "Audio Output", "Takes effect on the next playback start",
            SettingType::Select, "alsa", kAudioOutputChoices, countOf(kAudioOutputChoices), 0, 0, 1, 0, "", 0, 0,
            SettingTarget::AudioOutput, SettingNormalizer::AudioOutput, true, true },
        { "subtitles/language", "Subtitles", "Preferred Language", "", SettingType::Select, "", nullptr, 0, 0, 0, 1, 0,
            "", 0, 0, SettingTarget::SubtitleLanguage, SettingNormalizer::String, true, false },
        { "subtitles/mode", "Subtitles", "Playback Mode", "", SettingType::Select, "Default", kSubtitleModeChoices,
            countOf(kSubtitleModeChoices), 0, 0, 1, 0, "", 0, 0, SettingTarget::SubtitleMode,
            SettingNormalizer::SubtitleMode, true, true },
        { "subtitles/burnIn", "Subtitles", "Burn Subtitles", "Used when transcoding is enabled", SettingType::Select,
            "", kSubtitleBurnInChoices, countOf(kSubtitleBurnInChoices), 0, 0, 1, 0, "", 0, 0,
            SettingTarget::SubtitleBurnIn, SettingNormalizer::SubtitleBurnIn, true, true },
        { "subtitles/renderPgs", "Subtitles", "Render PGS Subtitles", "Prefer local rendering for image subtitles",
            SettingType::Toggle, "false", nullptr, 0, 0, 0, 1, 0, "", 0, 0, SettingTarget::SubtitleRenderPgs,
            SettingNormalizer::Bool, true, false },
        { "subtitles/alwaysBurnInWhenTranscoding", "Subtitles", "Always Burn In",
            "When playback falls back to transcoding", SettingType::Toggle, "false", nullptr, 0, 0, 0, 1, 0, "", 0, 0,
            SettingTarget::SubtitleAlwaysBurnIn, SettingNormalizer::Bool, true, false },
        { "subtitles/styling", "Subtitle Appearance", "Styling", "", SettingType::Select, "Auto",
            kSubtitleStylingChoices, countOf(kSubtitleStylingChoices), 0, 0, 1, 0, "", 0, 0,
            SettingTarget::SubtitleStyling, SettingNormalizer::SubtitleStyling, true, true },
        { "subtitles/textSize", "Subtitle Appearance", "Text Size", "", SettingType::Select, "",
            kSubtitleTextSizeChoices, countOf(kSubtitleTextSizeChoices), 0, 0, 1, 0, "", 0, 0,
            SettingTarget::SubtitleTextSize, SettingNormalizer::SubtitleTextSize, true, true },
        { "subtitles/scalePercent", "Subtitle Appearance", "Overall Scale",
            "Scales text, styled, and bitmap subtitles together", SettingType::Slider, "100", nullptr, 0, 50, 200, 5, 0,
            "%", 86, 300, SettingTarget::SubtitleScale, SettingNormalizer::IntRange, true, false },
        { "subtitles/bitmapSmoothing", "Subtitle Appearance", "Bitmap Smoothing",
            "Controls scaling of PGS, VobSub, and other image subtitles", SettingType::Select, "soft",
            kSubtitleBitmapSmoothingChoices, countOf(kSubtitleBitmapSmoothingChoices), 0, 0, 1, 0, "", 0, 0,
            SettingTarget::SubtitleBitmapSmoothing, SettingNormalizer::String, true, true },
        { "subtitles/textWeight", "Subtitle Appearance", "Text Weight", "", SettingType::Select, "normal",
            kSubtitleTextWeightChoices, countOf(kSubtitleTextWeightChoices), 0, 0, 1, 0, "", 0, 0,
            SettingTarget::SubtitleTextWeight, SettingNormalizer::SubtitleTextWeight, true, true },
        { "subtitles/font", "Subtitle Appearance", "Font", "", SettingType::Select, "", kSubtitleFontChoices,
            countOf(kSubtitleFontChoices), 0, 0, 1, 0, "", 0, 0, SettingTarget::SubtitleFont,
            SettingNormalizer::SubtitleFont, true, true },
        { "subtitles/textColor", "Subtitle Appearance", "Text Color", "", SettingType::Select, "#ffffff",
            kSubtitleTextColorChoices, countOf(kSubtitleTextColorChoices), 0, 0, 1, 0, "", 0, 0,
            SettingTarget::SubtitleTextColor, SettingNormalizer::SubtitleTextColor, true, true },
        { "subtitles/dropShadow", "Subtitle Appearance", "Drop Shadow", "", SettingType::Select, "",
            kSubtitleDropShadowChoices, countOf(kSubtitleDropShadowChoices), 0, 0, 1, 0, "", 0, 0,
            SettingTarget::SubtitleDropShadow, SettingNormalizer::SubtitleDropShadow, true, true },
        { "subtitles/textBackground", "Subtitle Appearance", "Text Background", "", SettingType::Select, "transparent",
            kSubtitleBackgroundChoices, countOf(kSubtitleBackgroundChoices), 0, 0, 1, 0, "", 0, 0,
            SettingTarget::SubtitleTextBackground, SettingNormalizer::String, true, false },
        { "subtitles/verticalPositionPercent", "Subtitle Appearance", "Vertical Position",
            "Screen height: 0% is the top and 100% is the original bottom position", SettingType::Slider, "100",
            nullptr, 0, 0, 100, 1, 0, "%", 72, 300, SettingTarget::SubtitleVerticalPosition,
            SettingNormalizer::IntRange, true, false },
        { "subtitles/dimInHdr", "Subtitle Appearance", "HDR Paper White",
            "Automatically reduce subtitle brightness during HDR playback", SettingType::Toggle, "true", nullptr, 0, 0,
            0, 1, 0, "", 0, 0, SettingTarget::SubtitleDimInHdr, SettingNormalizer::Bool, true, false },
        { "subtitles/hdrBrightnessPercent", "Subtitle Appearance", "HDR Brightness",
            "Paper-white subtitle level relative to the selected colour", SettingType::Slider, "75", nullptr, 0, 40,
            100, 5, 0, "%", 86, 300, SettingTarget::SubtitleHdrBrightness, SettingNormalizer::IntRange, true, false },
        { "settings/toneMappingVisualization", "Diagnostics", "GPU-next Tone Mapping View",
            "False-colour libplacebo tone-mapping diagnostic", SettingType::Toggle, "false", nullptr, 0, 0, 0, 1, 0, "",
            0, 0, SettingTarget::ToneMappingVisualization, SettingNormalizer::Bool, true, false },
        { "input/redButton", "Button Remap", "Red Button", "TV remote color button", SettingType::Select, "none",
            kButtonActionChoices, countOf(kButtonActionChoices), 0, 0, 1, 0, "", 0, 0, SettingTarget::RedButton,
            SettingNormalizer::String, true, false },
        { "input/greenButton", "Button Remap", "Green Button", "Defaults to skip back 10 s + enable subs",
            SettingType::Select, "skipBackAndEnableSubs", kButtonActionChoices, countOf(kButtonActionChoices), 0, 0, 1,
            0, "", 0, 0, SettingTarget::GreenButton, SettingNormalizer::String, true, false },
        { "input/yellowButton", "Button Remap", "Yellow Button", "", SettingType::Select, "none", kButtonActionChoices,
            countOf(kButtonActionChoices), 0, 0, 1, 0, "", 0, 0, SettingTarget::YellowButton, SettingNormalizer::String,
            true, false },
        { "input/blueButton", "Button Remap", "Blue Button", "", SettingType::Select, "none", kButtonActionChoices,
            countOf(kButtonActionChoices), 0, 0, 1, 0, "", 0, 0, SettingTarget::BlueButton, SettingNormalizer::String,
            true, false },
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
    case SettingNormalizer::SubtitleMode:
    case SettingNormalizer::SubtitleBurnIn:
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
            { QStringLiteral("source"), QStringLiteral("settings") },
            { QStringLiteral("group"), QLatin1String(spec.group) },
            { QStringLiteral("title"), QLatin1String(spec.title) },
            { QStringLiteral("description"), QLatin1String(spec.description) },
            { QStringLiteral("type"), typeName(spec.type) },
            { QStringLiteral("defaultValue"), settingDefaultValue(spec) }, { QStringLiteral("visible"), spec.visible },
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
