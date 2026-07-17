#include "SettingsController.h"

#include "../api/JellyfinApiFacade.h"
#include "../cache/DatabaseManager.h"
#include "../common/AsyncTask.h"
#include "../player/AudioSyncPolicy.h"
#include "../player/PlayerController.h"
#include "SettingsSchema.h"

#include <QDebug>
#include <QFontDatabase>
#include <QJsonArray>
#include <QSet>

namespace JellyfinNative {

namespace {

    constexpr auto kUiScaleSetupVersionKey = "appearance/uiScaleSetupVersion";
    constexpr int kUiScaleSetupVersion = 2;
    constexpr int kUiScaleRebasePercent = 15;

    QString keyString(const SettingSpec& spec)
    {
        return QString::fromLatin1(spec.key);
    }

    const SettingSpec& specForKey(const char *key)
    {
        const SettingSpec *spec = findSettingSpec(QString::fromLatin1(key));
        Q_ASSERT(spec);
        return *spec;
    }

} // namespace

SettingsController::SettingsController(
    DatabaseManager *database, JellyfinApiFacade *api, PlayerController *player, QObject *parent)
    : QObject(parent)
    , m_database(database)
    , m_api(api)
    , m_player(player)
{
    m_subtitleApplyTimer.setSingleShot(true);
    m_subtitleApplyTimer.setInterval(100);
    connect(&m_subtitleApplyTimer, &QTimer::timeout, this, [this]() {
        if (m_player)
            m_player->setSubtitlePreferences(m_subtitlePreferences);
    });
}

QStringList SettingsController::subtitleLanguageOptions() const
{
    return m_subtitleLanguageLabels;
}
int SettingsController::subtitleLanguageIndex() const
{
    const int index = m_subtitleLanguageCodes.indexOf(m_subtitlePreferences.language);
    return index >= 0 ? index : 0;
}
QString SettingsController::subtitleMode() const
{
    return m_subtitlePreferences.mode;
}
QString SettingsController::subtitleBurnIn() const
{
    return m_subtitlePreferences.burnInMode;
}
bool SettingsController::subtitleRenderPgs() const
{
    return m_subtitlePreferences.renderPgs;
}
bool SettingsController::subtitleAlwaysBurnIn() const
{
    return m_subtitlePreferences.alwaysBurnInWhenTranscoding;
}
QString SettingsController::subtitleStyling() const
{
    return m_subtitlePreferences.styling;
}
QString SettingsController::subtitleTextSize() const
{
    return m_subtitlePreferences.textSize;
}
QString SettingsController::subtitleTextWeight() const
{
    return m_subtitlePreferences.textWeight;
}
QString SettingsController::subtitleFont() const
{
    return m_subtitlePreferences.font;
}
QString SettingsController::subtitleTextColor() const
{
    return m_subtitlePreferences.textColor;
}
QString SettingsController::subtitleDropShadow() const
{
    return m_subtitlePreferences.dropShadow;
}
int SettingsController::subtitleVerticalPosition() const
{
    return m_subtitlePreferences.verticalPosition;
}
QStringList SettingsController::availableButtonActions() const
{
    QStringList actions;
    const auto& spec = specForKey("input/redButton");
    actions.reserve(spec.choiceCount);
    for (qsizetype i = 0; i < spec.choiceCount; ++i)
        actions.push_back(QLatin1String(spec.choices[i].value));
    return actions;
}

QStringList SettingsController::systemSubtitleFonts() const
{
#ifdef JELLYFIN_NATIVE_WEBOS
    return {};
#else
    QStringList families = QFontDatabase::families();
    families.sort(Qt::CaseInsensitive);
    families.removeDuplicates();
    return families;
#endif
}

QVariantList SettingsController::settingsSchema() const
{
    return settingSchemaModel();
}

QVariantMap SettingsController::values() const
{
    return m_values;
}

QVariant SettingsController::value(const QString& key) const
{
    if (m_values.contains(key))
        return m_values.value(key);
    if (const SettingSpec *spec = findSettingSpec(key))
        return settingDefaultValue(*spec);
    return {};
}

QString SettingsController::audioDelayTargetLabel() const
{
#ifdef JELLYFIN_NATIVE_WEBOS
    return AudioSyncPolicy::outputDisplayName(m_currentAudioOutput);
#else
    return QStringLiteral("Global");
#endif
}

QCoro::Task<void> SettingsController::loadLocalAsync()
{
    QStringList keys;
    keys.reserve(static_cast<qsizetype>(settingSpecs().size()) + 1);
    for (const SettingSpec& spec : settingSpecs()) {
#ifdef JELLYFIN_NATIVE_WEBOS
        if (spec.target == SettingTarget::AudioDelay)
            continue;
#endif
        keys.append(keyString(spec));
    }
    keys.append(QString::fromLatin1(kUiScaleSetupVersionKey));
    const QVariantMap storedValues = co_await m_database->loadValuesAsync(keys);

    for (const SettingSpec& spec : settingSpecs()) {
        const QString key = keyString(spec);
#ifdef JELLYFIN_NATIVE_WEBOS
        if (spec.target == SettingTarget::AudioDelay) {
            const QVariant normalized = normalizedSettingValue(spec, spec.defaultValue);
            m_values.insert(key, normalized);
            applySchemaValue(spec, normalized, false);
            continue;
        }
#endif
        const QVariant rawValue = storedValues.value(key);
        const QString stored = !rawValue.isValid() || rawValue.toString().isEmpty()
            ? QString::fromLatin1(spec.defaultValue)
            : rawValue.toString();
        const QVariant normalized = normalizedSettingValue(spec, stored);
        m_values.insert(key, normalized);
        applySchemaValue(spec, normalized, false);

        const QString serialized = serializedSettingValue(spec, normalized);
        if (serialized != stored)
            m_database->saveSetting(key, serialized);
    }
    const QString setupVersion
        = storedValues.value(QString::fromLatin1(kUiScaleSetupVersionKey), QStringLiteral("0")).toString();
    const int storedSetupVersion = setupVersion.toInt();
    m_uiScaleSetupVersion = storedSetupVersion > 0 ? qMin(storedSetupVersion, kUiScaleSetupVersion) : 0;
    if (storedSetupVersion == 1) {
        const SettingSpec& scaleSpec = specForKey("appearance/uiScalePercent");
        const int rebasedScale = normalizedSettingValue(scaleSpec, m_uiScalePercent + kUiScaleRebasePercent).toInt();
        m_values.insert(keyString(scaleSpec), rebasedScale);
        applySchemaValue(scaleSpec, rebasedScale, false);
        m_database->saveSetting(keyString(scaleSpec), serializedSettingValue(scaleSpec, rebasedScale));
        m_uiScaleSetupVersion = kUiScaleSetupVersion;
        m_database->saveSetting(QString::fromLatin1(kUiScaleSetupVersionKey), QString::number(kUiScaleSetupVersion));
    }

    m_localSettingsLoaded = true;
#ifdef JELLYFIN_NATIVE_WEBOS
    loadCurrentWebOSAudioDelay();
#endif

    if (m_player) {
        m_player->setNightModeEnabled(m_nightModeEnabled);
        m_player->setToneMappingVisualizationEnabled(m_toneMappingVisualizationEnabled);
        applyAudioDelayToPlayer();
        m_player->setAudioOutputMode(m_audioOutputMode);
    }
    applyPlaybackPreferences();
    if (m_player)
        m_player->setSubtitlePreferences(m_subtitlePreferences);

    emit settingsValuesChanged();
    emit nightModeChanged();
    emit toneMappingVisualizationChanged();
    emit playbackPreferencesChanged();
    emit audioDelayChanged();
    emit audioOutputModeChanged();
    emit subtitleSettingsChanged();
    emit buttonRemapChanged();
    emit appearanceChanged();
}

void SettingsController::loadRemote()
{
    if (!m_api || m_api->session().accessToken.isEmpty())
        return;

    Async::runScoped(
        this, m_api->fetchCultures(),
        [this](const QJsonArray& cultures) {
            QStringList codes { QString() };
            QStringList labels { QStringLiteral("Any language") };
            QSet<QString> seen { QString() };

            for (const QJsonValue& value : cultures) {
                const QJsonObject culture = value.toObject();
                const QString code = culture.value(QStringLiteral("ThreeLetterISOLanguageName")).toString();
                if (code.isEmpty() || seen.contains(code))
                    continue;
                QString label = culture.value(QStringLiteral("DisplayName")).toString();
                if (label.isEmpty())
                    label = code.toUpper();
                seen.insert(code);
                codes.push_back(code);
                labels.push_back(label);
            }

            if (!m_subtitlePreferences.language.isEmpty() && !seen.contains(m_subtitlePreferences.language)) {
                codes.push_back(m_subtitlePreferences.language);
                labels.push_back(m_subtitlePreferences.language.toUpper());
            }

            m_subtitleLanguageCodes = codes;
            m_subtitleLanguageLabels = labels;
            emit subtitleSettingsChanged();
        },
        [](const std::exception_ptr& error) {
            qWarning() << "subtitles: culture list failed" << exceptionMessage(error);
        });

    Async::runScoped(
        this, m_api->fetchUserConfiguration(),
        [this](const QJsonObject& configuration) {
            m_userConfiguration = configuration;
            const SettingSpec& languageSpec = specForKey("subtitles/language");
            const SettingSpec& modeSpec = specForKey("subtitles/mode");
            setSchemaValue(languageSpec, configuration.value(QStringLiteral("SubtitleLanguagePreference")).toString(),
                true, false, false);
            setSchemaValue(modeSpec,
                configuration.value(QStringLiteral("SubtitleMode")).toString(QStringLiteral("Default")), true, false,
                false);
            applySubtitlePreferencesToPlayer();
            emit settingsValuesChanged();
            emit subtitleSettingsChanged();
        },
        [](const std::exception_ptr& error) {
            qWarning() << "subtitles: user configuration failed" << exceptionMessage(error);
        });
}

void SettingsController::clearRemote()
{
    m_userConfiguration = {};
}

void SettingsController::setValue(const QString& key, const QVariant& value)
{
    const SettingSpec *spec = findSettingSpec(key);
    if (!spec) {
        qWarning() << "settings: unknown key" << key;
        return;
    }
    if (spec->target == SettingTarget::AudioDelay) {
        setAudioDelayMs(value.toInt());
        return;
    }
    setSchemaValue(*spec, value, true, true, true);
}

void SettingsController::setNightModeEnabled(bool enabled)
{
    setValue(QStringLiteral("settings/nightMode"), enabled);
}
void SettingsController::setToneMappingVisualizationEnabled(bool enabled)
{
    setValue(QStringLiteral("settings/toneMappingVisualization"), enabled);
}
void SettingsController::setMaxStreamingBitrateMbps(int bitrateMbps)
{
    setValue(QStringLiteral("playback/maxStreamingBitrateMbps"), bitrateMbps);
}
void SettingsController::setPreferRemux(bool enabled)
{
    setValue(QStringLiteral("playback/preferRemux"), enabled);
}
void SettingsController::setAudioDelayMs(int delayMs)
{
    const SettingSpec& spec = specForKey("settings/audioDelayMs");
#ifdef JELLYFIN_NATIVE_WEBOS
    const int normalized = normalizedSettingValue(spec, delayMs).toInt();
    if (m_audioDelayMs == normalized)
        return;

    const int previous = m_audioDelayMs;
    m_audioOutputLoadGeneration.invalidate();
    m_audioDelayMs = normalized;
    m_values.insert(keyString(spec), normalized);
    m_database->saveSetting(
        AudioSyncPolicy::delayStorageKey(m_currentAudioOutput), serializedSettingValue(spec, normalized));
    applyAudioDelayToPlayer();
    qInfo() << "app: audio delay trim for" << AudioSyncPolicy::normalizedOutputKey(m_currentAudioOutput) << previous
            << "->" << m_audioDelayMs << "ms; automatic" << m_automaticAudioDelayMs << "ms; effective"
            << qBound(-2000, m_automaticAudioDelayMs + m_audioDelayMs, 2000) << "ms";
    emit settingChanged(keyString(spec));
    emit settingsValuesChanged();
    emit audioDelayChanged();
#else
    setSchemaValue(spec, delayMs, true, true, true);
#endif
}
void SettingsController::setAudioOutputMode(const QString& mode)
{
    setValue(QStringLiteral("settings/audioOutputMode"), mode);
}
void SettingsController::setUiScalePercent(int percent)
{
    setValue(QStringLiteral("appearance/uiScalePercent"), percent);
}
void SettingsController::completeUiScaleSetup(int percent)
{
    setUiScalePercent(percent);
    if (m_uiScaleSetupVersion >= kUiScaleSetupVersion)
        return;
    m_uiScaleSetupVersion = kUiScaleSetupVersion;
    m_database->saveSetting(QString::fromLatin1(kUiScaleSetupVersionKey), QString::number(kUiScaleSetupVersion));
    emit appearanceChanged();
}
void SettingsController::setSubtitleLanguageIndex(int index)
{
    if (index >= 0 && index < m_subtitleLanguageCodes.size())
        setValue(QStringLiteral("subtitles/language"), m_subtitleLanguageCodes.at(index));
}
void SettingsController::setSubtitleMode(const QString& mode)
{
    setValue(QStringLiteral("subtitles/mode"), mode);
}
void SettingsController::setSubtitleBurnIn(const QString& mode)
{
    setValue(QStringLiteral("subtitles/burnIn"), mode);
}
void SettingsController::setSubtitleRenderPgs(bool enabled)
{
    setValue(QStringLiteral("subtitles/renderPgs"), enabled);
}
void SettingsController::setSubtitleAlwaysBurnIn(bool enabled)
{
    setValue(QStringLiteral("subtitles/alwaysBurnInWhenTranscoding"), enabled);
}
void SettingsController::setSubtitleStyling(const QString& styling)
{
    setValue(QStringLiteral("subtitles/styling"), styling);
}
void SettingsController::setSubtitleTextSize(const QString& size)
{
    setValue(QStringLiteral("subtitles/textSize"), size);
}
void SettingsController::setSubtitleTextWeight(const QString& weight)
{
    setValue(QStringLiteral("subtitles/textWeight"), weight);
}
void SettingsController::setSubtitleFont(const QString& font)
{
    setValue(QStringLiteral("subtitles/font"), font);
}
void SettingsController::setSubtitleTextColor(const QString& color)
{
    setValue(QStringLiteral("subtitles/textColor"), color);
}
void SettingsController::setSubtitleDropShadow(const QString& shadow)
{
    setValue(QStringLiteral("subtitles/dropShadow"), shadow);
}
void SettingsController::setSubtitleVerticalPosition(int position)
{
    setValue(QStringLiteral("subtitles/verticalPositionPercent"), position);
}
void SettingsController::setRedButtonAction(const QString& action)
{
    setValue(QStringLiteral("input/redButton"), action);
}
void SettingsController::setGreenButtonAction(const QString& action)
{
    setValue(QStringLiteral("input/greenButton"), action);
}
void SettingsController::setYellowButtonAction(const QString& action)
{
    setValue(QStringLiteral("input/yellowButton"), action);
}
void SettingsController::setBlueButtonAction(const QString& action)
{
    setValue(QStringLiteral("input/blueButton"), action);
}

bool SettingsController::setSchemaValue(
    const SettingSpec& spec, const QVariant& value, bool persist, bool apply, bool notify)
{
    const QString key = keyString(spec);
    const QVariant normalized = normalizedSettingValue(spec, value);
    if (m_values.contains(key) && m_values.value(key) == normalized)
        return false;

    const int previousAudioDelayMs = m_audioDelayMs;
    const QString previousAudioOutputMode = m_audioOutputMode;

    m_values.insert(key, normalized);
    applySchemaValue(spec, normalized, apply);

    if (persist)
        m_database->saveSetting(key, serializedSettingValue(spec, normalized));

    if (apply && spec.target == SettingTarget::AudioDelay) {
        qInfo() << "app: audio delay changed" << previousAudioDelayMs << "->" << m_audioDelayMs << "ms";
    } else if (apply && spec.target == SettingTarget::AudioOutput) {
        qInfo() << "app: audio output mode changed" << previousAudioOutputMode << "->" << m_audioOutputMode;
    }

    if (notify) {
        emit settingChanged(key);
        emit settingsValuesChanged();
        emitSchemaSignals(spec);
    }
    return true;
}

void SettingsController::applySchemaValue(const SettingSpec& spec, const QVariant& value, bool apply)
{
    switch (spec.target) {
    case SettingTarget::NightMode:
        m_nightModeEnabled = value.toBool();
        if (apply && m_player)
            m_player->setNightModeEnabled(m_nightModeEnabled);
        break;
    case SettingTarget::ToneMappingVisualization:
        m_toneMappingVisualizationEnabled = value.toBool();
        if (apply && m_player)
            m_player->setToneMappingVisualizationEnabled(m_toneMappingVisualizationEnabled);
        break;
    case SettingTarget::ManualStreamingBitrate:
        m_manualStreamingBitrate = value.toBool();
        if (apply)
            applyPlaybackPreferences();
        break;
    case SettingTarget::MaxStreamingBitrate:
        m_maxStreamingBitrateMbps = value.toInt();
        if (apply)
            applyPlaybackPreferences();
        break;
    case SettingTarget::UnlimitedLocalBitrate:
        m_unlimitedLocalBitrate = value.toBool();
        if (apply)
            applyPlaybackPreferences();
        break;
    case SettingTarget::PreferRemux:
        m_preferRemux = value.toBool();
        if (apply)
            applyPlaybackPreferences();
        break;
    case SettingTarget::PlayerVolumeSlider:
        break;
    case SettingTarget::AudioDelay:
        m_audioDelayMs = value.toInt();
        if (apply)
            applyAudioDelayToPlayer();
        break;
    case SettingTarget::AudioOutput:
        m_audioOutputMode = value.toString();
        if (apply && m_player)
            m_player->setAudioOutputMode(m_audioOutputMode);
        break;
    case SettingTarget::UiScale:
        m_uiScalePercent = value.toInt();
        break;
    case SettingTarget::SubtitleLanguage:
        m_subtitlePreferences.language = value.toString();
        if (apply) {
            saveSubtitleUserConfiguration();
            applySubtitlePreferencesToPlayer();
        }
        break;
    case SettingTarget::SubtitleMode:
        m_subtitlePreferences.mode = value.toString();
        if (apply) {
            saveSubtitleUserConfiguration();
            applySubtitlePreferencesToPlayer();
        }
        break;
    case SettingTarget::SubtitleBurnIn:
        m_subtitlePreferences.burnInMode = value.toString();
        break;
    case SettingTarget::SubtitleRenderPgs:
        m_subtitlePreferences.renderPgs = value.toBool();
        break;
    case SettingTarget::SubtitleAlwaysBurnIn:
        m_subtitlePreferences.alwaysBurnInWhenTranscoding = value.toBool();
        break;
    case SettingTarget::SubtitleStyling:
        m_subtitlePreferences.styling = value.toString();
        if (apply)
            applySubtitlePreferencesToPlayer();
        break;
    case SettingTarget::SubtitleTextSize:
        m_subtitlePreferences.textSize = value.toString();
        if (apply)
            applySubtitlePreferencesToPlayer();
        break;
    case SettingTarget::SubtitleTextWeight:
        m_subtitlePreferences.textWeight = value.toString();
        if (apply)
            applySubtitlePreferencesToPlayer();
        break;
    case SettingTarget::SubtitleFont:
        m_subtitlePreferences.font = value.toString();
        if (apply)
            applySubtitlePreferencesToPlayer();
        break;
    case SettingTarget::SubtitleTextColor:
        m_subtitlePreferences.textColor = value.toString();
        if (apply)
            applySubtitlePreferencesToPlayer();
        break;
    case SettingTarget::SubtitleDropShadow:
        m_subtitlePreferences.dropShadow = value.toString();
        if (apply)
            applySubtitlePreferencesToPlayer();
        break;
    case SettingTarget::SubtitleTextBackground:
        m_subtitlePreferences.textBackground = value.toString();
        break;
    case SettingTarget::SubtitleVerticalPosition:
        m_subtitlePreferences.verticalPosition = value.toInt();
        if (apply)
            applySubtitlePreferencesToPlayer();
        break;
    case SettingTarget::SubtitleScale:
        m_subtitlePreferences.scalePercent = value.toInt();
        if (apply)
            applySubtitlePreferencesToPlayer();
        break;
    case SettingTarget::SubtitleBitmapSmoothing:
        m_subtitlePreferences.bitmapSmoothing = value.toString();
        if (apply)
            applySubtitlePreferencesToPlayer();
        break;
    case SettingTarget::SubtitleDimInHdr:
        m_subtitlePreferences.dimInHdr = value.toBool();
        if (apply)
            applySubtitlePreferencesToPlayer();
        break;
    case SettingTarget::SubtitleHdrBrightness:
        m_subtitlePreferences.hdrBrightnessPercent = value.toInt();
        if (apply)
            applySubtitlePreferencesToPlayer();
        break;
    case SettingTarget::RedButton:
        m_redButtonAction = value.toString();
        break;
    case SettingTarget::GreenButton:
        m_greenButtonAction = value.toString();
        break;
    case SettingTarget::YellowButton:
        m_yellowButtonAction = value.toString();
        break;
    case SettingTarget::BlueButton:
        m_blueButtonAction = value.toString();
        break;
    }
}

void SettingsController::emitSchemaSignals(const SettingSpec& spec)
{
    switch (spec.target) {
    case SettingTarget::NightMode:
        emit nightModeChanged();
        break;
    case SettingTarget::ToneMappingVisualization:
        emit toneMappingVisualizationChanged();
        break;
    case SettingTarget::ManualStreamingBitrate:
    case SettingTarget::MaxStreamingBitrate:
    case SettingTarget::UnlimitedLocalBitrate:
    case SettingTarget::PreferRemux:
        emit playbackPreferencesChanged();
        break;
    case SettingTarget::PlayerVolumeSlider:
        break;
    case SettingTarget::AudioDelay:
        emit audioDelayChanged();
        break;
    case SettingTarget::AudioOutput:
        emit audioOutputModeChanged();
        break;
    case SettingTarget::UiScale:
        emit appearanceChanged();
        break;
    case SettingTarget::SubtitleLanguage:
    case SettingTarget::SubtitleMode:
    case SettingTarget::SubtitleBurnIn:
    case SettingTarget::SubtitleRenderPgs:
    case SettingTarget::SubtitleAlwaysBurnIn:
    case SettingTarget::SubtitleStyling:
    case SettingTarget::SubtitleTextSize:
    case SettingTarget::SubtitleTextWeight:
    case SettingTarget::SubtitleFont:
    case SettingTarget::SubtitleTextColor:
    case SettingTarget::SubtitleDropShadow:
    case SettingTarget::SubtitleTextBackground:
    case SettingTarget::SubtitleVerticalPosition:
    case SettingTarget::SubtitleScale:
    case SettingTarget::SubtitleBitmapSmoothing:
    case SettingTarget::SubtitleDimInHdr:
    case SettingTarget::SubtitleHdrBrightness:
        emit subtitleSettingsChanged();
        break;
    case SettingTarget::RedButton:
    case SettingTarget::GreenButton:
    case SettingTarget::YellowButton:
    case SettingTarget::BlueButton:
        emit buttonRemapChanged();
        break;
    }
}

void SettingsController::applyPlaybackPreferences()
{
    if (!m_api)
        return;
    const qint64 manualBitrate
        = m_manualStreamingBitrate ? static_cast<qint64>(m_maxStreamingBitrateMbps) * 1'000'000 : 0;
    m_api->setPlaybackPreferences(manualBitrate, m_unlimitedLocalBitrate, m_preferRemux);
}

void SettingsController::applyAudioDelayToPlayer()
{
    if (!m_player)
        return;
#ifdef JELLYFIN_NATIVE_WEBOS
    m_player->setAudioDelayMs(qBound(-2000, m_automaticAudioDelayMs + m_audioDelayMs, 2000));
#else
    m_player->setAudioDelayMs(m_audioDelayMs);
#endif
}

void SettingsController::updateWebOSAudioOutput(const QString& output, int displayLatencyMs, int outputLatencyMs)
{
#ifdef JELLYFIN_NATIVE_WEBOS
    const QString normalizedOutput = AudioSyncPolicy::normalizedOutputKey(output);
    const int automaticDelay
        = AudioSyncPolicy::automaticBaseDelayMs(normalizedOutput, displayLatencyMs, outputLatencyMs);
    const bool outputChanged = normalizedOutput != m_currentAudioOutput;
    const bool automaticDelayChanged = automaticDelay != m_automaticAudioDelayMs;
    if (!outputChanged && !automaticDelayChanged)
        return;

    qInfo() << "app: webOS audio output" << normalizedOutput << "display latency" << displayLatencyMs
            << "ms; output latency" << outputLatencyMs << "ms; automatic delay" << automaticDelay << "ms";

    m_audioOutputLoadGeneration.invalidate();
    m_currentAudioOutput = normalizedOutput;
    m_automaticAudioDelayMs = automaticDelay;
    if (outputChanged) {
        m_audioDelayMs = 0;
        m_values.insert(QStringLiteral("settings/audioDelayMs"), m_audioDelayMs);
    }
    applyAudioDelayToPlayer();

    emit audioOutputDeviceChanged();
    if (outputChanged) {
        emit settingChanged(QStringLiteral("settings/audioDelayMs"));
        emit settingsValuesChanged();
        emit audioDelayChanged();
        if (m_localSettingsLoaded)
            loadCurrentWebOSAudioDelay();
    }
#else
    Q_UNUSED(output)
    Q_UNUSED(displayLatencyMs)
    Q_UNUSED(outputLatencyMs)
#endif
}

void SettingsController::loadCurrentWebOSAudioDelay()
{
#ifdef JELLYFIN_NATIVE_WEBOS
    const QString output = AudioSyncPolicy::normalizedOutputKey(m_currentAudioOutput);
    const auto token = m_audioOutputLoadGeneration.next();
    Async::runLatest(
        this, m_database->loadSettingAsync(AudioSyncPolicy::delayStorageKey(output), QStringLiteral("0")),
        m_audioOutputLoadGeneration, token,
        [this, output](const QString& stored) {
            const SettingSpec& spec = specForKey("settings/audioDelayMs");
            applyLoadedWebOSAudioDelay(output, normalizedSettingValue(spec, stored).toInt());
        },
        [](const std::exception_ptr& error) {
            qWarning() << "app: failed to load webOS audio delay trim:" << exceptionMessage(error);
        },
        "load webOS audio delay trim");
#endif
}

void SettingsController::applyLoadedWebOSAudioDelay(const QString& output, int delayMs)
{
#ifdef JELLYFIN_NATIVE_WEBOS
    if (AudioSyncPolicy::normalizedOutputKey(output) != m_currentAudioOutput)
        return;

    m_audioDelayMs = delayMs;
    m_values.insert(QStringLiteral("settings/audioDelayMs"), delayMs);
    applyAudioDelayToPlayer();
    qInfo() << "app: loaded audio delay trim for" << m_currentAudioOutput << delayMs << "ms; automatic"
            << m_automaticAudioDelayMs << "ms; effective"
            << qBound(-2000, m_automaticAudioDelayMs + m_audioDelayMs, 2000) << "ms";
    emit settingChanged(QStringLiteral("settings/audioDelayMs"));
    emit settingsValuesChanged();
    emit audioDelayChanged();
#else
    Q_UNUSED(output)
    Q_UNUSED(delayMs)
#endif
}

void SettingsController::saveSubtitleUserConfiguration()
{
    if (!m_api || m_api->session().accessToken.isEmpty())
        return;

    QJsonObject configuration = m_userConfiguration;
    configuration.insert(QStringLiteral("SubtitleLanguagePreference"), m_subtitlePreferences.language);
    configuration.insert(QStringLiteral("SubtitleMode"), m_subtitlePreferences.mode);
    m_userConfiguration = configuration;

    Async::runScoped(
        this, m_api->updateUserConfiguration(configuration), []() {},
        [this](const std::exception_ptr& error) { emit errorOccurred(exceptionMessage(error)); });
}

void SettingsController::applySubtitlePreferencesToPlayer()
{
    if (m_player)
        m_subtitleApplyTimer.start();
}

} // namespace JellyfinNative
