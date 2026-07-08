#pragma once

#include <QVariant>
#include <QVariantList>
#include <QVector>

namespace JellyfinNative {

enum class SettingType {
    Toggle,
    Select,
    Slider,
};

enum class SettingTarget {
    NightMode,
    ToneMappingVisualization,
    MaxStreamingBitrate,
    PreferRemux,
    AudioDelay,
    AudioOutput,
    SubtitleLanguage,
    SubtitleMode,
    SubtitleBurnIn,
    SubtitleRenderPgs,
    SubtitleAlwaysBurnIn,
    SubtitleStyling,
    SubtitleTextSize,
    SubtitleTextWeight,
    SubtitleFont,
    SubtitleTextColor,
    SubtitleDropShadow,
    SubtitleTextBackground,
    SubtitleVerticalPosition,
    RedButton,
    GreenButton,
    YellowButton,
    BlueButton,
};

enum class SettingNormalizer {
    Bool,
    IntRange,
    String,
    Choice,
    AudioOutput,
    SubtitleMode,
    SubtitleBurnIn,
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
};

const QVector<SettingSpec>& settingSpecs();
const SettingSpec *findSettingSpec(const QString& key);
QVariant settingDefaultValue(const SettingSpec& spec);
QVariant normalizedSettingValue(const SettingSpec& spec, const QVariant& value);
QString serializedSettingValue(const SettingSpec& spec, const QVariant& value);
QVariantList settingSchemaModel();

} // namespace JellyfinNative
