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
    SubtitleLanguage,
    SubtitleMode,
    SubtitleStyling,
    SubtitleTextSize,
    SubtitleTextWeight,
    SubtitleFont,
    SubtitleTextColor,
    SubtitleDropShadow,
    SubtitleTextBackground,
    SubtitleVerticalPosition,
    SubtitleScale,
    SubtitleBitmapSmoothing,
    SubtitleDimInHdr,
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
    SubtitleTextSize,
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
    int valueBoxWidth;
    int sliderPreferredWidth;
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
