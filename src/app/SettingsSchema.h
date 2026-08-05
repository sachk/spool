#pragma once

#include <QVariant>
#include <QVariantList>
#include <QVector>

namespace JellyfinNative {

enum class SettingType {
    Action,
    ReadOnly,
    Text,
    Toggle,
    Select,
    Slider,
};

enum class SettingLevel {
    Essential,
    Advanced,
    Expert,
};

enum class SettingPlatform {
    All,
    Desktop,
    WebOS,
};

enum class SettingTarget {
    External,
    UiDetailLevel,
    NightMode,
    ToneMappingVisualization,
    ManualStreamingBitrate,
    MaxStreamingBitrate,
    UnlimitedLocalBitrate,
    PreferRemux,
    ForwardCacheSize,
    PlayerVolumeSlider,
    AudioDelay,
    AudioOutput,
    UiScale,
    LibraryView,
    AudioTrackMode,
    RememberSeriesAudioTrack,
    SubtitleLanguage,
    SubtitleMode,
    SubtitleStyling,
    SubtitleTextWeight,
    SubtitleFont,
    SubtitleTextColor,
    SubtitleTextColorOverride,
    SubtitleDropShadow,
    SubtitleTextBackground,
    SubtitleVerticalPosition,
    SubtitleScale,
    SubtitlePositionAndSizeOverride,
    SubtitleBitmapSharpness,
    SubtitleRecolorImages,
    SubtitleAllowInBlackBars,
    SubtitleHdrBrightness,
    RedButton,
    GreenButton,
    YellowButton,
    BlueButton,
    MpvConfigMode,
    MpvConfigDirectory,
};

enum class SettingNormalizer {
    Bool,
    IntRange,
    String,
    Choice,
    AudioOutput,
    SubtitleMode,
    SubtitleStyling,
    SubtitleTextWeight,
    SubtitleFont,
    SubtitleTextColor,
    SubtitleDropShadow,
};

struct SettingChoice {
    const char *value;
    const char *label;
};

struct SettingSpec {
    const char *key;
    const char *group;
    const char *title;
    const char *description;
    SettingType type;
    const char *defaultValue;
    const SettingChoice *choices;
    qsizetype choiceCount;
    int minimum;
    int maximum;
    int step;
    int decimals;
    const char *unit;
    SettingTarget target;
    SettingNormalizer normalizer;
    bool visible;
    bool normalizeChoices;
    SettingLevel level = SettingLevel::Essential;
    SettingPlatform platform = SettingPlatform::All;
    const char *icon = "settings";
    const char *valueSummary = "";
    const char *dependsOnKey = "";
    const char *dependsOnValue = "";
    bool persisted = true;
    bool requiresHdrPlayback = false;
};

const QVector<SettingSpec>& settingSpecs();
const SettingSpec *findSettingSpec(const QString& key);
QVariant settingDefaultValue(const SettingSpec& spec);
QVariant normalizedSettingValue(const SettingSpec& spec, const QVariant& value);
QString serializedSettingValue(const SettingSpec& spec, const QVariant& value);
QVariantList settingSchemaModel();

} // namespace JellyfinNative
