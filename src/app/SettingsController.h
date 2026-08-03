#pragma once

#include "../common/JellyfinTypes.h"
#include "../common/RequestGeneration.h"
#include "../platform/MpvConfigPolicy.h"

#include <QCoroTask>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace JellyfinNative {

class DatabaseManager;
class JellyfinApiFacade;
class PlayerController;
struct SettingSpec;

class SettingsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool nightModeEnabled MEMBER m_nightModeEnabled WRITE setNightModeEnabled NOTIFY nightModeChanged)
    Q_PROPERTY(bool toneMappingVisualizationEnabled MEMBER m_toneMappingVisualizationEnabled WRITE
            setToneMappingVisualizationEnabled NOTIFY toneMappingVisualizationChanged)
    Q_PROPERTY(int maxStreamingBitrateMbps MEMBER m_maxStreamingBitrateMbps WRITE setMaxStreamingBitrateMbps NOTIFY
            playbackPreferencesChanged)
    Q_PROPERTY(bool preferRemux MEMBER m_preferRemux WRITE setPreferRemux NOTIFY playbackPreferencesChanged)
    Q_PROPERTY(int audioDelayMs READ audioDelayMs WRITE setAudioDelayMs NOTIFY audioDelayChanged)
    Q_PROPERTY(int automaticAudioDelayMs READ automaticAudioDelayMs NOTIFY audioOutputDeviceChanged)
    Q_PROPERTY(int displayLatencyMs READ displayLatencyMs NOTIFY audioOutputDeviceChanged)
    Q_PROPERTY(QString audioDelayTargetLabel READ audioDelayTargetLabel NOTIFY audioOutputDeviceChanged)
    Q_PROPERTY(QString audioOutputMode MEMBER m_audioOutputMode WRITE setAudioOutputMode NOTIFY audioOutputModeChanged)
    Q_PROPERTY(int uiScalePercent READ uiScalePercent WRITE setUiScalePercent NOTIFY appearanceChanged)
    Q_PROPERTY(bool uiScaleSetupComplete READ uiScaleSetupComplete NOTIFY appearanceChanged)
    Q_PROPERTY(QStringList subtitleLanguageOptions READ subtitleLanguageOptions NOTIFY subtitleSettingsChanged)
    Q_PROPERTY(int subtitleLanguageIndex READ subtitleLanguageIndex WRITE setSubtitleLanguageIndex NOTIFY
            subtitleSettingsChanged)
    Q_PROPERTY(QString subtitleMode READ subtitleMode WRITE setSubtitleMode NOTIFY subtitleSettingsChanged)
    Q_PROPERTY(QString subtitleStyling READ subtitleStyling WRITE setSubtitleStyling NOTIFY subtitleSettingsChanged)
    Q_PROPERTY(QString subtitleTextSize READ subtitleTextSize WRITE setSubtitleTextSize NOTIFY subtitleSettingsChanged)
    Q_PROPERTY(
        QString subtitleTextWeight READ subtitleTextWeight WRITE setSubtitleTextWeight NOTIFY subtitleSettingsChanged)
    Q_PROPERTY(QString subtitleFont READ subtitleFont WRITE setSubtitleFont NOTIFY subtitleSettingsChanged)
    Q_PROPERTY(
        QString subtitleTextColor READ subtitleTextColor WRITE setSubtitleTextColor NOTIFY subtitleSettingsChanged)
    Q_PROPERTY(
        QString subtitleDropShadow READ subtitleDropShadow WRITE setSubtitleDropShadow NOTIFY subtitleSettingsChanged)
    Q_PROPERTY(int subtitleVerticalPosition READ subtitleVerticalPosition WRITE setSubtitleVerticalPosition NOTIFY
            subtitleSettingsChanged)
    Q_PROPERTY(QString redButtonAction MEMBER m_redButtonAction WRITE setRedButtonAction NOTIFY buttonRemapChanged)
    Q_PROPERTY(
        QString greenButtonAction MEMBER m_greenButtonAction WRITE setGreenButtonAction NOTIFY buttonRemapChanged)
    Q_PROPERTY(
        QString yellowButtonAction MEMBER m_yellowButtonAction WRITE setYellowButtonAction NOTIFY buttonRemapChanged)
    Q_PROPERTY(QString blueButtonAction MEMBER m_blueButtonAction WRITE setBlueButtonAction NOTIFY buttonRemapChanged)
    Q_PROPERTY(QStringList availableButtonActions READ availableButtonActions CONSTANT)
    Q_PROPERTY(QStringList systemSubtitleFonts READ systemSubtitleFonts CONSTANT)
    Q_PROPERTY(QVariantList settingsSchema READ settingsSchema CONSTANT)
    Q_PROPERTY(QVariantMap values READ values NOTIFY settingsValuesChanged)
    Q_PROPERTY(
        bool playerControlTooltipsEnabled READ playerControlTooltipsEnabled NOTIFY playerControlTooltipsEnabledChanged)

public:
    SettingsController(
        DatabaseManager *database, JellyfinApiFacade *api, PlayerController *player, QObject *parent = nullptr);

    int uiScalePercent() const
    {
        return m_uiScalePercent;
    }
    bool uiScaleSetupComplete() const
    {
        return m_uiScaleSetupVersion > 0;
    }
    int audioDelayMs() const
    {
        return m_audioDelayMs;
    }
    int automaticAudioDelayMs() const
    {
        return m_automaticAudioDelayMs;
    }
    int displayLatencyMs() const
    {
        return m_displayLatencyMs;
    }
    QString audioDelayTargetLabel() const;
    int subtitleLanguageIndex() const;
    int subtitleVerticalPosition() const;
    QString subtitleMode() const;
    QString subtitleStyling() const;
    QString subtitleTextSize() const;
    QString subtitleTextWeight() const;
    QString subtitleFont() const;
    QString subtitleTextColor() const;
    QString subtitleDropShadow() const;
    QStringList subtitleLanguageOptions() const;
    QStringList availableButtonActions() const;
    QStringList systemSubtitleFonts() const;
    QVariantList settingsSchema() const;
    QVariantMap values() const;
    Q_INVOKABLE QVariant value(const QString& key) const;
    bool playerControlTooltipsEnabled() const
    {
        return m_playerControlTooltipSessions < 3;
    }

    static QStringList localSettingKeys();
    void applyLocalValues(const QVariantMap& storedValues);

    QCoro::Task<void> loadLocalAsync();
    Q_INVOKABLE void loadRemote();
    void clearRemote();
    Q_INVOKABLE void setValue(const QString& key, const QVariant& value);
    Q_INVOKABLE void completePlayerControlTooltipSession();
    Q_INVOKABLE void setNightModeEnabled(bool enabled);
    Q_INVOKABLE void setToneMappingVisualizationEnabled(bool enabled);
    Q_INVOKABLE void setMaxStreamingBitrateMbps(int bitrateMbps);
    Q_INVOKABLE void setPreferRemux(bool enabled);
    Q_INVOKABLE void setAudioDelayMs(int delayMs);
    Q_INVOKABLE void setAudioOutputMode(const QString& mode);
    Q_INVOKABLE void setUiScalePercent(int percent);
    Q_INVOKABLE void completeUiScaleSetup(int percent);
    Q_INVOKABLE void setSubtitleLanguageIndex(int index);
    Q_INVOKABLE void setSubtitleMode(const QString& mode);
    Q_INVOKABLE void setSubtitleStyling(const QString& styling);
    Q_INVOKABLE void setSubtitleTextSize(const QString& size);
    Q_INVOKABLE void setSubtitleTextWeight(const QString& weight);
    Q_INVOKABLE void setSubtitleFont(const QString& font);
    Q_INVOKABLE void setSubtitleTextColor(const QString& color);
    Q_INVOKABLE void setSubtitleDropShadow(const QString& shadow);
    Q_INVOKABLE void setSubtitleVerticalPosition(int position);
    Q_INVOKABLE void resetSubtitleAppearance();
    Q_INVOKABLE void setRedButtonAction(const QString& action);
    Q_INVOKABLE void setGreenButtonAction(const QString& action);
    Q_INVOKABLE void setYellowButtonAction(const QString& action);
    Q_INVOKABLE void setBlueButtonAction(const QString& action);
    QString mpvConfigMode() const
    {
        return m_mpvConfigMode;
    }
    QString mpvConfigDirectory() const
    {
        return m_mpvConfigDirectory;
    }
    void updateAudioOutputRoute(const QString& output, int displayLatencyMs, int outputLatencyMs);

signals:
    void nightModeChanged();
    void toneMappingVisualizationChanged();
    void playbackPreferencesChanged();
    void audioDelayChanged();
    void audioOutputDeviceChanged();
    void audioOutputModeChanged();
    void subtitleSettingsChanged();
    void buttonRemapChanged();
    void appearanceChanged();
    void settingChanged(const QString& key);
    void settingsValuesChanged();
    void playerControlTooltipsEnabledChanged();
    void errorOccurred(const QString& message);

private:
    bool setSchemaValue(const SettingSpec& spec, const QVariant& value, bool persist, bool apply, bool notify);
    void applySchemaValue(const SettingSpec& spec, const QVariant& value, bool apply);
    void emitSchemaSignals(const SettingSpec& spec);
    void applyPlaybackPreferences();
    void applyAudioDelayToPlayer();
    void loadCurrentAudioDelay();
    void applyLoadedAudioDelay(const QString& output, int delayMs);
    void saveSubtitleUserConfiguration();
    void applySubtitlePreferencesToPlayer();
    void applyMpvConfigPolicy();

    DatabaseManager *m_database = nullptr;
    JellyfinApiFacade *m_api = nullptr;
    PlayerController *m_player = nullptr;
    QVariantMap m_values;
    bool m_remoteLoadStarted = false;
    bool m_nightModeEnabled = false;
    bool m_toneMappingVisualizationEnabled = false;
    bool m_manualStreamingBitrate = false;
    int m_maxStreamingBitrateMbps = 120;
    bool m_unlimitedLocalBitrate = false;
    bool m_preferRemux = true;
    int m_forwardCacheSizeMiB = 32;
    int m_audioDelayMs = 0;
    int m_automaticAudioDelayMs = 0;
    int m_displayLatencyMs = 0;
    QString m_currentAudioOutput;
    bool m_localSettingsLoaded = false;
    RequestGeneration m_audioOutputLoadGeneration;
    QString m_audioOutputMode = QStringLiteral("auto");
    QString m_mpvConfigMode = QStringLiteral("disabled");
    QString m_mpvConfigDirectory;
    int m_uiScalePercent = 115;
    int m_uiScaleSetupVersion = 0;
    int m_playerControlTooltipSessions = 0;
    SubtitlePreferences m_subtitlePreferences;
    QStringList m_subtitleLanguageCodes { QString() };
    QStringList m_subtitleLanguageLabels { QStringLiteral("Any language") };
    QJsonObject m_userConfiguration;
    QString m_redButtonAction = QStringLiteral("none");
    QString m_greenButtonAction = QStringLiteral("skipBackAndEnableSubs");
    QString m_yellowButtonAction = QStringLiteral("none");
    QString m_blueButtonAction = QStringLiteral("none");
};

} // namespace JellyfinNative
