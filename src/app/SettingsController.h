#pragma once

#include "../common/JellyfinTypes.h"

#include <QJsonObject>
#include <QObject>
#include <QStringList>

namespace JellyfinNative {

class DatabaseManager;
class JellyfinApiFacade;
class PlayerController;

class SettingsController final : public QObject {
  Q_OBJECT

public:
  SettingsController(DatabaseManager *database, JellyfinApiFacade *api,
                     PlayerController *player, QObject *parent = nullptr);

  bool visible() const;
  bool nightModeEnabled() const;
  bool toneMappingVisualizationEnabled() const;
  int audioDelayMs() const;
  QString audioOutputMode() const;
  QStringList subtitleLanguageOptions() const;
  int subtitleLanguageIndex() const;
  QString subtitleMode() const;
  QString subtitleBurnIn() const;
  bool subtitleRenderPgs() const;
  bool subtitleAlwaysBurnIn() const;
  QString subtitleStyling() const;
  QString subtitleTextSize() const;
  QString subtitleTextWeight() const;
  QString subtitleFont() const;
  QString subtitleTextColor() const;
  QString subtitleDropShadow() const;
  int subtitleVerticalPosition() const;
  QString redButtonAction() const;
  QString greenButtonAction() const;
  QString yellowButtonAction() const;
  QString blueButtonAction() const;
  QStringList availableButtonActions() const;
  QString buttonActionLabel(const QString &action) const;

  void loadLocal();
  void loadRemote();
  void clearRemote();
  void open();
  void close();
  void toggleNightMode();
  void setNightModeEnabled(bool enabled);
  void setToneMappingVisualizationEnabled(bool enabled);
  void setAudioDelayMs(int delayMs);
  void setAudioOutputMode(const QString &mode);
  void setSubtitleLanguageIndex(int index);
  void setSubtitleMode(const QString &mode);
  void setSubtitleBurnIn(const QString &mode);
  void setSubtitleRenderPgs(bool enabled);
  void setSubtitleAlwaysBurnIn(bool enabled);
  void setSubtitleStyling(const QString &styling);
  void setSubtitleTextSize(const QString &size);
  void setSubtitleTextWeight(const QString &weight);
  void setSubtitleFont(const QString &font);
  void setSubtitleTextColor(const QString &color);
  void setSubtitleDropShadow(const QString &shadow);
  void setSubtitleVerticalPosition(int position);
  void setRedButtonAction(const QString &action);
  void setGreenButtonAction(const QString &action);
  void setYellowButtonAction(const QString &action);
  void setBlueButtonAction(const QString &action);

signals:
  void visibleChanged();
  void nightModeChanged();
  void toneMappingVisualizationChanged();
  void audioDelayChanged();
  void audioOutputModeChanged();
  void subtitleSettingsChanged();
  void buttonRemapChanged();
  void errorOccurred(const QString &message);

private:
  void loadSubtitlePreferences();
  void saveSubtitlePreferences();
  void saveSubtitleUserConfiguration();
  void applySubtitlePreferencesToPlayer();

  DatabaseManager *m_database = nullptr;
  JellyfinApiFacade *m_api = nullptr;
  PlayerController *m_player = nullptr;
  bool m_visible = false;
  bool m_nightModeEnabled = false;
  bool m_toneMappingVisualizationEnabled = false;
  int m_audioDelayMs = 0;
  QString m_audioOutputMode = QStringLiteral("alsa");
  SubtitlePreferences m_subtitlePreferences;
  QStringList m_subtitleLanguageCodes{QString()};
  QStringList m_subtitleLanguageLabels{QStringLiteral("Any language")};
  QJsonObject m_userConfiguration;
  QString m_redButtonAction = QStringLiteral("none");
  QString m_greenButtonAction = QStringLiteral("skipBackAndEnableSubs");
  QString m_yellowButtonAction = QStringLiteral("none");
  QString m_blueButtonAction = QStringLiteral("none");
};

} // namespace JellyfinNative
