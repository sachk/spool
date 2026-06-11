#include "SettingsController.h"

#include "../api/JellyfinApiFacade.h"
#include "../cache/DatabaseManager.h"
#include "../common/AsyncTask.h"
#include "../player/PlayerController.h"

#include <QDebug>
#include <QJsonArray>
#include <QSet>

#include <algorithm>

namespace JellyfinNative {

namespace {

QString normalizedChoice(const QString &value, const QStringList &allowed,
                         const QString &fallback) {
  return allowed.contains(value) ? value : fallback;
}

QString normalizedSubtitleMode(const QString &mode) {
  return normalizedChoice(mode,
                          {QStringLiteral("Default"), QStringLiteral("Smart"),
                           QStringLiteral("OnlyForced"),
                           QStringLiteral("Always"), QStringLiteral("None")},
                          QStringLiteral("Default"));
}

QString normalizedSubtitleBurnIn(const QString &mode) {
  return normalizedChoice(mode,
                          {QString(), QStringLiteral("onlyimageformats"),
                           QStringLiteral("allcomplexformats"),
                           QStringLiteral("all")},
                          QString());
}

QString normalizedSubtitleStyling(const QString &styling) {
  return normalizedChoice(styling,
                          {QStringLiteral("Auto"), QStringLiteral("Custom"),
                           QStringLiteral("Native")},
                          QStringLiteral("Auto"));
}

QString normalizedSubtitleTextSize(const QString &size) {
  return normalizedChoice(size,
                          {QStringLiteral("smaller"), QStringLiteral("small"),
                           QString(), QStringLiteral("large"),
                           QStringLiteral("larger"),
                           QStringLiteral("extralarge")},
                          QString());
}

QString normalizedSubtitleTextWeight(const QString &weight) {
  return normalizedChoice(weight,
                          {QStringLiteral("normal"), QStringLiteral("bold")},
                          QStringLiteral("normal"));
}

QString normalizedSubtitleFont(const QString &font) {
  return normalizedChoice(font,
                          {QString(), QStringLiteral("typewriter"),
                           QStringLiteral("print"), QStringLiteral("console"),
                           QStringLiteral("cursive"), QStringLiteral("casual"),
                           QStringLiteral("smallcaps")},
                          QString());
}

QString normalizedSubtitleDropShadow(const QString &shadow) {
  return normalizedChoice(shadow,
                          {QStringLiteral("none"), QStringLiteral("raised"),
                           QStringLiteral("depressed"),
                           QStringLiteral("uniform"), QString()},
                          QString());
}

QString normalizedSubtitleColor(QString color) {
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

bool loadBoolSetting(DatabaseManager *database, const QString &key,
                     bool fallback) {
  const QString value =
      database
          ->loadSetting(key, fallback ? QStringLiteral("true")
                                      : QStringLiteral("false"))
          .trimmed()
          .toLower();
  return value == QStringLiteral("true") || value == QStringLiteral("1") ||
         value == QStringLiteral("yes");
}

} // namespace

SettingsController::SettingsController(DatabaseManager *database,
                                       JellyfinApiFacade *api,
                                       PlayerController *player,
                                       QObject *parent)
    : QObject(parent), m_database(database), m_api(api), m_player(player) {}

bool SettingsController::visible() const { return m_visible; }

bool SettingsController::nightModeEnabled() const { return m_nightModeEnabled; }

bool SettingsController::toneMappingVisualizationEnabled() const {
  return m_toneMappingVisualizationEnabled;
}

int SettingsController::audioDelayMs() const { return m_audioDelayMs; }

QString SettingsController::audioOutputMode() const {
  return m_audioOutputMode;
}

QStringList SettingsController::subtitleLanguageOptions() const {
  return m_subtitleLanguageLabels;
}

int SettingsController::subtitleLanguageIndex() const {
  const int index =
      m_subtitleLanguageCodes.indexOf(m_subtitlePreferences.language);
  return index >= 0 ? index : 0;
}

QString SettingsController::subtitleMode() const {
  return m_subtitlePreferences.mode;
}

QString SettingsController::subtitleBurnIn() const {
  return m_subtitlePreferences.burnInMode;
}

bool SettingsController::subtitleRenderPgs() const {
  return m_subtitlePreferences.renderPgs;
}

bool SettingsController::subtitleAlwaysBurnIn() const {
  return m_subtitlePreferences.alwaysBurnInWhenTranscoding;
}

QString SettingsController::subtitleStyling() const {
  return m_subtitlePreferences.styling;
}

QString SettingsController::subtitleTextSize() const {
  return m_subtitlePreferences.textSize;
}

QString SettingsController::subtitleTextWeight() const {
  return m_subtitlePreferences.textWeight;
}

QString SettingsController::subtitleFont() const {
  return m_subtitlePreferences.font;
}

QString SettingsController::subtitleTextColor() const {
  return m_subtitlePreferences.textColor;
}

QString SettingsController::subtitleDropShadow() const {
  return m_subtitlePreferences.dropShadow;
}

int SettingsController::subtitleVerticalPosition() const {
  return m_subtitlePreferences.verticalPosition;
}

QString SettingsController::redButtonAction() const {
  return m_redButtonAction;
}

QString SettingsController::greenButtonAction() const {
  return m_greenButtonAction;
}

QString SettingsController::yellowButtonAction() const {
  return m_yellowButtonAction;
}

QString SettingsController::blueButtonAction() const {
  return m_blueButtonAction;
}

QStringList SettingsController::availableButtonActions() const {
  return {
      QStringLiteral("none"),          QStringLiteral("togglePause"),
      QStringLiteral("toggleSubs"),    QStringLiteral("cycleSubs"),
      QStringLiteral("cycleAudio"),    QStringLiteral("skipBack10"),
      QStringLiteral("skipForward10"), QStringLiteral("skipBack30"),
      QStringLiteral("skipForward30"), QStringLiteral("skipBack90"),
      QStringLiteral("skipForward90"), QStringLiteral("skipBackAndEnableSubs"),
      QStringLiteral("skipSegment"),   QStringLiteral("showInfo"),
      QStringLiteral("stop"),
  };
}

QString SettingsController::buttonActionLabel(const QString &action) const {
  if (action == QStringLiteral("none"))
    return QStringLiteral("No action");
  if (action == QStringLiteral("togglePause"))
    return QStringLiteral("Play / Pause");
  if (action == QStringLiteral("toggleSubs"))
    return QStringLiteral("Toggle subtitles");
  if (action == QStringLiteral("cycleSubs"))
    return QStringLiteral("Cycle subtitles");
  if (action == QStringLiteral("cycleAudio"))
    return QStringLiteral("Cycle audio track");
  if (action == QStringLiteral("skipBack10"))
    return QStringLiteral("Skip back 10 s");
  if (action == QStringLiteral("skipForward10"))
    return QStringLiteral("Skip forward 10 s");
  if (action == QStringLiteral("skipBack30"))
    return QStringLiteral("Skip back 30 s");
  if (action == QStringLiteral("skipForward30"))
    return QStringLiteral("Skip forward 30 s");
  if (action == QStringLiteral("skipBack90"))
    return QStringLiteral("Skip back 90 s");
  if (action == QStringLiteral("skipForward90"))
    return QStringLiteral("Skip forward 90 s");
  if (action == QStringLiteral("skipBackAndEnableSubs"))
    return QStringLiteral("Skip back 10 s + enable subs");
  if (action == QStringLiteral("skipSegment"))
    return QStringLiteral("Skip intro / outro");
  if (action == QStringLiteral("showInfo"))
    return QStringLiteral("Show info");
  if (action == QStringLiteral("stop"))
    return QStringLiteral("Stop playback");
  return action;
}

void SettingsController::loadLocal() {
  m_nightModeEnabled = m_database->loadNightModeEnabled();
  m_toneMappingVisualizationEnabled = loadBoolSetting(
      m_database, QStringLiteral("settings/toneMappingVisualization"), false);
  m_audioDelayMs = m_database->loadAudioDelayMs();
  const QString storedAudioOutputMode = m_database->loadAudioOutputMode();
  m_audioOutputMode = normalizedAudioOutputMode(storedAudioOutputMode);
  if (m_audioOutputMode != storedAudioOutputMode)
    m_database->saveAudioOutputMode(m_audioOutputMode);
  m_redButtonAction = m_database->loadSetting(QStringLiteral("input/redButton"),
                                              QStringLiteral("none"));
  m_greenButtonAction =
      m_database->loadSetting(QStringLiteral("input/greenButton"),
                              QStringLiteral("skipBackAndEnableSubs"));
  m_yellowButtonAction = m_database->loadSetting(
      QStringLiteral("input/yellowButton"), QStringLiteral("none"));
  m_blueButtonAction = m_database->loadSetting(
      QStringLiteral("input/blueButton"), QStringLiteral("none"));
  loadSubtitlePreferences();

  m_player->setNightModeEnabled(m_nightModeEnabled);
  m_player->setToneMappingVisualizationEnabled(
      m_toneMappingVisualizationEnabled);
  m_player->setAudioDelayMs(m_audioDelayMs);
  m_player->setAudioOutputMode(m_audioOutputMode);
  applySubtitlePreferencesToPlayer();

  emit nightModeChanged();
  emit toneMappingVisualizationChanged();
  emit audioDelayChanged();
  emit audioOutputModeChanged();
  emit subtitleSettingsChanged();
  emit buttonRemapChanged();
}

void SettingsController::loadRemote() {
  if (!m_api || m_api->session().accessToken.isEmpty())
    return;

  Async::runScoped(
      this, m_api->fetchCultures(),
      [this](const QJsonArray &cultures) {
        QStringList codes{QString()};
        QStringList labels{QStringLiteral("Any language")};
        QSet<QString> seen{QString()};

        for (const QJsonValue &value : cultures) {
          const QJsonObject culture = value.toObject();
          const QString code =
              culture.value(QStringLiteral("ThreeLetterISOLanguageName"))
                  .toString();
          if (code.isEmpty() || seen.contains(code))
            continue;
          QString label =
              culture.value(QStringLiteral("DisplayName")).toString();
          if (label.isEmpty())
            label = code.toUpper();
          seen.insert(code);
          codes.push_back(code);
          labels.push_back(label);
        }

        if (!m_subtitlePreferences.language.isEmpty() &&
            !seen.contains(m_subtitlePreferences.language)) {
          codes.push_back(m_subtitlePreferences.language);
          labels.push_back(m_subtitlePreferences.language.toUpper());
        }

        m_subtitleLanguageCodes = codes;
        m_subtitleLanguageLabels = labels;
        emit subtitleSettingsChanged();
      },
      [](const std::exception_ptr &error) {
        qWarning() << "subtitles: culture list failed"
                   << exceptionMessage(error);
      });

  Async::runScoped(
      this, m_api->fetchUserConfiguration(),
      [this](const QJsonObject &configuration) {
        m_userConfiguration = configuration;
        m_subtitlePreferences.language =
            configuration.value(QStringLiteral("SubtitleLanguagePreference"))
                .toString();
        m_subtitlePreferences.mode = normalizedSubtitleMode(
            configuration.value(QStringLiteral("SubtitleMode"))
                .toString(QStringLiteral("Default")));
        saveSubtitlePreferences();
        applySubtitlePreferencesToPlayer();
        emit subtitleSettingsChanged();
      },
      [](const std::exception_ptr &error) {
        qWarning() << "subtitles: user configuration failed"
                   << exceptionMessage(error);
      });
}

void SettingsController::clearRemote() { m_userConfiguration = {}; }

void SettingsController::open() {
  if (m_visible)
    return;
  m_visible = true;
  emit visibleChanged();
}

void SettingsController::close() {
  if (!m_visible)
    return;
  m_visible = false;
  emit visibleChanged();
}

void SettingsController::toggleNightMode() {
  setNightModeEnabled(!m_nightModeEnabled);
}

void SettingsController::setNightModeEnabled(bool enabled) {
  if (m_nightModeEnabled == enabled)
    return;
  m_nightModeEnabled = enabled;
  m_database->saveNightModeEnabled(enabled);
  m_player->setNightModeEnabled(enabled);
  emit nightModeChanged();
}

void SettingsController::setToneMappingVisualizationEnabled(bool enabled) {
  if (m_toneMappingVisualizationEnabled == enabled)
    return;
  m_toneMappingVisualizationEnabled = enabled;
  m_database->saveSetting(QStringLiteral("settings/toneMappingVisualization"),
                          enabled ? QStringLiteral("true")
                                  : QStringLiteral("false"));
  m_player->setToneMappingVisualizationEnabled(enabled);
  emit toneMappingVisualizationChanged();
}

void SettingsController::setAudioDelayMs(int delayMs) {
  const int clampedDelayMs = std::clamp(delayMs, -2000, 2000);
  if (m_audioDelayMs == clampedDelayMs) {
    qInfo() << "app: audio delay unchanged" << clampedDelayMs << "ms";
    return;
  }
  qInfo() << "app: audio delay changed" << m_audioDelayMs << "->"
          << clampedDelayMs << "ms";
  m_audioDelayMs = clampedDelayMs;
  m_database->saveAudioDelayMs(clampedDelayMs);
  m_player->setAudioDelayMs(clampedDelayMs);
  emit audioDelayChanged();
}

void SettingsController::setAudioOutputMode(const QString &mode) {
  const QString normalized = normalizedAudioOutputMode(mode);
  if (m_audioOutputMode == normalized)
    return;
  qInfo() << "app: audio output mode changed" << m_audioOutputMode << "->"
          << normalized;
  m_audioOutputMode = normalized;
  m_database->saveAudioOutputMode(normalized);
  m_player->setAudioOutputMode(normalized);
  emit audioOutputModeChanged();
}

void SettingsController::setSubtitleLanguageIndex(int index) {
  if (index < 0 || index >= m_subtitleLanguageCodes.size())
    return;
  const QString language = m_subtitleLanguageCodes.at(index);
  if (m_subtitlePreferences.language == language)
    return;
  m_subtitlePreferences.language = language;
  saveSubtitlePreferences();
  saveSubtitleUserConfiguration();
  applySubtitlePreferencesToPlayer();
  emit subtitleSettingsChanged();
}

void SettingsController::setSubtitleMode(const QString &mode) {
  const QString normalized = normalizedSubtitleMode(mode);
  if (m_subtitlePreferences.mode == normalized)
    return;
  m_subtitlePreferences.mode = normalized;
  saveSubtitlePreferences();
  saveSubtitleUserConfiguration();
  applySubtitlePreferencesToPlayer();
  emit subtitleSettingsChanged();
}

void SettingsController::setSubtitleBurnIn(const QString &mode) {
  const QString normalized = normalizedSubtitleBurnIn(mode);
  if (m_subtitlePreferences.burnInMode == normalized)
    return;
  m_subtitlePreferences.burnInMode = normalized;
  saveSubtitlePreferences();
  emit subtitleSettingsChanged();
}

void SettingsController::setSubtitleRenderPgs(bool enabled) {
  if (m_subtitlePreferences.renderPgs == enabled)
    return;
  m_subtitlePreferences.renderPgs = enabled;
  saveSubtitlePreferences();
  emit subtitleSettingsChanged();
}

void SettingsController::setSubtitleAlwaysBurnIn(bool enabled) {
  if (m_subtitlePreferences.alwaysBurnInWhenTranscoding == enabled)
    return;
  m_subtitlePreferences.alwaysBurnInWhenTranscoding = enabled;
  saveSubtitlePreferences();
  emit subtitleSettingsChanged();
}

void SettingsController::setSubtitleStyling(const QString &styling) {
  const QString normalized = normalizedSubtitleStyling(styling);
  if (m_subtitlePreferences.styling == normalized)
    return;
  m_subtitlePreferences.styling = normalized;
  saveSubtitlePreferences();
  applySubtitlePreferencesToPlayer();
  emit subtitleSettingsChanged();
}

void SettingsController::setSubtitleTextSize(const QString &size) {
  const QString normalized = normalizedSubtitleTextSize(size);
  if (m_subtitlePreferences.textSize == normalized)
    return;
  m_subtitlePreferences.textSize = normalized;
  saveSubtitlePreferences();
  applySubtitlePreferencesToPlayer();
  emit subtitleSettingsChanged();
}

void SettingsController::setSubtitleTextWeight(const QString &weight) {
  const QString normalized = normalizedSubtitleTextWeight(weight);
  if (m_subtitlePreferences.textWeight == normalized)
    return;
  m_subtitlePreferences.textWeight = normalized;
  saveSubtitlePreferences();
  applySubtitlePreferencesToPlayer();
  emit subtitleSettingsChanged();
}

void SettingsController::setSubtitleFont(const QString &font) {
  const QString normalized = normalizedSubtitleFont(font);
  if (m_subtitlePreferences.font == normalized)
    return;
  m_subtitlePreferences.font = normalized;
  saveSubtitlePreferences();
  applySubtitlePreferencesToPlayer();
  emit subtitleSettingsChanged();
}

void SettingsController::setSubtitleTextColor(const QString &color) {
  const QString normalized = normalizedSubtitleColor(color);
  if (m_subtitlePreferences.textColor == normalized)
    return;
  m_subtitlePreferences.textColor = normalized;
  saveSubtitlePreferences();
  applySubtitlePreferencesToPlayer();
  emit subtitleSettingsChanged();
}

void SettingsController::setSubtitleDropShadow(const QString &shadow) {
  const QString normalized = normalizedSubtitleDropShadow(shadow);
  if (m_subtitlePreferences.dropShadow == normalized)
    return;
  m_subtitlePreferences.dropShadow = normalized;
  saveSubtitlePreferences();
  applySubtitlePreferencesToPlayer();
  emit subtitleSettingsChanged();
}

void SettingsController::setSubtitleVerticalPosition(int position) {
  const int clamped = std::clamp(position, -16, 16);
  if (m_subtitlePreferences.verticalPosition == clamped)
    return;
  m_subtitlePreferences.verticalPosition = clamped;
  saveSubtitlePreferences();
  applySubtitlePreferencesToPlayer();
  emit subtitleSettingsChanged();
}

void SettingsController::setRedButtonAction(const QString &action) {
  if (m_redButtonAction == action)
    return;
  m_redButtonAction = action;
  m_database->saveSetting(QStringLiteral("input/redButton"), action);
  emit buttonRemapChanged();
}

void SettingsController::setGreenButtonAction(const QString &action) {
  if (m_greenButtonAction == action)
    return;
  m_greenButtonAction = action;
  m_database->saveSetting(QStringLiteral("input/greenButton"), action);
  emit buttonRemapChanged();
}

void SettingsController::setYellowButtonAction(const QString &action) {
  if (m_yellowButtonAction == action)
    return;
  m_yellowButtonAction = action;
  m_database->saveSetting(QStringLiteral("input/yellowButton"), action);
  emit buttonRemapChanged();
}

void SettingsController::setBlueButtonAction(const QString &action) {
  if (m_blueButtonAction == action)
    return;
  m_blueButtonAction = action;
  m_database->saveSetting(QStringLiteral("input/blueButton"), action);
  emit buttonRemapChanged();
}

void SettingsController::loadSubtitlePreferences() {
  SubtitlePreferences preferences;
  preferences.language =
      m_database->loadSetting(QStringLiteral("subtitles/language"), QString());
  preferences.mode = normalizedSubtitleMode(m_database->loadSetting(
      QStringLiteral("subtitles/mode"), QStringLiteral("Default")));
  preferences.burnInMode = normalizedSubtitleBurnIn(
      m_database->loadSetting(QStringLiteral("subtitles/burnIn"), QString()));
  preferences.renderPgs =
      loadBoolSetting(m_database, QStringLiteral("subtitles/renderPgs"), false);
  preferences.alwaysBurnInWhenTranscoding = loadBoolSetting(
      m_database, QStringLiteral("subtitles/alwaysBurnInWhenTranscoding"),
      false);
  preferences.styling = normalizedSubtitleStyling(m_database->loadSetting(
      QStringLiteral("subtitles/styling"), QStringLiteral("Auto")));
  preferences.textSize = normalizedSubtitleTextSize(
      m_database->loadSetting(QStringLiteral("subtitles/textSize"), QString()));
  preferences.textWeight = normalizedSubtitleTextWeight(m_database->loadSetting(
      QStringLiteral("subtitles/textWeight"), QStringLiteral("normal")));
  preferences.font = normalizedSubtitleFont(
      m_database->loadSetting(QStringLiteral("subtitles/font"), QString()));
  preferences.textColor = normalizedSubtitleColor(m_database->loadSetting(
      QStringLiteral("subtitles/textColor"), QStringLiteral("#ffffff")));
  preferences.dropShadow = normalizedSubtitleDropShadow(m_database->loadSetting(
      QStringLiteral("subtitles/dropShadow"), QString()));
  preferences.textBackground =
      m_database->loadSetting(QStringLiteral("subtitles/textBackground"),
                              QStringLiteral("transparent"));
  preferences.verticalPosition =
      std::clamp(m_database
                     ->loadSetting(QStringLiteral("subtitles/verticalPosition"),
                                   QStringLiteral("-3"))
                     .toInt(),
                 -16, 16);
  m_subtitlePreferences = preferences;
}

void SettingsController::saveSubtitlePreferences() {
  m_database->saveSetting(QStringLiteral("subtitles/language"),
                          m_subtitlePreferences.language);
  m_database->saveSetting(QStringLiteral("subtitles/mode"),
                          m_subtitlePreferences.mode);
  m_database->saveSetting(QStringLiteral("subtitles/burnIn"),
                          m_subtitlePreferences.burnInMode);
  m_database->saveSetting(QStringLiteral("subtitles/renderPgs"),
                          m_subtitlePreferences.renderPgs
                              ? QStringLiteral("true")
                              : QStringLiteral("false"));
  m_database->saveSetting(
      QStringLiteral("subtitles/alwaysBurnInWhenTranscoding"),
      m_subtitlePreferences.alwaysBurnInWhenTranscoding
          ? QStringLiteral("true")
          : QStringLiteral("false"));
  m_database->saveSetting(QStringLiteral("subtitles/styling"),
                          m_subtitlePreferences.styling);
  m_database->saveSetting(QStringLiteral("subtitles/textSize"),
                          m_subtitlePreferences.textSize);
  m_database->saveSetting(QStringLiteral("subtitles/textWeight"),
                          m_subtitlePreferences.textWeight);
  m_database->saveSetting(QStringLiteral("subtitles/font"),
                          m_subtitlePreferences.font);
  m_database->saveSetting(QStringLiteral("subtitles/textColor"),
                          m_subtitlePreferences.textColor);
  m_database->saveSetting(QStringLiteral("subtitles/dropShadow"),
                          m_subtitlePreferences.dropShadow);
  m_database->saveSetting(QStringLiteral("subtitles/textBackground"),
                          m_subtitlePreferences.textBackground);
  m_database->saveSetting(
      QStringLiteral("subtitles/verticalPosition"),
      QString::number(m_subtitlePreferences.verticalPosition));
}

void SettingsController::saveSubtitleUserConfiguration() {
  if (!m_api || m_api->session().accessToken.isEmpty())
    return;

  QJsonObject configuration = m_userConfiguration;
  configuration.insert(QStringLiteral("SubtitleLanguagePreference"),
                       m_subtitlePreferences.language);
  configuration.insert(QStringLiteral("SubtitleMode"),
                       m_subtitlePreferences.mode);
  m_userConfiguration = configuration;

  Async::runScoped(
      this, m_api->updateUserConfiguration(configuration), []() {},
      [this](const std::exception_ptr &error) {
        emit errorOccurred(exceptionMessage(error));
      });
}

void SettingsController::applySubtitlePreferencesToPlayer() {
  if (m_player)
    m_player->setSubtitlePreferences(m_subtitlePreferences);
}

} // namespace JellyfinNative
